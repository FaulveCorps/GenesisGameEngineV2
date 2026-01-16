#include "engine/OpenGLRenderer.h"
#include "engine/MathUtils.h"
#include "engine/Texture.h"
#include "engine/Material.h"
#include <SDL.h>
#include <iostream>
#include <cmath>
#include <fstream>
#include <sstream>
#include <cstring>

#ifdef _WIN32
#define APIENTRY __stdcall
#endif

// Minimal GL API declarations (avoids including gl.h)
using PFNGLVIEWPORTPROC = void (APIENTRY*)(int, int, int, int);
using PFNGLCLEARCOLORPROC = void (APIENTRY*)(float, float, float, float);
using PFNGLENABLEPROC = void (APIENTRY*)(unsigned int);
using PFNGLCLEARPROC = void (APIENTRY*)(unsigned int);
using PFNGLBLENDFUNCPROC = void (APIENTRY*)(unsigned int, unsigned int);
using PFNGLDRAWBUFFERPROC = void (APIENTRY*)(unsigned int);

// Mesh/Shader related
using PFNGLGENVERTEXARRAYSPROC = void (APIENTRY*)(int, unsigned int*);
using PFNGLBINDVERTEXARRAYPROC = void (APIENTRY*)(unsigned int);
using PFNGLGENBUFFERSPROC = void (APIENTRY*)(int, unsigned int*);
using PFNGLBINDBUFFERPROC = void (APIENTRY*)(unsigned int, unsigned int);
using PFNGLBUFFERDATAPROC = void (APIENTRY*)(unsigned int, ptrdiff_t, const void*, unsigned int);
using PFNGLENABLEVERTEXATTRIBARRAYPROC = void (APIENTRY*)(unsigned int);
using PFNGLVERTEXATTRIBPOINTERPROC = void (APIENTRY*)(unsigned int, int, unsigned int, unsigned char, int, const void*);
using PFNGLDELETEVERTEXARRAYSPROC = void (APIENTRY*)(int, const unsigned int*);
using PFNGLDELETEBUFFERSPROC = void (APIENTRY*)(int, const unsigned int*);
using PFNGLDRAWELEMENTSPROC = void (APIENTRY*)(unsigned int, int, unsigned int, const void*);
using PFNGLUNIFORM1FPROC = void (APIENTRY*)(int, float);
using PFNGLUNIFORM3FPROC = void (APIENTRY*)(int, float, float, float);
using PFNGLUNIFORM4FPROC = void (APIENTRY*)(int, float, float, float, float);
using PFNGLUNIFORMMATRIX4FVPROC = void (APIENTRY*)(int, int, unsigned char, const float*);
using PFNGLGETUNIFORMLOCATIONPROC = int (APIENTRY*)(unsigned int, const char*);
using PFNGLUNIFORM1IPROC = void (APIENTRY*)(int, int);
using PFNGLACTIVETEXTUREPROC = void (APIENTRY*)(unsigned int);
using PFNGLBINDTEXTUREPROC = void (APIENTRY*)(unsigned int, unsigned int);

// Framebuffer object functions
using PFNGLGENFRAMEBUFFERSPROC = void (APIENTRY*)(int, unsigned int*);
using PFNGLBINDFRAMEBUFFERPROC = void (APIENTRY*)(unsigned int, unsigned int);
using PFNGLFRAMEBUFFERTEXTURE2DPROC = void (APIENTRY*)(unsigned int, unsigned int, unsigned int, unsigned int, int);
using PFNGLGENRENDERBUFFERSPROC = void (APIENTRY*)(int, unsigned int*);
using PFNGLBINDRENDERBUFFERPROC = void (APIENTRY*)(unsigned int, unsigned int);
using PFNGLRENDERBUFFERSTORAGEPROC = void (APIENTRY*)(unsigned int, unsigned int, int, int);
using PFNGLFRAMEBUFFERRENDERBUFFERPROC = void (APIENTRY*)(unsigned int, unsigned int, unsigned int, unsigned int);
using PFNGLCHECKFRAMEBUFFERSTATUSPROC = unsigned int (APIENTRY*)(unsigned int);
using PFNGLDELETEFRAMEBUFFERSPROC = void (APIENTRY*)(int, const unsigned int*);
using PFNGLDELETERENDERBUFFERSPROC = void (APIENTRY*)(int, const unsigned int*);
using PFNGLDRAWBUFFERSPROC = void (APIENTRY*)(int, const unsigned int*);
using PFNGLBLITFRAMEBUFFERPROC = void (APIENTRY*)(int, int, int, int, int, int, int, int, unsigned int, unsigned int);

// Texture functions
using PFNGLGENTEXTURESPROC = void (APIENTRY*)(int, unsigned int*);
using PFNGLDELETETEXTURESPROC = void (APIENTRY*)(int, const unsigned int*);
using PFNGLTEXIMAGE2DPROC = void (APIENTRY*)(unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void*);
using PFNGLTEXPARAMETERIPROC = void (APIENTRY*)(unsigned int, int, int);
using PFNGLTEXPARAMETERFVPROC = void (APIENTRY*)(unsigned int, unsigned int, const float*);
using PFNGLREADBUFFERPROC = void (APIENTRY*)(unsigned int);
using PFNGLREADPIXELSPROC = void (APIENTRY*)(int, int, int, int, unsigned int, unsigned int, void*);
using PFNGLDRAWARRAYSPROC = void (APIENTRY*)(unsigned int, int, int);
using PFNGLDISABLEPROC = void (APIENTRY*)(unsigned int);
using PFNGLBUFFERSUBDATAPROC = void (APIENTRY*)(unsigned int, ptrdiff_t, ptrdiff_t, const void*);
using PFNGLUNIFORM2FPROC = void (APIENTRY*)(int, float, float);

static PFNGLVIEWPORTPROC pglViewport = nullptr;
static PFNGLCLEARCOLORPROC pglClearColor = nullptr;
static PFNGLENABLEPROC pglEnable = nullptr;
static PFNGLCLEARPROC pglClear = nullptr;
static PFNGLBLENDFUNCPROC pglBlendFunc = nullptr;
static PFNGLDRAWBUFFERPROC pglDrawBuffer = nullptr;
static PFNGLREADPIXELSPROC pglReadPixels = nullptr; 

static PFNGLGENVERTEXARRAYSPROC pglGenVertexArrays = nullptr;
static PFNGLBINDVERTEXARRAYPROC pglBindVertexArray = nullptr;
static PFNGLGENBUFFERSPROC pglGenBuffers = nullptr;
static PFNGLBINDBUFFERPROC pglBindBuffer = nullptr;
static PFNGLBUFFERDATAPROC pglBufferData = nullptr;
static PFNGLENABLEVERTEXATTRIBARRAYPROC pglEnableVertexAttribArray = nullptr;
static PFNGLVERTEXATTRIBPOINTERPROC pglVertexAttribPointer = nullptr;
static PFNGLDELETEVERTEXARRAYSPROC pglDeleteVertexArrays = nullptr;
static PFNGLDELETEBUFFERSPROC pglDeleteBuffers = nullptr;
static PFNGLDRAWELEMENTSPROC pglDrawElements = nullptr;
static PFNGLUNIFORM1FPROC pglUniform1f = nullptr;
static PFNGLUNIFORM3FPROC pglUniform3f = nullptr;
static PFNGLUNIFORM4FPROC pglUniform4f = nullptr;
static PFNGLUNIFORMMATRIX4FVPROC pglUniformMatrix4fv = nullptr;
static PFNGLGETUNIFORMLOCATIONPROC pglGetUniformLocation = nullptr;
static PFNGLUNIFORM1IPROC pglUniform1i = nullptr;
static PFNGLACTIVETEXTUREPROC pglActiveTexture = nullptr;
static PFNGLBINDTEXTUREPROC pglBindTexture = nullptr;

