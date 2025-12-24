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

                // Additional diagnostic: create an EBO-backed triangle VAO and draw with glDrawElements
                auto addrGenVAO2 = (void*)SDL_GL_GetProcAddress("glGenVertexArrays");
                auto addrBindVAO2 = (void*)SDL_GL_GetProcAddress("glBindVertexArray");
                auto addrGenBuf2 = (void*)SDL_GL_GetProcAddress("glGenBuffers");
                auto addrBindBuf2 = (void*)SDL_GL_GetProcAddress("glBindBuffer");
                auto addrBufData2 = (void*)SDL_GL_GetProcAddress("glBufferData");
                auto addrEnableAttr2 = (void*)SDL_GL_GetProcAddress("glEnableVertexAttribArray");
                auto addrAttribPtr2 = (void*)SDL_GL_GetProcAddress("glVertexAttribPointer");
                auto addrDrawElements2 = (void*)SDL_GL_GetProcAddress("glDrawElements");
                if (addrGenVAO2 && addrBindVAO2 && addrGenBuf2 && addrBindBuf2 && addrBufData2 && addrEnableAttr2 && addrAttribPtr2 && addrDrawElements2) {
                    using PFNGLGENVERTEXARRAYSPROC = void (APIENTRY*)(int, unsigned int*);
                    using PFNGLBINDVERTEXARRAYPROC = void (APIENTRY*)(unsigned int);
                    using PFNGLGENBUFFERSPROC = void (APIENTRY*)(int, unsigned int*);
                    using PFNGLBINDBUFFERPROC = void (APIENTRY*)(unsigned int, unsigned int);
                    using PFNGLBUFFERDATAPROC = void (APIENTRY*)(unsigned int, ptrdiff_t, const void*, unsigned int);
                    using PFNGLENABLEVERTEXATTRIBARRAYPROC = void (APIENTRY*)(unsigned int);
                    using PFNGLVERTEXATTRIBPOINTERPROC = void (APIENTRY*)(unsigned int, int, unsigned int, unsigned char, int, const void*);
                    using PFNGLDRAWELEMENTSPROC = void (APIENTRY*)(unsigned int, int, unsigned int, const void*);

                    auto pglGenVertexArrays2 = (PFNGLGENVERTEXARRAYSPROC)addrGenVAO2;
                    auto pglBindVertexArray2 = (PFNGLBINDVERTEXARRAYPROC)addrBindVAO2;
                    auto pglGenBuffers2 = (PFNGLGENBUFFERSPROC)addrGenBuf2;
                    auto pglBindBuffer2 = (PFNGLBINDBUFFERPROC)addrBindBuf2;
                    auto pglBufferData2 = (PFNGLBUFFERDATAPROC)addrBufData2;
                    auto pglEnableVertexAttribArray2 = (PFNGLENABLEVERTEXATTRIBARRAYPROC)addrEnableAttr2;
                    auto pglVertexAttribPointer2 = (PFNGLVERTEXATTRIBPOINTERPROC)addrAttribPtr2;
                    auto pglDrawElements2 = (PFNGLDRAWELEMENTSPROC)addrDrawElements2;

                    unsigned int tmpVAO=0, tmpVBO=0, tmpEBO=0;
                    static const float triVerts2[] = { 0.0f,0.8f,0.0f, -0.8f,-0.8f,0.0f, 0.8f,-0.8f,0.0f };
                    static const unsigned int triIdx[] = {0,1,2};
                    pglGenVertexArrays2(1,&tmpVAO);
                    pglBindVertexArray2(tmpVAO);
                    pglGenBuffers2(1,&tmpVBO);
                    const unsigned int GL_ARRAY_BUFFER = 0x8892; const unsigned int GL_ELEMENT_ARRAY_BUFFER = 0x8893; const unsigned int GL_STATIC_DRAW = 0x88E4; const unsigned int GL_FLOAT = 0x1406; const unsigned int GL_UNSIGNED_INT = 0x1405;
                    pglBindBuffer2(GL_ARRAY_BUFFER, tmpVBO);
                    pglBufferData2(GL_ARRAY_BUFFER, sizeof(triVerts2), triVerts2, GL_STATIC_DRAW);
                    pglGenBuffers2(1,&tmpEBO);
                    pglBindBuffer2(GL_ELEMENT_ARRAY_BUFFER, tmpEBO);
                    pglBufferData2(GL_ELEMENT_ARRAY_BUFFER, sizeof(triIdx), triIdx, GL_STATIC_DRAW);
                    pglEnableVertexAttribArray2(0);
                    pglVertexAttribPointer2(0,3,GL_FLOAT,0,0,(const void*)0);

                    // Query bindings
                    auto addrGetIntegerv = (void*)SDL_GL_GetProcAddress("glGetIntegerv");
                    if (addrGetIntegerv) {
                        using PFNGLGETINTEGERVPROC = void (APIENTRY*)(unsigned int, int*);
                        PFNGLGETINTEGERVPROC pglGetIntegerv = (PFNGLGETINTEGERVPROC)addrGetIntegerv;
                        int boundArray=0,boundElem=0; const unsigned int GL_ARRAY_BUFFER_BINDING=0x8894; const unsigned int GL_ELEMENT_ARRAY_BUFFER_BINDING=0x8895; pglGetIntegerv(GL_ARRAY_BUFFER_BINDING,&boundArray); pglGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING,&boundElem); std::cout<<"Diag EBO test -> GL_ARRAY_BUFFER_BINDING="<<boundArray<<" GL_ELEMENT_ARRAY_BUFFER_BINDING="<<boundElem<<std::endl;
                    }

                    pglDrawElements2(0x0004, 3, GL_UNSIGNED_INT, nullptr);
                    auto addrGetError = (void*)SDL_GL_GetProcAddress("glGetError");
                    if (addrGetError) { using PFNGLGETERRORPROC = unsigned int (APIENTRY*)(); PFNGLGETERRORPROC pglGetError=(PFNGLGETERRORPROC)addrGetError; unsigned int e=pglGetError(); if (e!=0) std::cerr<<"Diag EBO test -> GL error after glDrawElements (UINT): 0x"<<std::hex<<e<<std::dec<<std::endl; else std::cout<<"Diag EBO test -> draw (UINT) succeeded"<<std::endl; }

                    // Try with unsigned short indices
                    {
                        static const unsigned short triIdxS[] = {0,1,2};
                        pglBufferData2(GL_ELEMENT_ARRAY_BUFFER, sizeof(triIdxS), triIdxS, GL_STATIC_DRAW);
                        pglDrawElements2(0x0004, 3, 0x1403 /*GL_UNSIGNED_SHORT*/, nullptr);
                        if (addrGetError) {
                            using PFNGLGETERRORPROC = unsigned int (APIENTRY*)(); PFNGLGETERRORPROC pglGetError=(PFNGLGETERRORPROC)addrGetError; unsigned int e2=pglGetError(); if (e2!=0) std::cerr<<"Diag EBO test -> GL error after glDrawElements (USHORT): 0x"<<std::hex<<e2<<std::dec<<std::endl; else std::cout<<"Diag EBO test -> draw (USHORT) succeeded"<<std::endl; }
                    }

                    // Cleanup
                    pglBindVertexArray2(0);
                    if (tmpVBO) { auto addrDelBuf=(void*)SDL_GL_GetProcAddress("glDeleteBuffers"); if (addrDelBuf) { using PFNGLDELETEBUFFERSPROC=void(APIENTRY*)(int,const unsigned int*); PFNGLDELETEBUFFERSPROC pglDeleteBuffers=(PFNGLDELETEBUFFERSPROC)addrDelBuf; pglDeleteBuffers(1,&tmpVBO);} }
                    if (tmpEBO) { auto addrDelBuf=(void*)SDL_GL_GetProcAddress("glDeleteBuffers"); if (addrDelBuf) { using PFNGLDELETEBUFFERSPROC=void(APIENTRY*)(int,const unsigned int*); PFNGLDELETEBUFFERSPROC pglDeleteBuffers=(PFNGLDELETEBUFFERSPROC)addrDelBuf; pglDeleteBuffers(1,&tmpEBO);} }
                    if (tmpVAO) { auto addrDelVAO=(void*)SDL_GL_GetProcAddress("glDeleteVertexArrays"); if (addrDelVAO) { using PFNGLDELETEVERTEXARRAYSPROC=void(APIENTRY*)(int,const unsigned int*); PFNGLDELETEVERTEXARRAYSPROC pglDeleteVertexArrays=(PFNGLDELETEVERTEXARRAYSPROC)addrDelVAO; pglDeleteVertexArrays(1,&tmpVAO);} }
                }
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
