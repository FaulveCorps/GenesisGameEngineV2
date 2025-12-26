#include "engine/OpenGLRenderer.h"
#include "engine/Texture.h"
#include <SDL.h>
#include <iostream>
#include <cmath>


#ifdef _WIN32
#define APIENTRY __stdcall
#endif

// Minimal GL API declarations (avoids including gl.h)
using PFNGLVIEWPORTPROC = void (APIENTRY*)(int, int, int, int);
using PFNGLCLEARCOLORPROC = void (APIENTRY*)(float, float, float, float);
using PFNGLENABLEPROC = void (APIENTRY*)(unsigned int);
using PFNGLCLEARPROC = void (APIENTRY*)(unsigned int);
using PFNGLBLENDFUNCPROC = void (APIENTRY*)(unsigned int, unsigned int);

static PFNGLVIEWPORTPROC pglViewport = nullptr;
static PFNGLCLEARCOLORPROC pglClearColor = nullptr;
static PFNGLENABLEPROC pglEnable = nullptr;
static PFNGLCLEARPROC pglClear = nullptr;
static PFNGLBLENDFUNCPROC pglBlendFunc = nullptr;

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
#define GL_BLEND             0x0BE2
#define GL_SRC_ALPHA         0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303

namespace Genesis::Engine {

bool OpenGLRenderer::Init(SDL_Window* window, SDL_GLContext glContext) {
    std::cout << "OpenGLRenderer::Init -> enter" << std::endl;
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
    std::cout << "OpenGLRenderer::Init -> SDL_GL_MakeCurrent succeeded" << std::endl;

    // Resolve core GL functions used
    ResolveGL((void**)&pglViewport, "glViewport");
    ResolveGL((void**)&pglClearColor, "glClearColor");
    ResolveGL((void**)&pglEnable, "glEnable");
    ResolveGL((void**)&pglClear, "glClear");

    std::cout << "OpenGLRenderer::Init -> GL func ptrs: pglViewport=" << (void*)pglViewport << " pglClearColor=" << (void*)pglClearColor << " pglEnable=" << (void*)pglEnable << " pglClear=" << (void*)pglClear << std::endl;

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
    if (pglEnable) { pglEnable(GL_DEPTH_TEST); std::cout << "OpenGLRenderer::Init -> enabled depth test" << std::endl; }
    ResolveGL((void**)&pglBlendFunc, "glBlendFunc");
    std::cout << "OpenGLRenderer::Init -> pglBlendFunc=" << (void*)pglBlendFunc << std::endl;
    if (pglBlendFunc) {
        pglEnable(GL_BLEND);
        pglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    {
        auto addr = (void*)SDL_GL_GetProcAddress("glGetError");
        if (addr) {
            using PFNGLGETERRORPROC = unsigned int (APIENTRY*)();
            PFNGLGETERRORPROC pglGetError = (PFNGLGETERRORPROC)addr;
            unsigned int err = pglGetError();
            if (err != 0) std::cerr << "GL error after glEnable: 0x" << std::hex << err << std::dec << std::endl;
        }
    }

    std::cout << "OpenGLRenderer::Init -> about to create default shader" << std::endl;

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
    std::cout << "OpenGLRenderer::Init -> Shader::FromSource returned program id=" << (s ? s->GetID() : 0) << std::endl;
    if (!s) {
        std::cerr << "Warning: default shader failed to compile; falling back to fixed-function pipeline" << std::endl;
    } else {
        m_defaultShader = s;
        std::cout << "OpenGLRenderer::Init -> default shader set" << std::endl;
    }

    // Create a simple debug triangle VAO/VBO so we can always draw something for troubleshooting
    {
        std::cout << "OpenGLRenderer::Init -> entering debug triangle creation" << std::endl;
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

    // Sprite shader & quad setup (2D immediate mode)
    const std::string spriteVert = R"(
        #version 330 core
        layout(location = 0) in vec2 aPos;
        layout(location = 1) in vec2 aUV;
        out vec2 vUV;
        uniform vec2 uScreen;
        void main() {
            vec2 ndc = (aPos / uScreen) * 2.0 - vec2(1.0, 1.0);
            ndc.y = -ndc.y;
            gl_Position = vec4(ndc, 0.0, 1.0);
            vUV = aUV;
        }
    )";

    const std::string spriteFrag = R"(
        #version 330 core
        in vec2 vUV;
        out vec4 FragColor;
        uniform sampler2D uTex;
        uniform vec4 uColor;
        void main() {
            vec4 c = texture(uTex, vUV) * uColor;
            FragColor = c;
        }
    )";

    m_spriteShader = Shader::FromSource(spriteVert, spriteFrag);
    if (!m_spriteShader || m_spriteShader->GetID() == 0) {
        std::cerr << "OpenGLRenderer: sprite shader failed to compile" << std::endl;
    } else {
        // Create quad VAO/VBO/EBO for streaming sprite draws
        using PFNGLGENVERTEXARRAYSPROC = void (APIENTRY*)(int, unsigned int*);
        using PFNGLBINDVERTEXARRAYPROC = void (APIENTRY*)(unsigned int);
        using PFNGLGENBUFFERSPROC = void (APIENTRY*)(int, unsigned int*);
        using PFNGLBINDBUFFERPROC = void (APIENTRY*)(unsigned int, unsigned int);
        using PFNGLBUFFERDATAPROC = void (APIENTRY*)(unsigned int, ptrdiff_t, const void*, unsigned int);
        using PFNGLENABLEVERTEXATTRIBARRAYPROC = void (APIENTRY*)(unsigned int);
        using PFNGLVERTEXATTRIBPOINTERPROC = void (APIENTRY*)(unsigned int, int, unsigned int, unsigned char, int, const void*);

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

            pglGenVertexArrays(1, &m_spriteVAO);
            pglBindVertexArray(m_spriteVAO);

            pglGenBuffers(1, &m_spriteVBO);
            const unsigned int GL_ARRAY_BUFFER = 0x8892;
            const unsigned int GL_DYNAMIC_DRAW = 0x88E8;
            pglBindBuffer(GL_ARRAY_BUFFER, m_spriteVBO);
            // Allocate space for 4 vertices (pos.xy, uv.xy)
            pglBufferData(GL_ARRAY_BUFFER, sizeof(float) * 4 * 4, nullptr, GL_DYNAMIC_DRAW);

            pglGenBuffers(1, &m_spriteEBO);
            const unsigned int GL_ELEMENT_ARRAY_BUFFER = 0x8893;
            const unsigned int GL_STATIC_DRAW = 0x88E4;
            pglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_spriteEBO);
            const unsigned int indices[] = {0,1,2, 2,3,0};
            pglBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

            pglEnableVertexAttribArray(0);
            const unsigned int GL_FLOAT = 0x1406;
            pglVertexAttribPointer(0, 2, GL_FLOAT, 0, sizeof(float) * 4, (const void*)0);
            pglEnableVertexAttribArray(1);
            pglVertexAttribPointer(1, 2, GL_FLOAT, 0, sizeof(float) * 4, (const void*)(sizeof(float) * 2));

            pglBindVertexArray(0);
            std::cout << "OpenGLRenderer: created sprite VAO=" << m_spriteVAO << " VBO=" << m_spriteVBO << " EBO=" << m_spriteEBO << std::endl;
        } else {
            std::cerr << "OpenGLRenderer: sprite quad - GL functions not available" << std::endl;
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
    #if 0
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
                    if (tmpVBO) { std::cout<<"OpenGLRenderer: deleting tmpVBO="<<tmpVBO<<std::endl; auto addrDelBuf=(void*)SDL_GL_GetProcAddress("glDeleteBuffers"); if (addrDelBuf) { using PFNGLDELETEBUFFERSPROC=void(APIENTRY*)(int,const unsigned int*); PFNGLDELETEBUFFERSPROC pglDeleteBuffers=(PFNGLDELETEBUFFERSPROC)addrDelBuf; pglDeleteBuffers(1,&tmpVBO);} }
                    if (tmpEBO) { std::cout<<"OpenGLRenderer: deleting tmpEBO="<<tmpEBO<<std::endl; auto addrDelBuf=(void*)SDL_GL_GetProcAddress("glDeleteBuffers"); if (addrDelBuf) { using PFNGLDELETEBUFFERSPROC=void(APIENTRY*)(int,const unsigned int*); PFNGLDELETEBUFFERSPROC pglDeleteBuffers=(PFNGLDELETEBUFFERSPROC)addrDelBuf; pglDeleteBuffers(1,&tmpEBO);} }
                    if (tmpVAO) { std::cout<<"OpenGLRenderer: deleting tmpVAO="<<tmpVAO<<std::endl; auto addrDelVAO=(void*)SDL_GL_GetProcAddress("glDeleteVertexArrays"); if (addrDelVAO) { using PFNGLDELETEVERTEXARRAYSPROC=void(APIENTRY*)(int,const unsigned int*); PFNGLDELETEVERTEXARRAYSPROC pglDeleteVertexArrays=(PFNGLDELETEVERTEXARRAYSPROC)addrDelVAO; pglDeleteVertexArrays(1,&tmpVAO);} }
                }
            }
        }
    }
    #endif
    }

    // Sprite draw function
    // Note: this is immediate-mode; it expects a current GL context and that BeginFrame made the context current
    void OpenGLRenderer::DrawTexture(Texture* tex, float x, float y, float w, float h,
                     float u0, float v0, float u1, float v1,
                     uint32_t color) {
        if (!tex) return;
        if (!m_spriteShader || m_spriteVAO == 0) {
            std::cerr << "OpenGLRenderer::DrawTexture -> sprite shader or buffers not initialized" << std::endl;
            return;
        }

        if (SDL_GL_MakeCurrent(m_window, m_context) != 0) {
            std::cerr << "OpenGLRenderer::DrawTexture -> SDL_GL_MakeCurrent failed: " << SDL_GetError() << std::endl;
            return;
        }

        // Ensure the texture has a GL id (let the renderer create the handle)
        tex->UploadToRenderer(this);
        unsigned int texId = tex->GetID();
        if (!texId) {
            std::cerr << "OpenGLRenderer::DrawTexture -> texture has no GL id" << std::endl;
            return;
        }

        // Activate sprite shader program
        if (m_spriteShader) m_spriteShader->Use();

        // Vertex layout: x,y,u,v (4 verts)
        float verts[16] = {
            x,     y,     u0, v0,
            x + w, y,     u1, v0,
            x + w, y + h, u1, v1,
            x,     y + h, u0, v1
        };

        // Resolve required GL functions
        auto addrGetUniformLoc = (void*)SDL_GL_GetProcAddress("glGetUniformLocation");
        auto addrUniform2f = (void*)SDL_GL_GetProcAddress("glUniform2f");
        auto addrUniform4f = (void*)SDL_GL_GetProcAddress("glUniform4f");
        auto addrUniform1i = (void*)SDL_GL_GetProcAddress("glUniform1i");
        auto addrActiveTexture = (void*)SDL_GL_GetProcAddress("glActiveTexture");
        auto addrBindTexture = (void*)SDL_GL_GetProcAddress("glBindTexture");
        auto addrBindVAO = (void*)SDL_GL_GetProcAddress("glBindVertexArray");
        auto addrBindBuf = (void*)SDL_GL_GetProcAddress("glBindBuffer");
        auto addrBufferSubData = (void*)SDL_GL_GetProcAddress("glBufferSubData");
        auto addrDrawElements = (void*)SDL_GL_GetProcAddress("glDrawElements");

        if (!addrGetUniformLoc || !addrUniform2f || !addrUniform4f || !addrUniform1i || !addrActiveTexture || !addrBindTexture || !addrBindVAO || !addrBindBuf || !addrBufferSubData || !addrDrawElements) {
            std::cerr << "OpenGLRenderer::DrawTexture -> GL functions missing for draw" << std::endl;
            return;
        }

        using PFNGLGETUNIFORMLOCATIONPROC = int (APIENTRY*)(unsigned int, const char*);
        using PFNGLUNIFORM2FPROC = void (APIENTRY*)(int, float, float);
        using PFNGLUNIFORM4FPROC = void (APIENTRY*)(int, float, float, float, float);
        using PFNGLUNIFORM1IPROC = void (APIENTRY*)(int, int);
        using PFNGLACTIVETEXTUREPROC = void (APIENTRY*)(unsigned int);
        using PFNGLBINDVERTEXARRAYPROC = void (APIENTRY*)(unsigned int);
        using PFNGLBINDBUFFERPROC = void (APIENTRY*)(unsigned int, unsigned int);
        using PFNGLBUFFERSUBDATAPROC = void (APIENTRY*)(unsigned int, ptrdiff_t, ptrdiff_t, const void*);
        using PFNGLDRAWELEMENTSPROC = void (APIENTRY*)(unsigned int, int, unsigned int, const void*);
        using PFNGLBINDTEXTUREPROC = void (APIENTRY*)(unsigned int, unsigned int);

        auto pglGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)addrGetUniformLoc;
        auto pglUniform2f = (PFNGLUNIFORM2FPROC)addrUniform2f;
        auto pglUniform4f = (PFNGLUNIFORM4FPROC)addrUniform4f;
        auto pglUniform1i = (PFNGLUNIFORM1IPROC)addrUniform1i;
        auto pglActiveTexture = (PFNGLACTIVETEXTUREPROC)addrActiveTexture;
        auto pglBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)addrBindVAO;
        auto pglBindBuffer = (PFNGLBINDBUFFERPROC)addrBindBuf;
        auto pglBufferSubData = (PFNGLBUFFERSUBDATAPROC)addrBufferSubData;
        auto pglDrawElements = (PFNGLDRAWELEMENTSPROC)addrDrawElements;
        auto pglBindTexture = (PFNGLBINDTEXTUREPROC)addrBindTexture;

        int wWin=0,hWin=0; SDL_GetWindowSize(m_window,&wWin,&hWin);
        int locScreen = pglGetUniformLocation(m_spriteShader->GetID(), "uScreen");
        if (locScreen >= 0) pglUniform2f(locScreen, (float)wWin, (float)hWin);

        // Interpret color as 0xAARRGGBB
        float a = ((color >> 24) & 0xFF) / 255.0f;
        float r = ((color >> 16) & 0xFF) / 255.0f;
        float g = ((color >> 8) & 0xFF) / 255.0f;
        float b = ((color >> 0) & 0xFF) / 255.0f;
        int locColor = pglGetUniformLocation(m_spriteShader->GetID(), "uColor");
        if (locColor >= 0) pglUniform4f(locColor, r, g, b, a);

        int locTex = pglGetUniformLocation(m_spriteShader->GetID(), "uTex");
        if (locTex >= 0) pglUniform1i(locTex, 0);

        const unsigned int GL_TEXTURE0 = 0x84C0;
        const unsigned int GL_TEXTURE_2D = 0x0DE1;
        pglActiveTexture(GL_TEXTURE0);
        pglBindTexture(GL_TEXTURE_2D, texId);

        const unsigned int GL_ARRAY_BUFFER = 0x8892;
        pglBindVertexArray(m_spriteVAO);
        pglBindBuffer(GL_ARRAY_BUFFER, m_spriteVBO);
        pglBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

        pglDrawElements(0x0004, 6, 0x1405, (const void*)0);

        pglBindVertexArray(0);
        pglBindTexture(GL_TEXTURE_2D, 0);
    }