static PFNGLGENFRAMEBUFFERSPROC pglGenFramebuffers = nullptr;
static PFNGLBINDFRAMEBUFFERPROC pglBindFramebuffer = nullptr;
static PFNGLFRAMEBUFFERTEXTURE2DPROC pglFramebufferTexture2D = nullptr;
static PFNGLGENRENDERBUFFERSPROC pglGenRenderbuffers = nullptr;
static PFNGLBINDRENDERBUFFERPROC pglBindRenderbuffer = nullptr;
static PFNGLRENDERBUFFERSTORAGEPROC pglRenderbufferStorage = nullptr;
static PFNGLFRAMEBUFFERRENDERBUFFERPROC pglFramebufferRenderbuffer = nullptr;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC pglCheckFramebufferStatus = nullptr;
static PFNGLDELETEFRAMEBUFFERSPROC pglDeleteFramebuffers = nullptr;
static PFNGLDELETERENDERBUFFERSPROC pglDeleteRenderbuffers = nullptr;
static PFNGLDRAWBUFFERSPROC pglDrawBuffers = nullptr;
static PFNGLBLITFRAMEBUFFERPROC pglBlitFramebuffer = nullptr;

static PFNGLGENTEXTURESPROC pglGenTextures = nullptr;
static PFNGLDELETETEXTURESPROC pglDeleteTextures = nullptr;
static PFNGLTEXIMAGE2DPROC pglTexImage2D = nullptr;
static PFNGLTEXPARAMETERIPROC pglTexParameteri = nullptr;
static PFNGLTEXPARAMETERFVPROC pglTexParameterfv = nullptr;
static PFNGLREADBUFFERPROC pglReadBuffer = nullptr;
static PFNGLDRAWARRAYSPROC pglDrawArrays = nullptr;
static PFNGLDISABLEPROC pglDisable = nullptr;
static PFNGLBUFFERSUBDATAPROC pglBufferSubData = nullptr;
static PFNGLUNIFORM2FPROC pglUniform2f = nullptr;

static bool ResolveGL(void** fnPtr, const char* name) {
    if (*fnPtr) return true;
    auto addr = (void*)SDL_GL_GetProcAddress(name);
    if (!addr) return false;
    *fnPtr = addr;
    return true;
}

static bool RequireGL(void** fnPtr, const char* name) {
    if (ResolveGL(fnPtr, name)) return true;
    std::cerr << "OpenGLRenderer: missing required GL function: " << name << std::endl;
    return false;
}

static void DeleteFramebuffer(unsigned int& fbo) {
    if (fbo && pglDeleteFramebuffers) {
        pglDeleteFramebuffers(1, &fbo);
        fbo = 0;
    }
}

static void DeleteFramebufferArray(unsigned int (&fbos)[2]) {
    if (pglDeleteFramebuffers && (fbos[0] || fbos[1])) {
        pglDeleteFramebuffers(2, fbos);
        fbos[0] = 0;
        fbos[1] = 0;
    }
}

static void DeleteRenderbuffer(unsigned int& rbo) {
    if (rbo && pglDeleteRenderbuffers) {
        pglDeleteRenderbuffers(1, &rbo);
        rbo = 0;
    }
}

static void DeleteTexture(unsigned int& tex) {
    if (tex && pglDeleteTextures) {
        pglDeleteTextures(1, &tex);
        tex = 0;
    }
}

static void DeleteTextureArray(unsigned int (&textures)[2]) {
    if (pglDeleteTextures && (textures[0] || textures[1])) {
        pglDeleteTextures(2, textures);
        textures[0] = 0;
        textures[1] = 0;
    }
}

static void CheckGLError(const char* label) {
    auto addr = (void*)SDL_GL_GetProcAddress("glGetError");
    if (addr) {
        using PFNGLGETERRORPROC = unsigned int (APIENTRY*)();
        PFNGLGETERRORPROC pglGetError = (PFNGLGETERRORPROC)addr;
        unsigned int err = pglGetError();
        if (err != 0) std::cerr << "GL error " << label << ": 0x" << std::hex << err << std::dec << std::endl;
    }
}

static std::string ReadFile(const std::string& path) {
    std::ifstream t(path);
    if (!t.is_open()) return "";
    std::stringstream buffer;
    buffer << t.rdbuf();
    return buffer.str();
}

// Needed GL constants
#define GL_DEPTH_TEST        0x0B71
#define GL_COLOR_BUFFER_BIT  0x00004000
#define GL_DEPTH_BUFFER_BIT  0x00000100
#define GL_BLEND             0x0BE2
#define GL_SRC_ALPHA         0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_ARRAY_BUFFER      0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STATIC_DRAW       0x88E4
#define GL_FLOAT             0x1406
#define GL_UNSIGNED_INT      0x1405
#define GL_UNSIGNED_SHORT    0x1403
#define GL_TRIANGLES         0x0004
#define GL_FALSE             0
#define GL_DYNAMIC_DRAW      0x88E8
#define GL_ARRAY_BUFFER_BINDING 0x8894
#define GL_ELEMENT_ARRAY_BUFFER_BINDING 0x8895
#define GL_VERTEX_ATTRIB_ARRAY_ENABLED 0x8622
#define GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING 0x889F
#define GL_TEXTURE0          0x84C0
#define GL_TEXTURE1          0x84C1
#define GL_TEXTURE2          0x84C2
#define GL_TEXTURE3          0x84C3
#define GL_TEXTURE_2D        0x0DE1
#define GL_RGBA              0x1908
#define GL_RGBA16F           0x881A
#define GL_DEPTH_COMPONENT   0x1902
#define GL_UNSIGNED_BYTE     0x1401
#define GL_NEAREST           0x2600
#define GL_LINEAR            0x2601
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S    0x2802
#define GL_TEXTURE_WRAP_T    0x2803
#define GL_CLAMP_TO_BORDER   0x812D
#define GL_TEXTURE_BORDER_COLOR 0x1004
#define GL_FRAMEBUFFER       0x8D40
#define GL_READ_FRAMEBUFFER  0x8CA8
#define GL_DRAW_FRAMEBUFFER  0x8CA9
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_COLOR_ATTACHMENT1 0x8CE1
#define GL_COLOR_ATTACHMENT2 0x8CE2
#define GL_DEPTH_ATTACHMENT  0x8D00
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#define GL_RENDERBUFFER      0x8D41
#define GL_DEPTH24_STENCIL8  0x88F0
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_NONE              0
#define GL_BACK              0x0405
#define GL_DEPTH_BUFFER_BIT  0x00000100
#define GL_COLOR_BUFFER_BIT  0x00004000
#define GL_TRIANGLES         0x0004
#define GL_LINES             0x0001


