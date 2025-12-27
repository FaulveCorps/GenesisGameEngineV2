#include "engine/IShaderSubsystem.h"
#include "engine/SubsystemRegistry.h"
#include <SDL.h>
#include <iostream>
#include <string>

namespace Genesis::Engine {

#ifdef _WIN32
#define APIENTRY __stdcall
#endif
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

static bool Resolve(void** fnPtr, const char* name) {
    if (*fnPtr) return true;
    auto addr = (void*)SDL_GL_GetProcAddress(name);
    if (!addr) return false;
    *fnPtr = addr;
    return true;
}

// minimal GL constants
#define GL_VERTEX_SHADER      0x8B31
#define GL_FRAGMENT_SHADER    0x8B30
#define GL_COMPILE_STATUS     0x8B81
#define GL_INFO_LOG_LENGTH    0x8B84
#define GL_LINK_STATUS        0x8B82
#ifndef GL_FALSE
#define GL_FALSE 0
#endif

static unsigned int CompileShaderInternal(unsigned int type, const std::string& source) {
    Resolve((void**)&pglCreateShader, "glCreateShader");
    Resolve((void**)&pglShaderSource, "glShaderSource");
    Resolve((void**)&pglCompileShader, "glCompileShader");
    Resolve((void**)&pglGetShaderiv, "glGetShaderiv");
    Resolve((void**)&pglGetShaderInfoLog, "glGetShaderInfoLog");
    Resolve((void**)&pglDeleteShader, "glDeleteShader");

    if (!pglCreateShader || !pglShaderSource || !pglCompileShader || !pglGetShaderiv || !pglGetShaderInfoLog || !pglDeleteShader) {
        std::cerr << "GLShaderSubsystem: shader functions not available" << std::endl;
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
        std::cerr << "GL shader compilation error: " << message << std::endl;
        if (pglDeleteShader) pglDeleteShader(id);
        return 0;
    }
    return id;
}

class GLShaderSubsystem : public IShaderSubsystem {
public:
    bool Init() override {
        // Ensure there's a current GL context; otherwise Init can still succeed but CreateProgram will fail until context available
        return true;
    }
    void Update(double /*dt*/) override {}
    void Shutdown() override {}
    std::string Name() const override { return "opengl"; }

    unsigned int CreateProgramFromSource(const std::string& vertexSrc, const std::string& fragmentSrc) override {
        if (!SDL_GL_GetCurrentContext()) {
            std::cerr << "GLShader: no GL context; cannot compile shader" << std::endl;
            return 0;
        }

        unsigned int vs = CompileShaderInternal(GL_VERTEX_SHADER, vertexSrc);
        if (!vs) return 0;
        unsigned int fs = CompileShaderInternal(GL_FRAGMENT_SHADER, fragmentSrc);
        if (!fs) { if (pglDeleteShader) pglDeleteShader(vs); return 0; }

        Resolve((void**)&pglCreateProgram, "glCreateProgram");
        Resolve((void**)&pglAttachShader, "glAttachShader");
        Resolve((void**)&pglBindAttribLocation, "glBindAttribLocation");
        Resolve((void**)&pglLinkProgram, "glLinkProgram");
        Resolve((void**)&pglGetProgramiv, "glGetProgramiv");
        Resolve((void**)&pglGetProgramInfoLog, "glGetProgramInfoLog");
        Resolve((void**)&pglDetachShader, "glDetachShader");
        Resolve((void**)&pglDeleteProgram, "glDeleteProgram");
        Resolve((void**)&pglDeleteShader, "glDeleteShader");

        if (!pglCreateProgram || !pglAttachShader || !pglBindAttribLocation || !pglLinkProgram || !pglGetProgramiv || !pglGetProgramInfoLog) {
            std::cerr << "GLShader: program functions not available" << std::endl;
            if (pglDeleteShader) { pglDeleteShader(vs); pglDeleteShader(fs); }
            return 0;
        }

        unsigned int program = pglCreateProgram();
        pglAttachShader(program, vs);
        pglAttachShader(program, fs);
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
            if (pglDeleteProgram) pglDeleteProgram(program);
            if (pglDeleteShader) { pglDeleteShader(vs); pglDeleteShader(fs); }
            return 0;
        }

        if (pglDetachShader) { pglDetachShader(program, vs); pglDetachShader(program, fs); }
        if (pglDeleteShader) { pglDeleteShader(vs); pglDeleteShader(fs); }

        return program;
    }

    void DestroyProgram(unsigned int programID) override {
        if (!programID) return;
        if (!SDL_GL_GetCurrentContext()) { std::cerr << "GLShader::DestroyProgram -> no GL context; deferring delete of " << programID << std::endl; return; }
        Resolve((void**)&pglDeleteProgram, "glDeleteProgram");
        if (pglDeleteProgram) {
            pglDeleteProgram(programID);
        } else {
            std::cerr << "GLShader: glDeleteProgram not available" << std::endl;
        }
    }
};

// Register factory for opengl
static bool register_gl_shader = []() {
    SubsystemRegistry::Instance().RegisterFactory("Shader", "opengl", []() {
        return std::make_unique<GLShaderSubsystem>();
    });
    return true;
}();

// Explicit registration function in case the translation unit is not pulled by the linker
void RegisterGLShaderFactory() {
    SubsystemRegistry::Instance().RegisterFactory("Shader", "opengl", []() {
        return std::make_unique<GLShaderSubsystem>();
    });
}

} // namespace Genesis::Engine