void OpenGLRenderer::EndFrame() {
    std::cout << "OpenGLRenderer::EndFrame -> enter" << std::endl;
    // Swap buffers
    SDL_GL_SwapWindow(m_window);

    // Check GL error after swap (if available)
    auto addrGetError = (void*)SDL_GL_GetProcAddress("glGetError");
    if (addrGetError) {
        using PFNGLGETERRORPROC = unsigned int (APIENTRY*)();
        PFNGLGETERRORPROC pglGetError=(PFNGLGETERRORPROC)addrGetError;
        unsigned int e = pglGetError();
        if (e != 0) std::cerr << "OpenGLRenderer::EndFrame -> GL error after SwapWindow: 0x" << std::hex << e << std::dec << std::endl;
        else std::cout << "OpenGLRenderer::EndFrame -> no GL error after SwapWindow" << std::endl;
    }
    std::cout << "OpenGLRenderer::EndFrame -> exit" << std::endl;
}

void OpenGLRenderer::Shutdown() {
    std::cout << "OpenGLRenderer::Shutdown -> enter" << std::endl;
    // Delete sprite GL resources if created
    if (m_spriteVBO || m_spriteEBO) {
        auto addrDelBuf = (void*)SDL_GL_GetProcAddress("glDeleteBuffers");
        if (addrDelBuf) {
            using PFNGLDELETEBUFFERSPROC = void (APIENTRY*)(int, const unsigned int*);
            PFNGLDELETEBUFFERSPROC pglDeleteBuffers = (PFNGLDELETEBUFFERSPROC)addrDelBuf;
            if (m_spriteVBO) { pglDeleteBuffers(1, &m_spriteVBO); std::cout << "OpenGLRenderer: deleted sprite VBO="<<m_spriteVBO<<std::endl; m_spriteVBO = 0; }
            if (m_spriteEBO) { pglDeleteBuffers(1, &m_spriteEBO); std::cout << "OpenGLRenderer: deleted sprite EBO="<<m_spriteEBO<<std::endl; m_spriteEBO = 0; }
        }
    }
    if (m_spriteVAO) {
        auto addrDelVAO = (void*)SDL_GL_GetProcAddress("glDeleteVertexArrays");
        if (addrDelVAO) {
            using PFNGLDELETEVERTEXARRAYSPROC = void (APIENTRY*)(int, const unsigned int*);
            PFNGLDELETEVERTEXARRAYSPROC pglDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)addrDelVAO;
            pglDeleteVertexArrays(1, &m_spriteVAO);
            std::cout << "OpenGLRenderer: deleted sprite VAO="<<m_spriteVAO<<std::endl;
            m_spriteVAO = 0;
        }
    }

    // Nothing platform-specific here; Window owns the GL context
    m_window = nullptr;
    m_context = nullptr;
    std::cout << "OpenGLRenderer::Shutdown -> exit" << std::endl;
}