namespace Genesis::Engine {

bool OpenGLRenderer::Init(SDL_Window* window, SDL_GLContext glContext) {
    std::cout << "OpenGLRenderer::Init -> enter" << std::endl;
    if (!window || !glContext) {
        std::cerr << "OpenGLRenderer: invalid window or GL context" << std::endl;
        return false;
    }

    m_window = window;
    m_context = glContext;

    if (SDL_GL_GetCurrentContext() != m_context) {
        if (SDL_GL_MakeCurrent(m_window, m_context) != 0) {
            std::cerr << "SDL_GL_MakeCurrent failed: " << SDL_GetError() << std::endl;
            // return false; // Try to continue even if MakeCurrent fails, maybe it's a false positive or already active
        }
    } else {
        std::cout << "OpenGLRenderer::Init -> Context already current" << std::endl;
    }

    // Resolve core GL functions used
    bool glOk = true;
    glOk &= RequireGL((void**)&pglViewport, "glViewport");
    glOk &= RequireGL((void**)&pglClearColor, "glClearColor");
    glOk &= RequireGL((void**)&pglEnable, "glEnable");
    glOk &= RequireGL((void**)&pglClear, "glClear");
    glOk &= RequireGL((void**)&pglDrawBuffer, "glDrawBuffer");
    glOk &= RequireGL((void**)&pglBlendFunc, "glBlendFunc");

    // Resolve Mesh/Shader functions
    glOk &= RequireGL((void**)&pglGenVertexArrays, "glGenVertexArrays");
    glOk &= RequireGL((void**)&pglBindVertexArray, "glBindVertexArray");
    glOk &= RequireGL((void**)&pglGenBuffers, "glGenBuffers");
    glOk &= RequireGL((void**)&pglBindBuffer, "glBindBuffer");
    glOk &= RequireGL((void**)&pglBufferData, "glBufferData");
    glOk &= RequireGL((void**)&pglEnableVertexAttribArray, "glEnableVertexAttribArray");
    glOk &= RequireGL((void**)&pglVertexAttribPointer, "glVertexAttribPointer");
    glOk &= RequireGL((void**)&pglDeleteVertexArrays, "glDeleteVertexArrays");
    glOk &= RequireGL((void**)&pglDeleteBuffers, "glDeleteBuffers");
    glOk &= RequireGL((void**)&pglDrawElements, "glDrawElements");
    glOk &= RequireGL((void**)&pglUniform1f, "glUniform1f");
    glOk &= RequireGL((void**)&pglUniform3f, "glUniform3f");
    glOk &= RequireGL((void**)&pglUniform4f, "glUniform4f");
    glOk &= RequireGL((void**)&pglUniformMatrix4fv, "glUniformMatrix4fv");
    glOk &= RequireGL((void**)&pglGetUniformLocation, "glGetUniformLocation");
    glOk &= RequireGL((void**)&pglUniform1i, "glUniform1i");
    glOk &= RequireGL((void**)&pglActiveTexture, "glActiveTexture");
    glOk &= RequireGL((void**)&pglBindTexture, "glBindTexture");

    // Resolve Framebuffer functions
    glOk &= RequireGL((void**)&pglGenFramebuffers, "glGenFramebuffers");
    glOk &= RequireGL((void**)&pglBindFramebuffer, "glBindFramebuffer");
    glOk &= RequireGL((void**)&pglFramebufferTexture2D, "glFramebufferTexture2D");
    glOk &= RequireGL((void**)&pglGenRenderbuffers, "glGenRenderbuffers");
    glOk &= RequireGL((void**)&pglBindRenderbuffer, "glBindRenderbuffer");
    glOk &= RequireGL((void**)&pglRenderbufferStorage, "glRenderbufferStorage");
    glOk &= RequireGL((void**)&pglFramebufferRenderbuffer, "glFramebufferRenderbuffer");
    glOk &= RequireGL((void**)&pglCheckFramebufferStatus, "glCheckFramebufferStatus");
    glOk &= RequireGL((void**)&pglDeleteFramebuffers, "glDeleteFramebuffers");
    glOk &= RequireGL((void**)&pglDeleteRenderbuffers, "glDeleteRenderbuffers");
    glOk &= RequireGL((void**)&pglDrawBuffers, "glDrawBuffers");
    // Optional
    ResolveGL((void**)&pglBlitFramebuffer, "glBlitFramebuffer");

    glOk &= RequireGL((void**)&pglGenTextures, "glGenTextures");
    glOk &= RequireGL((void**)&pglDeleteTextures, "glDeleteTextures");
    glOk &= RequireGL((void**)&pglTexImage2D, "glTexImage2D");
    glOk &= RequireGL((void**)&pglTexParameteri, "glTexParameteri");
    glOk &= RequireGL((void**)&pglTexParameterfv, "glTexParameterfv");
    glOk &= RequireGL((void**)&pglReadBuffer, "glReadBuffer");
    glOk &= RequireGL((void**)&pglDrawArrays, "glDrawArrays");
    glOk &= RequireGL((void**)&pglDisable, "glDisable");
    // Optional (we have fallbacks)
    ResolveGL((void**)&pglBufferSubData, "glBufferSubData");
    ResolveGL((void**)&pglUniform2f, "glUniform2f");

    if (!glOk) {
        std::cerr << "OpenGLRenderer: init failed due to missing GL symbols" << std::endl;
        return false;
    }

    if (pglViewport) {
        int w, h;
        SDL_GetWindowSize(m_window, &w, &h);
        pglViewport(0, 0, w, h);
        InitPostProcessing(w, h);
        InitShadowMap();
        InitGBuffer(w, h);
    }

    if (pglClearColor) pglClearColor(0.1f, 0.12f, 0.15f, 1.0f);
    if (pglEnable) { pglEnable(GL_DEPTH_TEST); }
    if (pglBlendFunc) {
        pglEnable(GL_BLEND);
        pglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    // Load Deferred Shaders
    m_gBufferShader = Shader::CreateFromFile("Assets/shaders/gbuffer.vert", "Assets/shaders/gbuffer.frag");
    if (!m_gBufferShader) {
        std::cerr << "Failed to load G-Buffer shaders" << std::endl;
    }

    m_deferredLightingShader = Shader::CreateFromFile("Assets/shaders/deferred_lighting.vert", "Assets/shaders/deferred_lighting.frag");
    if (!m_deferredLightingShader) {
        std::cerr << "Failed to load Deferred Lighting shaders" << std::endl;
    }

    // Load PBR shader (fallback/forward)
    m_pbrShader = Shader::CreateFromFile("Assets/shaders/pbr.vert", "Assets/shaders/pbr.frag");

    // Load PostProcess Shaders
    m_postProcessShader = Shader::CreateFromFile("Assets/shaders/postprocess.vert", "Assets/shaders/postprocess.frag");
    m_blurShader = Shader::CreateFromFile("Assets/shaders/blur.vert", "Assets/shaders/blur.frag");

    // Sprite shader
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
    if (m_spriteShader) {
        if (pglGenVertexArrays && pglBindVertexArray && pglGenBuffers && pglBindBuffer && pglBufferData && pglEnableVertexAttribArray && pglVertexAttribPointer) {
            pglGenVertexArrays(1, &m_spriteVAO);
            pglBindVertexArray(m_spriteVAO);
            pglGenBuffers(1, &m_spriteVBO);
            pglBindBuffer(GL_ARRAY_BUFFER, m_spriteVBO);
            pglBufferData(GL_ARRAY_BUFFER, sizeof(float) * 4 * 4, nullptr, GL_DYNAMIC_DRAW);
            pglGenBuffers(1, &m_spriteEBO);
            pglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_spriteEBO);
            const unsigned int indices[] = {0,1,2, 2,3,0};
            pglBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
            pglEnableVertexAttribArray(0);
            pglVertexAttribPointer(0, 2, GL_FLOAT, 0, sizeof(float) * 4, (const void*)0);
            pglEnableVertexAttribArray(1);
            pglVertexAttribPointer(1, 2, GL_FLOAT, 0, sizeof(float) * 4, (const void*)(sizeof(float) * 2));
            pglBindVertexArray(0);
        }
    }

    // Debug Shader
    const std::string debugVert = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec3 aColor;
        out vec3 vColor;
        uniform mat4 uVP;
        void main() {
            gl_Position = uVP * vec4(aPos, 1.0);
            vColor = aColor;
        }
    )";
    const std::string debugFrag = R"(
        #version 330 core
        in vec3 vColor;
        out vec4 FragColor;
        void main() {
            FragColor = vec4(vColor, 1.0);
        }
    )";
    m_debugShader = Shader::FromSource(debugVert, debugFrag);
    if (m_debugShader) {
        if (pglGenVertexArrays && pglGenBuffers) {
             pglGenVertexArrays(1, &m_debugVAO);
             pglGenBuffers(1, &m_debugVBO);
        }
    }

    return true;
}

