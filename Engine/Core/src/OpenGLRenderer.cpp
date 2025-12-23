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
    // Check errors
    {
        auto addr = (void*)SDL_GL_GetProcAddress("glGetError");
        if (addr) {
            using PFNGLGETERRORPROC = unsigned int (APIENTRY*)();
            PFNGLGETERRORPROC pglGetError = (PFNGLGETERRORPROC)addr;
            unsigned int err = pglGetError();
            if (err != 0) std::cerr << "GL error after glViewport: 0x" << std::hex << err << std::dec << std::endl;
        }
    }
    if (pglClearColor) pglClearColor(0.1f, 0.12f, 0.15f, 1.0f);
    {
        auto addr = (void*)SDL_GL_GetProcAddress("glGetError");
        if (addr) {
            using PFNGLGETERRORPROC = unsigned int (APIENTRY*)();
            PFNGLGETERRORPROC pglGetError = (PFNGLGETERRORPROC)addr;
            unsigned int err = pglGetError();
            if (err != 0) std::cerr << "GL error after glClearColor: 0x" << std::hex << err << std::dec << std::endl;
        }
    }
    if (pglEnable) pglEnable(GL_DEPTH_TEST);
    {
        auto addr = (void*)SDL_GL_GetProcAddress("glGetError");
        if (addr) {
            using PFNGLGETERRORPROC = unsigned int (APIENTRY*)();
            PFNGLGETERRORPROC pglGetError = (PFNGLGETERRORPROC)addr;
            unsigned int err = pglGetError();
            if (err != 0) std::cerr << "GL error after glEnable: 0x" << std::hex << err << std::dec << std::endl;
        }
    }

    // Build a simple default shader (fallback embedded sources)
    const std::string defaultVert = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec3 aNormal;
        out vec3 vNormal;
        void main() {
            vNormal = aNormal;
            gl_Position = vec4(aPos, 1.0);
        }
    )";

    const std::string defaultFrag = R"(
        #version 330 core
        in vec3 vNormal;
        out vec4 FragColor;
        void main() {
            FragColor = vec4(1.0, 1.0, 1.0, 1.0);
        }
    )";

    auto s = Shader::FromSource(defaultVert, defaultFrag);
    if (!s) {
        std::cerr << "Warning: default shader failed to compile; falling back to fixed-function pipeline" << std::endl;
    } else {
        m_defaultShader = std::make_shared<Shader>(std::move(*s));
    }

    // Create a simple debug triangle VAO/VBO so we can always draw something for troubleshooting
    {
        // Vertex positions (NDC)
        static const float triVerts[] = {
            0.0f,  0.8f, 0.0f,
           -0.8f, -0.8f, 0.0f,
            0.8f, -0.8f, 0.0f
        };
        using PFNGLGENVERTEXARRAYSPROC = void (APIENTRY*)(int, unsigned int*);
        using PFNGLBINDVERTEXARRAYPROC = void (APIENTRY*)(unsigned int);
        using PFNGLGENBUFFERSPROC = void (APIENTRY*)(int, unsigned int*);
        using PFNGLBINDBUFFERPROC = void (APIENTRY*)(unsigned int, unsigned int);
        using PFNGLBUFFERDATAPROC = void (APIENTRY*)(unsigned int, ptrdiff_t, const void*, unsigned int);
        using PFNGLENABLEVERTEXATTRIBARRAYPROC = void (APIENTRY*)(unsigned int);
        using PFNGLVERTEXATTRIBPOINTERPROC = void (APIENTRY*)(unsigned int, int, unsigned int, unsigned char, int, const void*);
        using PFNGLDRAWARRAYSPROC = void (APIENTRY*)(unsigned int, int, int);

        auto addrGenVAO = (void*)SDL_GL_GetProcAddress("glGenVertexArrays");
        auto addrBindVAO = (void*)SDL_GL_GetProcAddress("glBindVertexArray");
        auto addrGenBuf = (void*)SDL_GL_GetProcAddress("glGenBuffers");
        auto addrBindBuf = (void*)SDL_GL_GetProcAddress("glBindBuffer");
        auto addrBufData = (void*)SDL_GL_GetProcAddress("glBufferData");
        auto addrEnableAttr = (void*)SDL_GL_GetProcAddress("glEnableVertexAttribArray");
        auto addrAttribPtr = (void*)SDL_GL_GetProcAddress("glVertexAttribPointer");

        if (addrGenVAO && addrBindVAO && addrGenBuf && addrBindBuf && addrBufData && addrEnableAttr && addrAttribPtr) {
            auto pglGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)addrGenVAO;
            auto pglBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)addrBindVAO;
            auto pglGenBuffers = (PFNGLGENBUFFERSPROC)addrGenBuf;
            auto pglBindBuffer = (PFNGLBINDBUFFERPROC)addrBindBuf;
            auto pglBufferData = (PFNGLBUFFERDATAPROC)addrBufData;
            auto pglEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)addrEnableAttr;
            auto pglVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)addrAttribPtr;

            pglGenVertexArrays(1, &m_debugVAO);
            pglBindVertexArray(m_debugVAO);

            pglGenBuffers(1, &m_debugVBO);
            const unsigned int GL_ARRAY_BUFFER = 0x8892;
            const unsigned int GL_STATIC_DRAW = 0x88E4;
            pglBindBuffer(GL_ARRAY_BUFFER, m_debugVBO);
            pglBufferData(GL_ARRAY_BUFFER, sizeof(triVerts), triVerts, GL_STATIC_DRAW);

            pglEnableVertexAttribArray(0);
            const unsigned int GL_FLOAT = 0x1406;
            pglVertexAttribPointer(0, 3, GL_FLOAT, 0, 0, (const void*)0);

            pglBindVertexArray(0);
            std::cout << "Debug triangle VAO=" << m_debugVAO << " VBO=" << m_debugVBO << std::endl;
        } else {
            std::cerr << "Debug triangle: GL functions not available" << std::endl;
        }
    }

    // Print GL renderer info for debug
    {
        auto addrGetString = (void*)SDL_GL_GetProcAddress("glGetString");
        if (addrGetString) {
            using PFNGLGETSTRINGPROC = const unsigned char* (APIENTRY*)(unsigned int);
            PFNGLGETSTRINGPROC pglGetString = (PFNGLGETSTRINGPROC)addrGetString;
            const unsigned char* vendor = pglGetString(0x1F00);
            const unsigned char* renderer = pglGetString(0x1F01);
            const unsigned char* version = pglGetString(0x1F02);
            const unsigned char* slver = pglGetString(0x8B8C);
            std::cout << "GL_VENDOR: " << (vendor ? (const char*)vendor : "(null)") << std::endl;
            std::cout << "GL_RENDERER: " << (renderer ? (const char*)renderer : "(null)") << std::endl;
            std::cout << "GL_VERSION: " << (version ? (const char*)version : "(null)") << std::endl;
            std::cout << "GL_SHADING_LANGUAGE_VERSION: " << (slver ? (const char*)slver : "(null)") << std::endl;
        } else {
            std::cerr << "glGetString not available" << std::endl;
        }
    }

    return true;
}

