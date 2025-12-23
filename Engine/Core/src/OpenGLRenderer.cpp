#include "engine/OpenGLRenderer.h"
#include <SDL.h>
#include <iostream>

#ifdef _WIN32
#define APIENTRY __stdcall
#endif

// Minimal GL API declarations (avoids including gl.h)
using PFNGLVIEWPORTPROC = void (APIENTRY*)(int, int, int, int);
using PFNGLCLEARCOLORPROC = void (APIENTRY*)(float, float, float, float);
using PFNGLENABLEPROC = void (APIENTRY*)(unsigned int);
using PFNGLCLEARPROC = void (APIENTRY*)(unsigned int);

static PFNGLVIEWPORTPROC pglViewport = nullptr;
static PFNGLCLEARCOLORPROC pglClearColor = nullptr;
static PFNGLENABLEPROC pglEnable = nullptr;
static PFNGLCLEARPROC pglClear = nullptr;

static bool ResolveGL(void** fnPtr, const char* name) {
    if (*fnPtr) return true;
    auto addr = (void*)SDL_GL_GetProcAddress(name);
    if (!addr) return false;
    *fnPtr = addr;
    return true;
}

// Needed GL constants
#define GL_DEPTH_TEST        0x0B71
#define GL_COLOR_BUFFER_BIT  0x00004000
#define GL_DEPTH_BUFFER_BIT  0x00000100

namespace Genesis::Engine {

bool OpenGLRenderer::Init(SDL_Window* window, SDL_GLContext glContext) {
    if (!window || !glContext) {
        std::cerr << "OpenGLRenderer: invalid window or GL context" << std::endl;
        return false;
    }

    m_window = window;
    m_context = glContext;

    // Make sure the GL context is current on this thread
    if (SDL_GL_MakeCurrent(m_window, m_context) != 0) {
        std::cerr << "SDL_GL_MakeCurrent failed: " << SDL_GetError() << std::endl;
        return false;
    }

    // Resolve core GL functions used
    ResolveGL((void**)&pglViewport, "glViewport");
    ResolveGL((void**)&pglClearColor, "glClearColor");
    ResolveGL((void**)&pglEnable, "glEnable");
    ResolveGL((void**)&pglClear, "glClear");

    // Basic GL init
    if (pglViewport) pglViewport(0, 0, 1280, 720);
    if (pglClearColor) pglClearColor(0.1f, 0.12f, 0.15f, 1.0f);
    if (pglEnable) pglEnable(GL_DEPTH_TEST);

    // Build a simple default shader (fallback embedded sources)
    const std::string defaultVert = R"(
        #version 120
        attribute vec3 aPos;
        attribute vec3 aNormal;
        varying vec3 vNormal;
        void main() {
            vNormal = aNormal;
            gl_Position = gl_ModelViewProjectionMatrix * vec4(aPos, 1.0);
        }
    )";

    const std::string defaultFrag = R"(
        #version 120
        varying vec3 vNormal;
        void main() {
            vec3 n = normalize(vNormal);
            float light = max(dot(n, vec3(0.0,0.0,1.0)), 0.0);
            gl_FragColor = vec4(vec3(0.6,0.7,1.0) * light, 1.0);
        }
    )";

    auto s = Shader::FromSource(defaultVert, defaultFrag);
    if (!s) {
        std::cerr << "Warning: default shader failed to compile; falling back to fixed-function pipeline" << std::endl;
    } else {
        m_defaultShader = std::make_shared<Shader>(std::move(*s));
    }

    return true;
}

void OpenGLRenderer::BeginFrame() {
    if (SDL_GL_MakeCurrent(m_window, m_context) != 0) {
        std::cerr << "SDL_GL_MakeCurrent failed in BeginFrame: " << SDL_GetError() << std::endl;
    }
    if (pglClear) pglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Ensure the default shader is active where available
    if (m_defaultShader) m_defaultShader->Use();
}

void OpenGLRenderer::EndFrame() {
    SDL_GL_SwapWindow(m_window);
}

void OpenGLRenderer::Shutdown() {
    // Nothing platform-specific here; Window owns the GL context
    m_window = nullptr;
    m_context = nullptr;
}

} // namespace Genesis::Engine