void OpenGLRenderer::InitGBuffer(int width, int height) {
    if (!pglGenFramebuffers) return;

    // If reinitializing, clean up old resources first.
    DeleteFramebuffer(m_gBuffer);
    DeleteTexture(m_gPosition);
    DeleteTexture(m_gNormal);
    DeleteTexture(m_gAlbedoSpec);
    DeleteRenderbuffer(m_gDepthRBO);

    pglGenFramebuffers(1, &m_gBuffer);
    pglBindFramebuffer(GL_FRAMEBUFFER, m_gBuffer);

    // Position color buffer
    pglGenTextures(1, &m_gPosition);
    pglBindTexture(GL_TEXTURE_2D, m_gPosition);
    pglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    pglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_gPosition, 0);

    // Normal color buffer
    pglGenTextures(1, &m_gNormal);
    pglBindTexture(GL_TEXTURE_2D, m_gNormal);
    pglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    pglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_gNormal, 0);

    // Color + Specular color buffer
    pglGenTextures(1, &m_gAlbedoSpec);
    pglBindTexture(GL_TEXTURE_2D, m_gAlbedoSpec);
    pglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    pglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, m_gAlbedoSpec, 0);

    unsigned int attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    pglDrawBuffers(3, attachments);

    // Create and attach depth buffer (renderbuffer)
    pglGenRenderbuffers(1, &m_gDepthRBO);
    pglBindRenderbuffer(GL_RENDERBUFFER, m_gDepthRBO);
    pglRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    pglFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_gDepthRBO);
    
    if (pglCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "Framebuffer not complete!" << std::endl;
    
    pglBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLRenderer::ResizeGBuffer(int width, int height) {
    DeleteFramebuffer(m_gBuffer);
    DeleteTexture(m_gPosition);
    DeleteTexture(m_gNormal);
    DeleteTexture(m_gAlbedoSpec);
    DeleteRenderbuffer(m_gDepthRBO);
    InitGBuffer(width, height);
}

void OpenGLRenderer::InitPostProcessing(int width, int height) {
    if (!pglGenFramebuffers) return;

    // If reinitializing, clean up old resources first.
    DeleteFramebuffer(m_fbo);
    DeleteTexture(m_screenTexture);
    DeleteTexture(m_brightTexture);
    DeleteRenderbuffer(m_rbo);
    DeleteFramebufferArray(m_pingPongFBO);
    DeleteTextureArray(m_pingPongTexture);
    DeleteFramebuffer(m_finalFBO);
    DeleteTexture(m_finalTexture);
    
    m_screenWidth = width;
    m_screenHeight = height;

    // Create FBO
    pglGenFramebuffers(1, &m_fbo);
    pglBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    // Create color attachment texture
    pglGenTextures(1, &m_screenTexture);
    pglBindTexture(GL_TEXTURE_2D, m_screenTexture);
    pglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    pglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_screenTexture, 0);

    // Create bloom bright texture
    pglGenTextures(1, &m_brightTexture);
    pglBindTexture(GL_TEXTURE_2D, m_brightTexture);
    pglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    pglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_brightTexture, 0);

    unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    pglDrawBuffers(2, attachments);

    // Create RBO for depth/stencil
    pglGenRenderbuffers(1, &m_rbo);
    pglBindRenderbuffer(GL_RENDERBUFFER, m_rbo);
    pglRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    pglFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_rbo);

    if (pglCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "Post-process Framebuffer not complete!" << std::endl;

    pglBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Ping-pong buffers for bloom
    pglGenFramebuffers(2, m_pingPongFBO);
    pglGenTextures(2, m_pingPongTexture);
    for (unsigned int i = 0; i < 2; i++) {
        pglBindFramebuffer(GL_FRAMEBUFFER, m_pingPongFBO[i]);
        pglBindTexture(GL_TEXTURE_2D, m_pingPongTexture[i]);
        pglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
        pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        pglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_pingPongTexture[i], 0);
        if (pglCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
             std::cerr << "PingPong Framebuffer not complete!" << std::endl;
    }

    // Screen quad (size-independent, create once)
    if (!m_screenQuadVAO) {
        float quadVertices[] = { 
            // positions   // texCoords
            -1.0f,  1.0f,  0.0f, 1.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,
             1.0f, -1.0f,  1.0f, 0.0f,

            -1.0f,  1.0f,  0.0f, 1.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
             1.0f,  1.0f,  1.0f, 1.0f
        };
        pglGenVertexArrays(1, &m_screenQuadVAO);
        pglGenBuffers(1, &m_screenQuadVBO);
        pglBindVertexArray(m_screenQuadVAO);
        pglBindBuffer(GL_ARRAY_BUFFER, m_screenQuadVBO);
        pglBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        pglEnableVertexAttribArray(0);
        pglVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        pglEnableVertexAttribArray(1);
        pglVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        pglBindVertexArray(0);
    }
}

void OpenGLRenderer::ResizePostProcessing(int width, int height) {
    InitPostProcessing(width, height);
}

void OpenGLRenderer::InitShadowMap() {
    if (!pglGenFramebuffers) return;

    // If reinitializing, clean up old resources first.
    DeleteFramebuffer(m_shadowMapFBO);
    DeleteTexture(m_shadowMapTexture);

    pglGenFramebuffers(1, &m_shadowMapFBO);
    
    pglGenTextures(1, &m_shadowMapTexture);
    pglBindTexture(GL_TEXTURE_2D, m_shadowMapTexture);
    pglTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    pglTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    pglBindFramebuffer(GL_FRAMEBUFFER, m_shadowMapFBO);
    pglFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_shadowMapTexture, 0);
    pglDrawBuffer(GL_NONE);
    pglReadBuffer(GL_NONE);

    // Restore default framebuffer state so later rendering/readback works.
    // (glDrawBuffer/glReadBuffer are context state; leaving them as GL_NONE can
    // cause subsequent default-FB rendering to silently discard color writes.)
    pglBindFramebuffer(GL_FRAMEBUFFER, 0);
    pglDrawBuffer(GL_BACK);
    pglReadBuffer(GL_BACK);

    // Load shadow shader
    m_shadowShader = Shader::CreateFromFile("Assets/shaders/shadow.vert", "Assets/shaders/shadow.frag");
}

