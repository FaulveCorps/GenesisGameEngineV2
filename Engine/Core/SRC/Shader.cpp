#include "engine/Shader.h"
#include "engine/ShaderRegistry.h"
#include "engine/IGraphics.h"
#include <SDL.h>
#include "engine/Engine.h"
#include "engine/IShaderSubsystem.h"

namespace Genesis::Engine {

#ifdef _WIN32
#define APIENTRY __stdcall
#endif

// Declarations for required GL shader/program functions (loaded at runtime)
using PFNGLCREATESHADERPROC = unsigned int (APIENTRY*)(unsigned int);
using PFNGLSHADERSOURCEPROC = void (APIENTRY*)(unsigned int, int, const char* const*, const int*);
using PFNGLCOMPILESHADERPROC = void (APIENTRY*)(unsigned int);
using PFNGLGETSHADERIVPROC = void (APIENTRY*)(unsigned int, unsigned int, int*);
using PFNGLGETSHADERINFOLOGPROC = void (APIENTRY*)(unsigned int, int, int*, char*);
using PFNGLDELETESHADERPROC = void (APIENTRY*)(unsigned int);
using PFNGLCREATEPROGRAMPROC = unsigned int (APIENTRY*)();
using PFNGLATTACHSHADERPROC = void (APIENTRY*)(unsigned int, unsigned int);
using PFNGLBINDATTRIBLOCATIONPROC = void (APIENTRY*)(unsigned int, unsigned int, const char*);
using PFNGLLINKPROGRAMPROC = void (APIENTRY*)(unsigned int);
using PFNGLGETPROGRAMIVPROC = void (APIENTRY*)(unsigned int, unsigned int, int*);
using PFNGLGETPROGRAMINFOLOGPROC = void (APIENTRY*)(unsigned int, int, int*, char*);
using PFNGLDETACHSHADERPROC = void (APIENTRY*)(unsigned int, unsigned int);
using PFNGLDELETEPROGRAMPROC = void (APIENTRY*)(unsigned int);
using PFNGLUSEPROGRAMPROC = void (APIENTRY*)(unsigned int);

static PFNGLCREATESHADERPROC pglCreateShader = nullptr;
static PFNGLSHADERSOURCEPROC pglShaderSource = nullptr;
static PFNGLCOMPILESHADERPROC pglCompileShader = nullptr;
static PFNGLGETSHADERIVPROC pglGetShaderiv = nullptr;
static PFNGLGETSHADERINFOLOGPROC pglGetShaderInfoLog = nullptr;
static PFNGLDELETESHADERPROC pglDeleteShader = nullptr;
static PFNGLCREATEPROGRAMPROC pglCreateProgram = nullptr;
static PFNGLATTACHSHADERPROC pglAttachShader = nullptr;
static PFNGLBINDATTRIBLOCATIONPROC pglBindAttribLocation = nullptr;
static PFNGLLINKPROGRAMPROC pglLinkProgram = nullptr;
static PFNGLGETPROGRAMIVPROC pglGetProgramiv = nullptr;
static PFNGLGETPROGRAMINFOLOGPROC pglGetProgramInfoLog = nullptr;
static PFNGLDETACHSHADERPROC pglDetachShader = nullptr;
static PFNGLDELETEPROGRAMPROC pglDeleteProgram = nullptr;
static PFNGLUSEPROGRAMPROC pglUseProgram = nullptr;

static bool Resolve(void** fnPtr, const char* name) {
    if (*fnPtr) return true;
    auto addr = (void*)SDL_GL_GetProcAddress(name);
    if (!addr) return false;
    *fnPtr = addr;
    return true;
}

// Required GL constants
#define GL_VERTEX_SHADER      0x8B31
#define GL_FRAGMENT_SHADER    0x8B30
#define GL_COMPILE_STATUS     0x8B81
#define GL_INFO_LOG_LENGTH    0x8B84
#define GL_LINK_STATUS        0x8B82
// GL_FALSE constant without including headers
#ifndef GL_FALSE
#define GL_FALSE 0
#endif

static unsigned int CompileShader(unsigned int type, const std::string& source) {
    Resolve((void**)&pglCreateShader, "glCreateShader");
    Resolve((void**)&pglShaderSource, "glShaderSource");
    Resolve((void**)&pglCompileShader, "glCompileShader");
    Resolve((void**)&pglGetShaderiv, "glGetShaderiv");
    Resolve((void**)&pglGetShaderInfoLog, "glGetShaderInfoLog");
    Resolve((void**)&pglDeleteShader, "glDeleteShader");

    if (!pglCreateShader || !pglShaderSource || !pglCompileShader || !pglGetShaderiv || !pglGetShaderInfoLog || !pglDeleteShader) {
        std::cerr << "GL shader functions not available" << std::endl;
        return 0;
    }

    unsigned int id = pglCreateShader(type);
    const char* src = source.c_str();
    pglShaderSource(id, 1, &src, nullptr);
    pglCompileShader(id);

    int result = 0;
    pglGetShaderiv(id, GL_COMPILE_STATUS, &result);
    if (result == GL_FALSE) {
        int length = 0;
        pglGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
        std::string message(length, '\0');
        pglGetShaderInfoLog(id, length, &length, &message[0]);
        std::cerr << "Shader compilation error: " << message << std::endl;
        pglDeleteShader(id);
        return 0;
    }
    return id;
}

std::shared_ptr<Shader> Shader::CreateFromSource(const std::string& vertexSrc, const std::string& fragmentSrc) {
    auto s = std::make_shared<Shader>();
    s->vertexSrcGL_ = vertexSrc;
    s->fragmentSrcGL_ = fragmentSrc;

    // Register so the registry can rebuild/destroy across switches
    ShaderRegistry::Instance().Register(s.get());
    return s;
}

std::shared_ptr<Shader> Shader::FromSource(const std::string& vertexSrc, const std::string& fragmentSrc) {
    auto s = CreateFromSource(vertexSrc, fragmentSrc);
    // Attempt immediate GL compile if possible
    s->UploadToRenderer(nullptr);
    if (s->programID_ == 0) {
        // Try to compile under current GL context; if still 0, caller should check and handle
        // Note: returning s even with programID_==0 is acceptable; it can be compiled later
    }
    return s;
}

void Shader::UploadToRenderer(IGraphicsAPI* /*renderer*/) {
    // Prefer using an installed shader subsystem when available.
    if (programID_) return; // already built
    if (vertexSrcGL_.empty() || fragmentSrcGL_.empty()) return;

    // Try subsystem first
    auto shaderSub = Genesis::Engine::GetShaderSubsystem();
    if (!shaderSub) {
        // If no subsystem is present but a GL context exists, try creating a GL subsystem on-demand
        if (SDL_GL_GetCurrentContext()) {
            if (Genesis::Engine::CreateShaderSubsystem("opengl")) {
                shaderSub = Genesis::Engine::GetShaderSubsystem();
            }
        }
    }

    if (shaderSub) {
        unsigned int pid = shaderSub->CreateProgramFromSource(vertexSrcGL_, fragmentSrcGL_);
        if (pid) {
            programID_ = pid;
            return;
        }
        // if subsystem failed, fall back to legacy GL compile path
        std::cerr << "Shader::UploadToRenderer -> subsystem failed to create program; falling back" << std::endl;
    }

    // Fallback: legacy local GL compile (as before)
    unsigned int vs = CompileShader(GL_VERTEX_SHADER, vertexSrcGL_);
    if (!vs) return;
    unsigned int fs = CompileShader(GL_FRAGMENT_SHADER, fragmentSrcGL_);
    if (!fs) { if (vs) pglDeleteShader(vs); return; }

    Resolve((void**)&pglCreateProgram, "glCreateProgram");
    Resolve((void**)&pglAttachShader, "glAttachShader");
    Resolve((void**)&pglBindAttribLocation, "glBindAttribLocation");
    Resolve((void**)&pglLinkProgram, "glLinkProgram");
    Resolve((void**)&pglGetProgramiv, "glGetProgramiv");
    Resolve((void**)&pglGetProgramInfoLog, "glGetProgramInfoLog");
    Resolve((void**)&pglDeleteProgram, "glDeleteProgram");
    Resolve((void**)&pglDeleteShader, "glDeleteShader");
    if (!pglCreateProgram || !pglAttachShader || !pglBindAttribLocation || !pglLinkProgram || !pglGetProgramiv || !pglGetProgramInfoLog || !pglDeleteProgram || !pglDeleteShader) {
        std::cerr << "GL program functions not available" << std::endl;
        if (vs) pglDeleteShader(vs);
        if (fs) pglDeleteShader(fs);
        return;
    }

    unsigned int program = pglCreateProgram();
    pglAttachShader(program, vs);
    pglAttachShader(program, fs);

    // Bind attribute locations so we know where aPos/aNormal map (0 and 1)
    pglBindAttribLocation(program, 0, "aPos");
    pglBindAttribLocation(program, 1, "aNormal");

    pglLinkProgram(program);

    int linked = 0;
    pglGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        int length = 0;
        pglGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::string message(length, '\0');
        pglGetProgramInfoLog(program, length, &length, &message[0]);
        std::cerr << "GL link error: " << message << std::endl;
        pglDeleteProgram(program);
        pglDeleteShader(vs);
        pglDeleteShader(fs);
        return;
    }