// Renderer-managed texture lifecycle
IGraphicsAPI::TextureHandle OpenGLRenderer::CreateTexture(uint32_t width, uint32_t height, const uint8_t* pixels) {
    IGraphicsAPI::TextureHandle h;
    if (!m_window || !m_context) {
        std::cerr << "OpenGLRenderer::CreateTexture -> no window/context" << std::endl;
        return h;
    }

    if (SDL_GL_MakeCurrent(m_window, m_context) != 0) {
        std::cerr << "OpenGLRenderer::CreateTexture -> SDL_GL_MakeCurrent failed: " << SDL_GetError() << std::endl;
        return h;
    }

    // Resolve required GL functions
    auto addrGen = (void*)SDL_GL_GetProcAddress("glGenTextures");
    auto addrBind = (void*)SDL_GL_GetProcAddress("glBindTexture");
    auto addrTexParam = (void*)SDL_GL_GetProcAddress("glTexParameteri");
    auto addrTexImage = (void*)SDL_GL_GetProcAddress("glTexImage2D");

    if (!addrGen || !addrBind || !addrTexParam || !addrTexImage) {
        std::cerr << "OpenGLRenderer::CreateTexture -> GL texture functions not available" << std::endl;
        return h;
    }

#ifdef _WIN32
#define APIENTRY __stdcall
#endif
    using PFNGLGENTEXTURESPROC = void (APIENTRY*)(int, unsigned int*);
    using PFNGLBINDTEXTUREPROC = void (APIENTRY*)(unsigned int, unsigned int);
    using PFNGLTEXPARAMETERIPROC = void (APIENTRY*)(unsigned int, int, int);
    using PFNGLTEXIMAGE2DPROC = void (APIENTRY*)(unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void*);

    auto pglGenTextures = (PFNGLGENTEXTURESPROC)addrGen;
    auto pglBindTexture = (PFNGLBINDTEXTUREPROC)addrBind;
    auto pglTexParameteri = (PFNGLTEXPARAMETERIPROC)addrTexParam;
    auto pglTexImage2D = (PFNGLTEXIMAGE2DPROC)addrTexImage;

    const unsigned int GL_TEXTURE_2D = 0x0DE1;
    const unsigned int GL_RGBA = 0x1908;
    const unsigned int GL_UNSIGNED_BYTE = 0x1401;
    const unsigned int GL_NEAREST = 0x2600;
    const unsigned int GL_TEXTURE_MIN_FILTER = 0x2801;
    const unsigned int GL_TEXTURE_MAG_FILTER = 0x2800;

    unsigned int id = 0;
    pglGenTextures(1, &id);
    pglBindTexture(GL_TEXTURE_2D, id);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    if (pixels && width > 0 && height > 0) {
        pglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (int)width, (int)height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    }

    h.id = id;
    std::cout << "OpenGLRenderer::CreateTexture -> created GL texture " << id << " (" << width << "x" << height << ")" << std::endl;
    return h;
}