void OpenGLRenderer::RenderShadowPass() {
    if (!m_shadowMapFBO || !m_shadowShader) return;

    pglViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    pglBindFramebuffer(GL_FRAMEBUFFER, m_shadowMapFBO);
    pglClear(GL_DEPTH_BUFFER_BIT);

    // Compute light space matrix
    float near_plane = 1.0f, far_plane = 50.0f;
    float orthoSize = 20.0f;
    Matrix4 lightProjection = Matrix4::CreateOrtho(-orthoSize, orthoSize, -orthoSize, orthoSize, near_plane, far_plane);

    // Normalize light dir
    float lx = m_lightDir[0], ly = m_lightDir[1], lz = m_lightDir[2];
    float len = std::sqrt(lx*lx + ly*ly + lz*lz);
    if (len > 0.0001f) { lx /= len; ly /= len; lz /= len; }

    // Position light far away looking at origin
    float dist = 25.0f;
    float eyeX = -lx * dist;
    float eyeY = -ly * dist;
    float eyeZ = -lz * dist;

    // Handle case where light is looking straight down/up
    float upX = 0.0f, upY = 1.0f, upZ = 0.0f;
    if (std::abs(ly) > 0.99f) {
        upY = 0.0f; upZ = 1.0f;
    }

    Matrix4 lightView = Matrix4::CreateLookAt(eyeX, eyeY, eyeZ, 
                                              0.0f, 0.0f, 0.0f, 
                                              upX, upY, upZ);

    Matrix4 lightSpace = lightProjection * lightView;
    std::memcpy(m_lightSpaceMatrix, lightSpace.m, sizeof(float) * 16);

    m_shadowShader->Use();
    int locLSM = pglGetUniformLocation(m_shadowShader->GetID(), "lightSpaceMatrix");
    if (locLSM >= 0) pglUniformMatrix4fv(locLSM, 1, GL_FALSE, m_lightSpaceMatrix);

    // Draw scene
    // We need to disable face culling or cull front faces for shadows to prevent peter panning
    // pglEnable(GL_CULL_FACE);
    // pglCullFace(GL_FRONT);

    for (const auto& cmd : m_drawQueue) {
        ExecuteDraw(cmd, m_shadowShader.get());
    }

    // pglCullFace(GL_BACK);
    // pglDisable(GL_CULL_FACE);

    pglBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLRenderer::BeginFrame() {
    if (SDL_GL_GetCurrentContext() != m_context) {
        if (SDL_GL_MakeCurrent(m_window, m_context) != 0) {
            std::cerr << "SDL_GL_MakeCurrent failed in BeginFrame: " << SDL_GetError() << std::endl;
        }
    }

    // Hot-reload shaders if changed (check every 60 frames to avoid IO overhead)
    static int frameCount = 0;
    frameCount++;
    if (frameCount % 60 == 0) {
        if (m_gBufferShader) m_gBufferShader->ReloadIfChanged();
        if (m_deferredLightingShader) m_deferredLightingShader->ReloadIfChanged();
        if (m_pbrShader) m_pbrShader->ReloadIfChanged();
        if (m_postProcessShader) m_postProcessShader->ReloadIfChanged();
        if (m_blurShader) m_blurShader->ReloadIfChanged();
        if (m_shadowShader) m_shadowShader->ReloadIfChanged();
    }
    
    int w, h;
    SDL_GetWindowSize(m_window, &w, &h);
    if (w != m_screenWidth || h != m_screenHeight) {
        ResizePostProcessing(w, h);
        ResizeGBuffer(w, h);
    }

    // Clear default framebuffer just in case
    BindDefaultFramebuffer();
    pglViewport(0, 0, w, h);
    pglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGLRenderer::EndFrame() {
    if (SDL_GL_GetCurrentContext() != m_context) {
        if (SDL_GL_MakeCurrent(m_window, m_context) != 0) {
            std::cerr << "OpenGLRenderer::EndFrame -> SDL_GL_MakeCurrent failed: " << SDL_GetError() << std::endl;
        }
    }

    // 1. Shadow Pass
    RenderShadowPass();

    // 2. Geometry Pass (Deferred)
    if (m_gBuffer && m_gBufferShader) {
        pglBindFramebuffer(GL_FRAMEBUFFER, m_gBuffer);
        pglViewport(0, 0, m_screenWidth, m_screenHeight);
        pglClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        pglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        m_gBufferShader->Use();
        for (const auto& cmd : m_drawQueue) {
            ExecuteDraw(cmd, m_gBufferShader.get());
        }
    }

    // 3. Lighting Pass (Deferred)
    if (m_fbo && m_deferredLightingShader) {
        pglBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        pglViewport(0, 0, m_screenWidth, m_screenHeight);
        pglClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        pglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear for fresh lighting

        m_deferredLightingShader->Use();
        
        pglActiveTexture(GL_TEXTURE0);
        pglBindTexture(GL_TEXTURE_2D, m_gPosition);
        pglActiveTexture(GL_TEXTURE1);
        pglBindTexture(GL_TEXTURE_2D, m_gNormal);
        pglActiveTexture(GL_TEXTURE2);
        pglBindTexture(GL_TEXTURE_2D, m_gAlbedoSpec);
        pglActiveTexture(GL_TEXTURE3);
        pglBindTexture(GL_TEXTURE_2D, m_shadowMapTexture);

        int locPos = pglGetUniformLocation(m_deferredLightingShader->GetID(), "gPosition");
        if (locPos >= 0) pglUniform1i(locPos, 0);
        int locNorm = pglGetUniformLocation(m_deferredLightingShader->GetID(), "gNormal");
        if (locNorm >= 0) pglUniform1i(locNorm, 1);
        int locAlb = pglGetUniformLocation(m_deferredLightingShader->GetID(), "gAlbedoSpec");
        if (locAlb >= 0) pglUniform1i(locAlb, 2);
        int locShadow = pglGetUniformLocation(m_deferredLightingShader->GetID(), "shadowMap");
        if (locShadow >= 0) pglUniform1i(locShadow, 3);

        int locLightDir = pglGetUniformLocation(m_deferredLightingShader->GetID(), "uLightDir");
        if (locLightDir >= 0) pglUniform3f(locLightDir, m_lightDir[0], m_lightDir[1], m_lightDir[2]);
        int locLightColor = pglGetUniformLocation(m_deferredLightingShader->GetID(), "uLightColor");
        if (locLightColor >= 0) pglUniform3f(locLightColor, m_lightColor[0], m_lightColor[1], m_lightColor[2]);
        int locLightInt = pglGetUniformLocation(m_deferredLightingShader->GetID(), "uLightIntensity");
        if (locLightInt >= 0) pglUniform1f(locLightInt, m_lightIntensity);

        int locNrPointLights = pglGetUniformLocation(m_deferredLightingShader->GetID(), "nrPointLights");
        if (locNrPointLights >= 0) pglUniform1i(locNrPointLights, (int)m_pointLights.size());

        for (size_t i = 0; i < m_pointLights.size() && i < 16; ++i) {
            std::string base = "pointLights[" + std::to_string(i) + "]";
            int locPos = pglGetUniformLocation(m_deferredLightingShader->GetID(), (base + ".position").c_str());
            if (locPos >= 0) pglUniform3f(locPos, m_pointLights[i].position[0], m_pointLights[i].position[1], m_pointLights[i].position[2]);
            
            int locCol = pglGetUniformLocation(m_deferredLightingShader->GetID(), (base + ".color").c_str());
            if (locCol >= 0) pglUniform3f(locCol, m_pointLights[i].color[0], m_pointLights[i].color[1], m_pointLights[i].color[2]);
            
            int locInt = pglGetUniformLocation(m_deferredLightingShader->GetID(), (base + ".intensity").c_str());
            if (locInt >= 0) pglUniform1f(locInt, m_pointLights[i].intensity);
            
            int locRad = pglGetUniformLocation(m_deferredLightingShader->GetID(), (base + ".radius").c_str());
            if (locRad >= 0) pglUniform1f(locRad, m_pointLights[i].radius);
        }

        int locViewPos = pglGetUniformLocation(m_deferredLightingShader->GetID(), "viewPos");
        if (locViewPos >= 0) pglUniform3f(locViewPos, 0.0f, 0.0f, 10.0f); 
        
        int locLSM = pglGetUniformLocation(m_deferredLightingShader->GetID(), "lightSpaceMatrix");
        if (locLSM >= 0) pglUniformMatrix4fv(locLSM, 1, GL_FALSE, m_lightSpaceMatrix);

        // Draw Quad
        pglBindVertexArray(m_screenQuadVAO);
        pglDrawArrays(GL_TRIANGLES, 0, 6);
        pglBindVertexArray(0);
    }

    // 4. Bloom Blur Pass
    bool horizontal = true, first_iteration = true;
    unsigned int amount = 10;
    if (m_bloom && m_blurShader && m_brightTexture) {
        m_blurShader->Use();
        int locHor = pglGetUniformLocation(m_blurShader->GetID(), "horizontal");
        int locImg = pglGetUniformLocation(m_blurShader->GetID(), "image");
        if (locImg >= 0) pglUniform1i(locImg, 0);

        pglBindVertexArray(m_screenQuadVAO);
        for (unsigned int i = 0; i < amount; i++)
        {
            pglBindFramebuffer(GL_FRAMEBUFFER, m_pingPongFBO[horizontal]);
            if (locHor >= 0) pglUniform1i(locHor, horizontal);
            
            pglActiveTexture(GL_TEXTURE0);
            pglBindTexture(GL_TEXTURE_2D, first_iteration ? m_brightTexture : m_pingPongTexture[!horizontal]); 
            
            pglDrawArrays(GL_TRIANGLES, 0, 6);
            
            horizontal = !horizontal;
            if (first_iteration) first_iteration = false;
        }
        pglBindVertexArray(0);
    }

    // 5. Post-Process Pass (Final Combine)
    if (m_fbo) {
        // If present is disabled (Editor Mode), render to internal texture instead of screen
        if (!m_presentEnabled) {
            if (!m_finalFBO) {
                pglGenFramebuffers(1, &m_finalFBO);
                pglGenTextures(1, &m_finalTexture);
                pglBindFramebuffer(GL_FRAMEBUFFER, m_finalFBO);
                pglBindTexture(GL_TEXTURE_2D, m_finalTexture);
                pglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_screenWidth, m_screenHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
                pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                pglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_finalTexture, 0);
            }
            pglBindFramebuffer(GL_FRAMEBUFFER, m_finalFBO);
        } else {
            pglBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        int w, h; SDL_GetWindowSize(m_window, &w, &h);
        pglViewport(0, 0, w, h);
        pglClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Clear to black instead of white
        pglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (m_postProcessShader) {
            m_postProcessShader->Use();
            
            pglActiveTexture(GL_TEXTURE0);
            pglBindTexture(GL_TEXTURE_2D, m_screenTexture);
            
            int locTex = pglGetUniformLocation(m_postProcessShader->GetID(), "uScreenTexture");
            if (locTex >= 0) pglUniform1i(locTex, 0);

            if (m_bloom) {
                pglActiveTexture(GL_TEXTURE1);
                pglBindTexture(GL_TEXTURE_2D, m_pingPongTexture[!horizontal]);
                int locBloom = pglGetUniformLocation(m_postProcessShader->GetID(), "uBloomBlur");
                if (locBloom >= 0) pglUniform1i(locBloom, 1);
            }

            int locBloomBool = pglGetUniformLocation(m_postProcessShader->GetID(), "uBloom");
            if (locBloomBool >= 0) pglUniform1i(locBloomBool, m_bloom);

            int locExp = pglGetUniformLocation(m_postProcessShader->GetID(), "uExposure");
            if (locExp >= 0) pglUniform1f(locExp, m_exposure);

            int locGamma = pglGetUniformLocation(m_postProcessShader->GetID(), "uGamma");
            if (locGamma >= 0) pglUniform1f(locGamma, m_gamma);

            pglBindVertexArray(m_screenQuadVAO);
            pglDisable(GL_DEPTH_TEST);
            pglDrawArrays(GL_TRIANGLES, 0, 6);
            pglBindVertexArray(0);
            pglEnable(GL_DEPTH_TEST);
        }
    }

    // 5b. Debug Draw Pass
    if (!m_debugVertices.empty() && m_debugShader && m_debugVAO) {
         m_debugShader->Use();
         
         Matrix4 view;
         Matrix4 projection;
         std::memcpy(view.m, m_view, sizeof(m_view));
         std::memcpy(projection.m, m_projection, sizeof(m_projection));
         Matrix4 vp = projection * view;
         
         int locVP = pglGetUniformLocation(m_debugShader->GetID(), "uVP");
         if (locVP >= 0) pglUniformMatrix4fv(locVP, 1, GL_FALSE, vp.m);
         
         // Combine vertices/colors
         std::vector<float> bufferData;
         bufferData.reserve(m_debugVertices.size() * 2);
         size_t count = m_debugVertices.size() / 3;
         for (size_t i = 0; i < count; ++i) {
             bufferData.push_back(m_debugVertices[i*3+0]);
             bufferData.push_back(m_debugVertices[i*3+1]);
             bufferData.push_back(m_debugVertices[i*3+2]);
             bufferData.push_back(m_debugColors[i*3+0]);
             bufferData.push_back(m_debugColors[i*3+1]);
             bufferData.push_back(m_debugColors[i*3+2]);
         }
         
         pglBindVertexArray(m_debugVAO);
         pglBindBuffer(GL_ARRAY_BUFFER, m_debugVBO);
         pglBufferData(GL_ARRAY_BUFFER, bufferData.size() * sizeof(float), bufferData.data(), GL_DYNAMIC_DRAW);
         
         size_t stride = 6 * sizeof(float);
         pglEnableVertexAttribArray(0);
         pglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, (int)stride, (void*)0);
         pglEnableVertexAttribArray(1);
         pglVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, (int)stride, (void*)(3 * sizeof(float)));
         
         pglDrawArrays(GL_LINES, 0, (int)count);
         pglBindVertexArray(0);
         
         m_debugVertices.clear();
         m_debugColors.clear();
    }

    // 6. Draw 2D Sprites / UI (Overlay)
    if (!m_textureDrawQueue.empty()) {
        // Ensure we are drawing to the default framebuffer
        BindDefaultFramebuffer();
        
        // Reset viewport to window size for 2D sprites
        int w, h; SDL_GetWindowSize(m_window, &w, &h);
        pglViewport(0, 0, w, h);

        pglDisable(GL_DEPTH_TEST);
        pglEnable(GL_BLEND);
        pglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        for (const auto& cmd : m_textureDrawQueue) {
            ExecuteDrawTexture(cmd);
        }
        
        pglEnable(GL_DEPTH_TEST);
        m_textureDrawQueue.clear();
    }

    m_drawQueue.clear();
    if (m_presentEnabled) {
        SDL_GL_SwapWindow(m_window);
    }
}