    Resolve((void**)&pglDetachShader, "glDetachShader");
    Resolve((void**)&pglDeleteShader, "glDeleteShader");
    if (pglDetachShader) {
        pglDetachShader(program, vs);
        pglDetachShader(program, fs);
    }
    if (pglDeleteShader) {
        pglDeleteShader(vs);
        pglDeleteShader(fs);
    }

    programID_ = program;
} 

void Shader::DestroyOnRenderer(IGraphicsAPI* /*renderer*/) {
    if (!programID_) return;

    // If a shader subsystem is available, ask it to destroy the program
    auto shaderSub = Genesis::Engine::GetShaderSubsystem();
    if (shaderSub) {
        shaderSub->DestroyProgram(programID_);
        programID_ = 0;
        return;
    }

    // Legacy path: Only attempt GL deletion if a GL context is current
    if (!SDL_GL_GetCurrentContext()) {
        std::cerr << "Shader::DestroyOnRenderer -> no GL context; deferring deletion of program " << programID_ << std::endl;
        return;
    }
    Resolve((void**)&pglDeleteProgram, "glDeleteProgram");
    if (pglDeleteProgram) {
        pglDeleteProgram(programID_);
    } else {
        std::cerr << "Shader::DestroyOnRenderer -> glDeleteProgram not available" << std::endl;
    }
    programID_ = 0;
}

void Shader::Use() const {
    if (programID_) {
        Resolve((void**)&pglUseProgram, "glUseProgram");
        if (pglUseProgram) {
            pglUseProgram(programID_);
        } else {
            std::cerr << "Shader::Use -> glUseProgram not available" << std::endl;
        }
    }
} 

Shader::~Shader() {
    // Ensure removal from registry and cleanup
    ShaderRegistry::Instance().Unregister(this);
    DestroyOnRenderer(nullptr);
}

} // namespace Genesis::Engine