void OpenGLRenderer::DestroyTexture(const IGraphicsAPI::TextureHandle& h) {
    if (!h.IsValid()) return;
    if (!m_window || !m_context) {
        std::cerr << "OpenGLRenderer::DestroyTexture -> no window/context" << std::endl;
        return;
    }
    if (SDL_GL_MakeCurrent(m_window, m_context) != 0) {
        std::cerr << "OpenGLRenderer::DestroyTexture -> SDL_GL_MakeCurrent failed: " << SDL_GetError() << std::endl;
        return;
    }
    auto addrDel = (void*)SDL_GL_GetProcAddress("glDeleteTextures");
    if (!addrDel) {
        std::cerr << "OpenGLRenderer::DestroyTexture -> glDeleteTextures not available" << std::endl;
        return;
    }
#ifdef _WIN32
#define APIENTRY __stdcall
#endif
    using PFNGLDELETETEXTURESPROC = void (APIENTRY*)(int, const unsigned int*);
    auto pglDeleteTextures = (PFNGLDELETETEXTURESPROC)addrDel;
    unsigned int id = static_cast<unsigned int>(h.id);
    pglDeleteTextures(1, &id);
    std::cout << "OpenGLRenderer::DestroyTexture -> deleted GL texture " << id << std::endl;
}

} // namespace Genesis::Engine