void OpenGLRenderer::Present() {
    if (m_window) {
        SDL_GL_SwapWindow(m_window);
    }
}

void OpenGLRenderer::Shutdown() {
    if (m_window && m_context) {
        if (SDL_GL_GetCurrentContext() != m_context) {
            SDL_GL_MakeCurrent(m_window, m_context);
        }
    }

    // Post-processing / deferred
    DeleteFramebuffer(m_fbo);
    DeleteTexture(m_screenTexture);
    DeleteTexture(m_brightTexture);
    DeleteRenderbuffer(m_rbo);
    DeleteFramebufferArray(m_pingPongFBO);
    DeleteTextureArray(m_pingPongTexture);
    DeleteFramebuffer(m_finalFBO);
    DeleteTexture(m_finalTexture);

    // G-buffer
    DeleteFramebuffer(m_gBuffer);
    DeleteTexture(m_gPosition);
    DeleteTexture(m_gNormal);
    DeleteTexture(m_gAlbedoSpec);
    DeleteRenderbuffer(m_gDepthRBO);

    // Shadow map
    DeleteFramebuffer(m_shadowMapFBO);
    DeleteTexture(m_shadowMapTexture);

    // Geometry buffers
    if (m_screenQuadVAO && pglDeleteVertexArrays) { pglDeleteVertexArrays(1, &m_screenQuadVAO); m_screenQuadVAO = 0; }
    if (m_debugVAO && pglDeleteVertexArrays) { pglDeleteVertexArrays(1, &m_debugVAO); m_debugVAO = 0; }
    if (m_debugVBO && pglDeleteBuffers) { pglDeleteBuffers(1, &m_debugVBO); m_debugVBO = 0; }
    if (m_screenQuadVBO && pglDeleteBuffers) { pglDeleteBuffers(1, &m_screenQuadVBO); m_screenQuadVBO = 0; }
    if (m_spriteVBO && pglDeleteBuffers) { pglDeleteBuffers(1, &m_spriteVBO); m_spriteVBO = 0; }
    if (m_spriteEBO && pglDeleteBuffers) { pglDeleteBuffers(1, &m_spriteEBO); m_spriteEBO = 0; }
    if (m_spriteVAO && pglDeleteVertexArrays) { pglDeleteVertexArrays(1, &m_spriteVAO); m_spriteVAO = 0; }

    m_window = nullptr;
    m_context = nullptr;
}

