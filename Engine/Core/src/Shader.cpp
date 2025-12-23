#include "engine/Shader.h"
#include <SDL3/SDL.h>

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

std::optional<Shader> Shader::FromSource(const std::string& vertexSrc, const std::string& fragmentSrc) {
    Shader s;
    unsigned int vs = CompileShader(GL_VERTEX_SHADER, vertexSrc);
    if (!vs) return std::nullopt;
    unsigned int fs = CompileShader(GL_FRAGMENT_SHADER, fragmentSrc);
    if (!fs) { Resolve((void**)&pglDeleteShader, "glDeleteShader"); if (pglDeleteShader) pglDeleteShader(vs); return std::nullopt; }

    Resolve((void**)&pglCreateProgram, "glCreateProgram");
    Resolve((void**)&pglAttachShader, "glAttachShader");
    Resolve((void**)&pglBindAttribLocation, "glBindAttribLocation");
    Resolve((void**)&pglLinkProgram, "glLinkProgram");
    Resolve((void**)&pglGetProgramiv, "glGetProgramiv");
    Resolve((void**)&pglGetProgramInfoLog, "glGetProgramInfoLog");
    Resolve((void**)&pglDetachShader, "glDetachShader");
    Resolve((void**)&pglDeleteProgram, "glDeleteProgram");
    Resolve((void**)&pglDeleteShader, "glDeleteShader");
    if (!pglCreateProgram || !pglAttachShader || !pglBindAttribLocation || !pglLinkProgram || !pglGetProgramiv || !pglGetProgramInfoLog || !pglDetachShader || !pglDeleteProgram || !pglDeleteShader) {
        std::cerr << "GL program functions not available" << std::endl;
        return std::nullopt;
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
        std::cerr << "Shader link error: " << message << std::endl;
        pglDeleteProgram(program);
        pglDeleteShader(vs);
        pglDeleteShader(fs);
        return std::nullopt;
    }

    pglDetachShader(program, vs);
    pglDetachShader(program, fs);
    pglDeleteShader(vs);
    pglDeleteShader(fs);

    s.programID_ = program;
    return s;
}

Shader::~Shader() {
    if (programID_) { Resolve((void**)&pglDeleteProgram, "glDeleteProgram"); if (pglDeleteProgram) pglDeleteProgram(programID_); }
}

void Shader::Use() const {
    if (programID_) { Resolve((void**)&pglUseProgram, "glUseProgram"); if (pglUseProgram) pglUseProgram(programID_); }
}

} // namespace Genesis::Engine