void OpenGLRenderer::BeginFrame() {
    if (SDL_GL_MakeCurrent(m_window, m_context) != 0) {
        std::cerr << "SDL_GL_MakeCurrent failed in BeginFrame: " << SDL_GetError() << std::endl;
    }
    
    // Update viewport in case window was resized
    int w, h;
    SDL_GetWindowSize(m_window, &w, &h);
    if (pglViewport) pglViewport(0, 0, w, h);
    
    if (pglClear) pglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Check for GL errors after clearing
    {
        using PFNGLGETERRORPROC = unsigned int (APIENTRY*)();
        PFNGLGETERRORPROC pglGetError = nullptr;
        auto addr = (void*)SDL_GL_GetProcAddress("glGetError");
        if (addr) pglGetError = (PFNGLGETERRORPROC)addr;
        if (pglGetError) {
            unsigned int err = pglGetError();
            if (err != 0) std::cerr << "GL error after glClear: 0x" << std::hex << err << std::dec << std::endl;
        }
    }

    // Ensure the default shader is active where available
    if (m_defaultShader) m_defaultShader->Use();

    // Draw debug triangle if present
    if (m_debugVAO) {
        auto addrDrawArrays = (void*)SDL_GL_GetProcAddress("glDrawArrays");
        if (addrDrawArrays) {
            using PFNGLBINDVERTEXARRAYPROC = void (APIENTRY*)(unsigned int);
            using PFNGLDRAWARRAYSPROC = void (APIENTRY*)(unsigned int, int, int);
            auto pglBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)SDL_GL_GetProcAddress("glBindVertexArray");
            auto pglDrawArrays = (PFNGLDRAWARRAYSPROC)addrDrawArrays;
            if (pglBindVertexArray && pglDrawArrays) {
                pglBindVertexArray(m_debugVAO);
                // Debug: query attrib 0 enabled and buffer binding
                auto addrGetIntegerv = (void*)SDL_GL_GetProcAddress("glGetIntegerv");
                if (addrGetIntegerv) {
                    using PFNGLGETINTEGERVPROC = void (APIENTRY*)(unsigned int, int*);
                    PFNGLGETINTEGERVPROC pglGetIntegerv = (PFNGLGETINTEGERVPROC)addrGetIntegerv;
                    int boundArray = 0; const unsigned int GL_ARRAY_BUFFER_BINDING = 0x8894; pglGetIntegerv(GL_ARRAY_BUFFER_BINDING, &boundArray);
                    std::cout << "DebugTriangle -> GL_ARRAY_BUFFER_BINDING=" << boundArray << std::endl;
                }
                auto addrGetVertexAttrib = (void*)SDL_GL_GetProcAddress("glGetVertexAttribiv");
                if (addrGetVertexAttrib) {
                    using PFNGLGETVERTEXATTRIBIVPROC = void (APIENTRY*)(unsigned int, unsigned int, int*);
                    PFNGLGETVERTEXATTRIBIVPROC pglGetVertexAttribiv = (PFNGLGETVERTEXATTRIBIVPROC)addrGetVertexAttrib;
                    int enabled0 = 0, bufbind0 = -1; const unsigned int GL_VERTEX_ATTRIB_ARRAY_ENABLED = 0x8622; const unsigned int GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING = 0x889F;
                    pglGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled0);
                    pglGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &bufbind0);
                    std::cout << "DebugTriangle -> attrib0 enabled=" << enabled0 << " buffer_binding=" << bufbind0 << std::endl;
                }
                pglDrawArrays(0x0004 /*GL_TRIANGLES*/, 0, 3);
                pglBindVertexArray(0);
            }
        }
    }
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