IGraphicsAPI::TextureHandle OpenGLRenderer::CreateTexture(uint32_t width, uint32_t height, const uint8_t* pixels) {
    IGraphicsAPI::TextureHandle h;
    if (!m_window || !m_context) return h;
    if (SDL_GL_GetCurrentContext() != m_context) {
        if (SDL_GL_MakeCurrent(m_window, m_context) != 0) return h;
    }

    unsigned int id = 0;
    pglGenTextures(1, &id);
    pglBindTexture(GL_TEXTURE_2D, id);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    if (pixels && width > 0 && height > 0) {
        pglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (int)width, (int)height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    }

    h.id = id;
    return h;
}

void OpenGLRenderer::DestroyTexture(const IGraphicsAPI::TextureHandle& h) {
    if (!h.IsValid()) return;
    if (SDL_GL_GetCurrentContext() != m_context) {
        if (SDL_GL_MakeCurrent(m_window, m_context) != 0) return;
    }
    unsigned int id = static_cast<unsigned int>(h.id);
    if (pglDeleteTextures) {
        pglDeleteTextures(1, &id);
    }
}

MeshHandle OpenGLRenderer::CreateMesh(const MeshDesc& desc) {
    MeshHandle h;
    if (!m_window || !m_context) return h;
    if (SDL_GL_GetCurrentContext() != m_context) {
        if (SDL_GL_MakeCurrent(m_window, m_context) != 0) return h;
    }

    GLMesh mesh;
    pglGenVertexArrays(1, &mesh.vao);
    pglBindVertexArray(mesh.vao);

    pglGenBuffers(1, &mesh.vbo);
    pglBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    
    std::vector<float> interleaved;
    bool hasNormals = !desc.normals.empty();
    bool hasUVs = !desc.uvs.empty();
    size_t vertexCount = desc.vertices.size() / 3;
    
    for (size_t i = 0; i < vertexCount; ++i) {
        interleaved.push_back(desc.vertices[i*3+0]);
        interleaved.push_back(desc.vertices[i*3+1]);
        interleaved.push_back(desc.vertices[i*3+2]);
        if (hasNormals) {
            interleaved.push_back(desc.normals[i*3+0]);
            interleaved.push_back(desc.normals[i*3+1]);
            interleaved.push_back(desc.normals[i*3+2]);
        }
        if (hasUVs) {
            interleaved.push_back(desc.uvs[i*2+0]);
            interleaved.push_back(desc.uvs[i*2+1]);
        }
    }
    
    pglBufferData(GL_ARRAY_BUFFER, (ptrdiff_t)(interleaved.size() * sizeof(float)), interleaved.data(), GL_STATIC_DRAW);

    pglGenBuffers(1, &mesh.ebo);
    pglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    
    uint32_t maxIndex = 0;
    for (uint32_t i : desc.indices) if (i > maxIndex) maxIndex = i;
    
    if (maxIndex <= 0xFFFFu) {
        std::vector<uint16_t> indices16;
        for (uint32_t i : desc.indices) indices16.push_back((uint16_t)i);
        pglBufferData(GL_ELEMENT_ARRAY_BUFFER, (ptrdiff_t)(indices16.size() * sizeof(uint16_t)), indices16.data(), GL_STATIC_DRAW);
        mesh.indexType = GL_UNSIGNED_SHORT;
    } else {
        pglBufferData(GL_ELEMENT_ARRAY_BUFFER, (ptrdiff_t)(desc.indices.size() * sizeof(uint32_t)), desc.indices.data(), GL_STATIC_DRAW);
        mesh.indexType = GL_UNSIGNED_INT;
    }
    mesh.indexCount = desc.indices.size();

    size_t stride = (3 + (hasNormals ? 3 : 0) + (hasUVs ? 2 : 0)) * sizeof(float);
    pglEnableVertexAttribArray(0);
    pglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, (int)stride, (const void*)0);
    
    size_t offset = 3 * sizeof(float);
    if (hasNormals) {
        pglEnableVertexAttribArray(1);
        pglVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, (int)stride, (const void*)offset);
        offset += 3 * sizeof(float);
    }

    if (hasUVs) {
        pglEnableVertexAttribArray(2);
        pglVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, (int)stride, (const void*)offset);
    }

    pglBindVertexArray(0);

    h.id = m_nextMeshId++;
    m_meshes[h.id] = mesh;
    return h;
}

void OpenGLRenderer::DestroyMesh(const MeshHandle& h) {
    if (!h.IsValid()) return;
    auto it = m_meshes.find(h.id);
    if (it == m_meshes.end()) return;
    
    bool contextOk = true;
    if (SDL_GL_GetCurrentContext() != m_context) {
        if (SDL_GL_MakeCurrent(m_window, m_context) != 0) contextOk = false;
    }

    if (contextOk) {
        GLMesh& m = it->second;
        if (pglDeleteBuffers) {
            if (m.vbo) pglDeleteBuffers(1, &m.vbo);
            if (m.ebo) pglDeleteBuffers(1, &m.ebo);
        }
        if (pglDeleteVertexArrays && m.vao) pglDeleteVertexArrays(1, &m.vao);
    }
    m_meshes.erase(it);
}

void OpenGLRenderer::DrawMesh(const MeshHandle& h) {
    DrawMesh(h, nullptr, nullptr);
}

void OpenGLRenderer::DrawMesh(const MeshHandle& h, Material* material, const float* transform) {
    if (!h.IsValid()) return;
    DrawCommand cmd;
    cmd.mesh = h;
    cmd.material = material;
    if (transform) std::memcpy(cmd.transform, transform, sizeof(float) * 16);
    else {
        std::memset(cmd.transform, 0, sizeof(float) * 16);
        cmd.transform[0] = cmd.transform[5] = cmd.transform[10] = cmd.transform[15] = 1.0f;
    }
    m_drawQueue.push_back(cmd);
}

void OpenGLRenderer::ExecuteDraw(const DrawCommand& cmd, Shader* overrideShader) {
    if (!cmd.mesh.IsValid()) return;
    auto it = m_meshes.find(cmd.mesh.id);
    if (it == m_meshes.end()) return;
    const GLMesh& m = it->second;

    Shader* shader = overrideShader;
    if (!shader) {
        // Use material shader or default PBR
        // For now, just use m_pbrShader if available
        shader = m_pbrShader.get();
    }

    if (!shader) return;
    shader->Use();

    // Set uniforms
    int locModel = pglGetUniformLocation(shader->GetID(), "uModel");
    if (locModel >= 0) pglUniformMatrix4fv(locModel, 1, GL_FALSE, cmd.transform);

    // Set ViewProjection
    Matrix4 view;
    Matrix4 projection;
    std::memcpy(view.m, m_view, sizeof(m_view));
    std::memcpy(projection.m, m_projection, sizeof(m_projection));
    Matrix4 vp = projection * view;

    int locVP = pglGetUniformLocation(shader->GetID(), "uViewProjection");
    if (locVP >= 0) pglUniformMatrix4fv(locVP, 1, GL_FALSE, vp.m);

    // Bind Mesh
    pglBindVertexArray(m.vao);
    
    // Bind Material Textures if not override shader
    if (!overrideShader && cmd.material) {
        // Bind Albedo
        if (cmd.material->baseColorTextureObj) {
             pglActiveTexture(GL_TEXTURE0);
             pglBindTexture(GL_TEXTURE_2D, cmd.material->baseColorTextureObj->GetID());
             int loc = pglGetUniformLocation(shader->GetID(), "uAlbedoMap");
             if (loc >= 0) pglUniform1i(loc, 0);
        }
        // ... other textures
    }

    if (m.indexCount > 0) {
        pglDrawElements(GL_TRIANGLES, (int)m.indexCount, m.indexType, (void*)0);
    } else {
        // Draw arrays if no indices? (Not supported by CreateMesh currently)
    }

    pglBindVertexArray(0);
}

void OpenGLRenderer::SetGlobalLight(const float direction[3], const float color[3], float intensity) {
    if (direction) { m_lightDir[0] = direction[0]; m_lightDir[1] = direction[1]; m_lightDir[2] = direction[2]; }
    if (color) { m_lightColor[0] = color[0]; m_lightColor[1] = color[1]; m_lightColor[2] = color[2]; }
    m_lightIntensity = intensity;
}

void OpenGLRenderer::AddPointLight(const PointLightData& light) {
    m_pointLights.push_back(light);
}

void OpenGLRenderer::ClearPointLights() {
    m_pointLights.clear();
}

void OpenGLRenderer::SetPostProcessParams(float exposure, float gamma) {
    m_exposure = exposure;
    m_gamma = gamma;
}

void OpenGLRenderer::DrawTexture(Texture* tex, float x, float y, float w, float h, float u0, float v0, float u1, float v1, uint32_t color) {
    if (!tex) return;
    TextureDrawCommand cmd;
    cmd.texture = tex;
    cmd.x = x; cmd.y = y; cmd.w = w; cmd.h = h;
    cmd.u0 = u0; cmd.v0 = v0; cmd.u1 = u1; cmd.v1 = v1;
    cmd.color = color;
    m_textureDrawQueue.push_back(cmd);
}

void OpenGLRenderer::ExecuteDrawTexture(const TextureDrawCommand& cmd) {
    if (!cmd.texture || !m_spriteShader) return;
    
    // Ensure texture is uploaded
    cmd.texture->UploadToRenderer(this);
    unsigned int texId = cmd.texture->GetID();
    if (!texId) return;

    m_spriteShader->Use();

    float verts[16] = {
        cmd.x,         cmd.y,         cmd.u0, cmd.v0,
        cmd.x + cmd.w, cmd.y,         cmd.u1, cmd.v0,
        cmd.x + cmd.w, cmd.y + cmd.h, cmd.u1, cmd.v1,
        cmd.x,         cmd.y + cmd.h, cmd.u0, cmd.v1
    };

    int wWin=0,hWin=0; SDL_GetWindowSize(m_window,&wWin,&hWin);
    int locScreen = pglGetUniformLocation(m_spriteShader->GetID(), "uScreen");
    if (locScreen >= 0) {
        if (pglUniform2f) {
            pglUniform2f(locScreen, (float)wWin, (float)hWin);
        }
    }

    float a = ((cmd.color >> 24) & 0xFF) / 255.0f;
    float r = ((cmd.color >> 16) & 0xFF) / 255.0f;
    float g = ((cmd.color >> 8) & 0xFF) / 255.0f;
    float b = ((cmd.color >> 0) & 0xFF) / 255.0f;
    int locColor = pglGetUniformLocation(m_spriteShader->GetID(), "uColor");
    if (locColor >= 0) pglUniform4f(locColor, r, g, b, a);

    int locTex = pglGetUniformLocation(m_spriteShader->GetID(), "uTex");
    if (locTex >= 0) pglUniform1i(locTex, 0);

    pglActiveTexture(GL_TEXTURE0);
    pglBindTexture(GL_TEXTURE_2D, texId);

    pglBindVertexArray(m_spriteVAO);
    pglBindBuffer(GL_ARRAY_BUFFER, m_spriteVBO);
    
    if (pglBufferSubData) {
        pglBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
    } else if (pglBufferData) {
        pglBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
    }

    pglDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (const void*)0);
    pglBindVertexArray(0);
}

void OpenGLRenderer::DrawLines(const std::vector<float>& vertices, const std::vector<float>& colors) {
    if (vertices.empty()) return;
    
    // Accumulate lines for deferred rendering in EndFrame
    size_t startSize = m_debugVertices.size();
    m_debugVertices.insert(m_debugVertices.end(), vertices.begin(), vertices.end());
    
    if (colors.size() >= vertices.size()) {
        m_debugColors.insert(m_debugColors.end(), colors.begin(), colors.end());
    } else {
        // Pad with default color (Green)
        size_t count = vertices.size() / 3;
        for (size_t i = 0; i < count; ++i) {
            m_debugColors.push_back(0.0f); m_debugColors.push_back(1.0f); m_debugColors.push_back(0.0f);
        }
    }
}

void OpenGLRenderer::SetViewProjection(const float* view, const float* projection) {
    if (view) std::memcpy(m_view, view, sizeof(float) * 16);
    else {
        std::memset(m_view, 0, sizeof(float) * 16);
        m_view[0] = m_view[5] = m_view[10] = m_view[15] = 1.0f;
    }

    if (projection) std::memcpy(m_projection, projection, sizeof(float) * 16);
    else {
        std::memset(m_projection, 0, sizeof(float) * 16);
        m_projection[0] = m_projection[5] = m_projection[10] = m_projection[15] = 1.0f;
    }
}

bool OpenGLRenderer::ReadDepthAtWindowCoord(int x, int y, float& outDepth) {
    if (!m_window) return false;
    if (SDL_GL_MakeCurrent(m_window, m_context) != 0) return false;

    // Resolve glReadPixels lazily
    if (!pglReadPixels) {
        auto addrReadPixels = (void*)SDL_GL_GetProcAddress("glReadPixels");
        if (!addrReadPixels) return false;
        pglReadPixels = (PFNGLREADPIXELSPROC)addrReadPixels;
    }

    // Prefer the g-buffer depth if available, otherwise fall back to the post-process FBO or default
    unsigned int fboToRead = (m_gBuffer != 0) ? m_gBuffer : ((m_fbo != 0) ? m_fbo : 0);

    // Bind framebuffer for read
    if (pglBindFramebuffer) pglBindFramebuffer(GL_FRAMEBUFFER, fboToRead);

    int ix = x;
    int iy = y;
    int readY = m_screenHeight - 1 - iy; // convert to GL lower-left origin

    float depth = 1.0f;
    pglReadPixels(ix, readY, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);

    // Restore default framebuffer binding
    if (pglBindFramebuffer) pglBindFramebuffer(GL_FRAMEBUFFER, 0);

    outDepth = depth;
    return true;
}

void OpenGLRenderer::BindDefaultFramebuffer() {
    if (pglBindFramebuffer) pglBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Ensure default framebuffer color writes/readback are enabled.
    // Shadow-map setup uses GL_NONE for draw/read buffers.
    if (pglDrawBuffer) pglDrawBuffer(GL_BACK);
    if (pglReadBuffer) pglReadBuffer(GL_BACK);
} 

void OpenGLRenderer::Clear(float r, float g, float b, float a) {
    if (pglClearColor && pglClear) {
        pglClearColor(r, g, b, a);
        pglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
}

} // namespace Genesis::Engine
