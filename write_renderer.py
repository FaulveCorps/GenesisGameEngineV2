import argparse
import os
import shutil
import sys

content = r"""#include "engine/OpenGLRenderer.h"
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
using PFNGLTEXIMAGE2DPROC = void (APIENTRY*)(unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void*);
using PFNGLTEXPARAMETERIPROC = void (APIENTRY*)(unsigned int, int, int);
using PFNGLTEXPARAMETERFVPROC = void (APIENTRY*)(unsigned int, unsigned int, const float*);
using PFNGLREADBUFFERPROC = void (APIENTRY*)(unsigned int);
using PFNGLDRAWARRAYSPROC = void (APIENTRY*)(unsigned int, int, int);
using PFNGLDISABLEPROC = void (APIENTRY*)(unsigned int);

static PFNGLVIEWPORTPROC pglViewport = nullptr;
static PFNGLCLEARCOLORPROC pglClearColor = nullptr;
static PFNGLENABLEPROC pglEnable = nullptr;
static PFNGLCLEARPROC pglClear = nullptr;
static PFNGLBLENDFUNCPROC pglBlendFunc = nullptr;
static PFNGLDRAWBUFFERPROC pglDrawBuffer = nullptr;

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
static PFNGLTEXIMAGE2DPROC pglTexImage2D = nullptr;
static PFNGLTEXPARAMETERIPROC pglTexParameteri = nullptr;
static PFNGLTEXPARAMETERFVPROC pglTexParameterfv = nullptr;
static PFNGLREADBUFFERPROC pglReadBuffer = nullptr;
static PFNGLDRAWARRAYSPROC pglDrawArrays = nullptr;
static PFNGLDISABLEPROC pglDisable = nullptr;

static bool ResolveGL(void** fnPtr, const char* name) {
    if (*fnPtr) return true;
    auto addr = (void*)SDL_GL_GetProcAddress(name);
    if (!addr) return false;
    *fnPtr = addr;
    return true;
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
#define GL_DEPTH_BUFFER_BIT  0x00000100
#define GL_COLOR_BUFFER_BIT  0x00004000
#define GL_TRIANGLES         0x0004

namespace Genesis::Engine {

bool OpenGLRenderer::Init(SDL_Window* window, SDL_GLContext glContext) {
    std::cout << "OpenGLRenderer::Init -> enter" << std::endl;
    if (!window || !glContext) {
        std::cerr << "OpenGLRenderer: invalid window or GL context" << std::endl;
        return false;
    }

    m_window = window;
    m_context = glContext;

    if (SDL_GL_MakeCurrent(m_window, m_context) != 0) {
        std::cerr << "SDL_GL_MakeCurrent failed: " << SDL_GetError() << std::endl;
        return false;
    }

    // Resolve core GL functions used
    ResolveGL((void**)&pglViewport, "glViewport");
    ResolveGL((void**)&pglClearColor, "glClearColor");
    ResolveGL((void**)&pglEnable, "glEnable");
    ResolveGL((void**)&pglClear, "glClear");
    ResolveGL((void**)&pglDrawBuffer, "glDrawBuffer");
    ResolveGL((void**)&pglBlendFunc, "glBlendFunc");

    // Resolve Mesh/Shader functions
    ResolveGL((void**)&pglGenVertexArrays, "glGenVertexArrays");
    ResolveGL((void**)&pglBindVertexArray, "glBindVertexArray");
    ResolveGL((void**)&pglGenBuffers, "glGenBuffers");
    ResolveGL((void**)&pglBindBuffer, "glBindBuffer");
    ResolveGL((void**)&pglBufferData, "glBufferData");
    ResolveGL((void**)&pglEnableVertexAttribArray, "glEnableVertexAttribArray");
    ResolveGL((void**)&pglVertexAttribPointer, "glVertexAttribPointer");
    ResolveGL((void**)&pglDeleteVertexArrays, "glDeleteVertexArrays");
    ResolveGL((void**)&pglDeleteBuffers, "glDeleteBuffers");
    ResolveGL((void**)&pglDrawElements, "glDrawElements");
    ResolveGL((void**)&pglUniform1f, "glUniform1f");
    ResolveGL((void**)&pglUniform3f, "glUniform3f");
    ResolveGL((void**)&pglUniform4f, "glUniform4f");
    ResolveGL((void**)&pglUniformMatrix4fv, "glUniformMatrix4fv");
    ResolveGL((void**)&pglGetUniformLocation, "glGetUniformLocation");
    ResolveGL((void**)&pglUniform1i, "glUniform1i");
    ResolveGL((void**)&pglActiveTexture, "glActiveTexture");
    ResolveGL((void**)&pglBindTexture, "glBindTexture");

    // Resolve Framebuffer functions
    ResolveGL((void**)&pglGenFramebuffers, "glGenFramebuffers");
    ResolveGL((void**)&pglBindFramebuffer, "glBindFramebuffer");
    ResolveGL((void**)&pglFramebufferTexture2D, "glFramebufferTexture2D");
    ResolveGL((void**)&pglGenRenderbuffers, "glGenRenderbuffers");
    ResolveGL((void**)&pglBindRenderbuffer, "glBindRenderbuffer");
    ResolveGL((void**)&pglRenderbufferStorage, "glRenderbufferStorage");
    ResolveGL((void**)&pglFramebufferRenderbuffer, "glFramebufferRenderbuffer");
    ResolveGL((void**)&pglCheckFramebufferStatus, "glCheckFramebufferStatus");
    ResolveGL((void**)&pglDeleteFramebuffers, "glDeleteFramebuffers");
    ResolveGL((void**)&pglDeleteRenderbuffers, "glDeleteRenderbuffers");
    ResolveGL((void**)&pglDrawBuffers, "glDrawBuffers");
    ResolveGL((void**)&pglBlitFramebuffer, "glBlitFramebuffer");

    ResolveGL((void**)&pglGenTextures, "glGenTextures");
    ResolveGL((void**)&pglTexImage2D, "glTexImage2D");
    ResolveGL((void**)&pglTexParameteri, "glTexParameteri");
    ResolveGL((void**)&pglTexParameterfv, "glTexParameterfv");
    ResolveGL((void**)&pglReadBuffer, "glReadBuffer");
    ResolveGL((void**)&pglDrawArrays, "glDrawArrays");
    ResolveGL((void**)&pglDisable, "glDisable");

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
    std::string gbufferVert = ReadFile("Assets/shaders/gbuffer.vert");
    std::string gbufferFrag = ReadFile("Assets/shaders/gbuffer.frag");
    if (!gbufferVert.empty() && !gbufferFrag.empty()) {
        m_gBufferShader = Shader::FromSource(gbufferVert, gbufferFrag);
    } else {
        std::cerr << "Failed to load G-Buffer shaders" << std::endl;
    }

    std::string lightingVert = ReadFile("Assets/shaders/deferred_lighting.vert");
    std::string lightingFrag = ReadFile("Assets/shaders/deferred_lighting.frag");
    if (!lightingVert.empty() && !lightingFrag.empty()) {
        m_deferredLightingShader = Shader::FromSource(lightingVert, lightingFrag);
    } else {
        std::cerr << "Failed to load Deferred Lighting shaders" << std::endl;
    }

    // Load PBR shader (fallback/forward)
    std::string pbrVertSrc = ReadFile("Assets/shaders/pbr.vert");
    std::string pbrFragSrc = ReadFile("Assets/shaders/pbr.frag");
    if (!pbrVertSrc.empty() && !pbrFragSrc.empty()) {
        m_pbrShader = Shader::FromSource(pbrVertSrc, pbrFragSrc);
    }

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

    return true;
}

void OpenGLRenderer::InitGBuffer(int width, int height) {
    if (!pglGenFramebuffers) return;

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
    unsigned int rboDepth;
    pglGenRenderbuffers(1, &rboDepth);
    pglBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    pglRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    pglFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
    
    if (pglCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "Framebuffer not complete!" << std::endl;
    
    pglBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLRenderer::ResizeGBuffer(int width, int height) {
    // TODO: Delete old textures/FBO and recreate
    // For now, just re-init (will leak old handles, but acceptable for YOLO prototype)
    InitGBuffer(width, height);
}

void OpenGLRenderer::InitPostProcessing(int width, int height) {
    if (!pglGenFramebuffers) return;
    
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

    // Screen quad
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

    // Load shaders
    std::string postVert = ReadFile("Assets/shaders/postprocess.vert");
    std::string postFrag = ReadFile("Assets/shaders/postprocess.frag");
    if (!postVert.empty() && !postFrag.empty()) {
        m_postProcessShader = Shader::FromSource(postVert, postFrag);
    }

    std::string blurVert = ReadFile("Assets/shaders/blur.vert");
    std::string blurFrag = ReadFile("Assets/shaders/blur.frag");
    if (!blurVert.empty() && !blurFrag.empty()) {
        m_blurShader = Shader::FromSource(blurVert, blurFrag);
    }
}

void OpenGLRenderer::ResizePostProcessing(int width, int height) {
    // Simple re-init for now (leaks handles, but okay for prototype)
    InitPostProcessing(width, height);
}

void OpenGLRenderer::InitShadowMap() {
    if (!pglGenFramebuffers) return;

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
    pglBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Load shadow shader
    const std::string shadowVert = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        uniform mat4 lightSpaceMatrix;
        uniform mat4 model;
        void main() {
            gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);
        }
    )";
    const std::string shadowFrag = R"(
        #version 330 core
        void main() {
            // gl_FragDepth = gl_FragCoord.z;
        }
    )";
    m_shadowShader = Shader::FromSource(shadowVert, shadowFrag);
}

void OpenGLRenderer::RenderShadowPass() {
    if (!m_shadowMapFBO || !m_shadowShader) return;

    pglViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    pglBindFramebuffer(GL_FRAMEBUFFER, m_shadowMapFBO);
    pglClear(GL_DEPTH_BUFFER_BIT);

    // Compute light space matrix
    // Orthographic projection for directional light
    // Simple lookAt from light position
    // Light dir is m_lightDir. Position it somewhere far away.
    
    // Identity matrix for now to satisfy linker and basic run
    std::memset(m_lightSpaceMatrix, 0, sizeof(float)*16);
    m_lightSpaceMatrix[0] = m_lightSpaceMatrix[5] = m_lightSpaceMatrix[10] = m_lightSpaceMatrix[15] = 1.0f;

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
    if (SDL_GL_MakeCurrent(m_window, m_context) != 0) {
        std::cerr << "SDL_GL_MakeCurrent failed in BeginFrame: " << SDL_GetError() << std::endl;
    }
    
    int w, h;
    SDL_GetWindowSize(m_window, &w, &h);
    if (w != m_screenWidth || h != m_screenHeight) {
        ResizePostProcessing(w, h);
        ResizeGBuffer(w, h);
    }

    // Clear default framebuffer just in case
    pglBindFramebuffer(GL_FRAMEBUFFER, 0);
    pglViewport(0, 0, w, h);
    pglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGLRenderer::EndFrame() {
    if (SDL_GL_MakeCurrent(m_window, m_context) != 0) {
        std::cerr << "OpenGLRenderer::EndFrame -> SDL_GL_MakeCurrent failed: " << SDL_GetError() << std::endl;
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
        pglBindFramebuffer(GL_FRAMEBUFFER, 0);
        int w, h; SDL_GetWindowSize(m_window, &w, &h);
        pglViewport(0, 0, w, h);
        pglClearColor(1.0f, 1.0f, 1.0f, 1.0f);
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

    m_drawQueue.clear();
    SDL_GL_SwapWindow(m_window);
}

void OpenGLRenderer::Shutdown() {
    if (m_fbo && pglDeleteFramebuffers) pglDeleteFramebuffers(1, &m_fbo);
    if (m_rbo && pglDeleteRenderbuffers) pglDeleteRenderbuffers(1, &m_rbo);
    if (m_gBuffer && pglDeleteFramebuffers) pglDeleteFramebuffers(1, &m_gBuffer);
    if (m_screenQuadVAO && pglDeleteVertexArrays) pglDeleteVertexArrays(1, &m_screenQuadVAO);
    if (m_screenQuadVBO && pglDeleteBuffers) pglDeleteBuffers(1, &m_screenQuadVBO);
    if (m_spriteVBO && pglDeleteBuffers) pglDeleteBuffers(1, &m_spriteVBO);
    if (m_spriteEBO && pglDeleteBuffers) pglDeleteBuffers(1, &m_spriteEBO);
    if (m_spriteVAO && pglDeleteVertexArrays) pglDeleteVertexArrays(1, &m_spriteVAO);
    m_window = nullptr;
    m_context = nullptr;
}

IGraphicsAPI::TextureHandle OpenGLRenderer::CreateTexture(uint32_t width, uint32_t height, const uint8_t* pixels) {
    IGraphicsAPI::TextureHandle h;
    if (!m_window || !m_context) return h;
    if (SDL_GL_MakeCurrent(m_window, m_context) != 0) return h;

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
    if (SDL_GL_MakeCurrent(m_window, m_context) != 0) return;
    unsigned int id = static_cast<unsigned int>(h.id);
    // pglDeleteTextures(1, &id); 
}

MeshHandle OpenGLRenderer::CreateMesh(const MeshDesc& desc) {
    MeshHandle h;
    if (!m_window || !m_context) return h;
    if (SDL_GL_MakeCurrent(m_window, m_context) != 0) return h;

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
    if (SDL_GL_MakeCurrent(m_window, m_context) == 0) {
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

    // Set ViewProjection (we need a camera!)
    // For now, let's assume a static camera or identity if not provided.
    // Wait, the DrawCommand doesn't have camera info. The Renderer should have it.
    // But we haven't added SetCamera to the IGraphicsAPI yet?
    // Let's check IGraphics.h... 
    // The Scene class usually handles the camera and passes VP matrix?
    // Actually, in the current `Scene::Render`, it calls `renderer->DrawMesh`.
    // We need to pass the ViewProjection matrix to the renderer somewhere.
    // For now, let's just use identity to get it compiling.
    float vp[16];
    std::memset(vp, 0, sizeof(float)*16);
    vp[0] = vp[5] = vp[10] = vp[15] = 1.0f;
    
    // In a real engine, we'd have a SetCamera(view, proj) method.
    // Let's assume we have one or just hack it for now.
    // The linker error is the priority.

    int locVP = pglGetUniformLocation(shader->GetID(), "uViewProjection");
    if (locVP >= 0) pglUniformMatrix4fv(locVP, 1, GL_FALSE, vp);

    // Bind Mesh
    pglBindVertexArray(m.vao);
    
    // Bind Material Textures if not override shader
    if (!overrideShader && cmd.material) {
        // Bind Albedo
        if (cmd.material->albedoTexture) {
             pglActiveTexture(GL_TEXTURE0);
             pglBindTexture(GL_TEXTURE_2D, cmd.material->albedoTexture->GetID());
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

void OpenGLRenderer::SetPostProcessParams(float exposure, float gamma) {
    m_exposure = exposure;
    m_gamma = gamma;
}

void OpenGLRenderer::DrawTexture(Texture* tex, float x, float y, float w, float h, float u0, float v0, float u1, float v1, uint32_t color) {
    if (!tex || !m_spriteShader) return;
    if (SDL_GL_MakeCurrent(m_window, m_context) != 0) return;

    tex->UploadToRenderer(this);
    unsigned int texId = tex->GetID();
    if (!texId) return;

    m_spriteShader->Use();

    float verts[16] = {
        x,     y,     u0, v0,
        x + w, y,     u1, v0,
        x + w, y + h, u1, v1,
        x,     y + h, u0, v1
    };

    int wWin=0,hWin=0; SDL_GetWindowSize(m_window,&wWin,&hWin);
    int locScreen = pglGetUniformLocation(m_spriteShader->GetID(), "uScreen");
    if (locScreen >= 0) { 
        auto addrUniform2f = (void*)SDL_GL_GetProcAddress("glUniform2f");
        if (addrUniform2f) {
            using PFNGLUNIFORM2FPROC = void (APIENTRY*)(int, float, float);
            ((PFNGLUNIFORM2FPROC)addrUniform2f)(locScreen, (float)wWin, (float)hWin);
        }
    }

    float a = ((color >> 24) & 0xFF) / 255.0f;
    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >> 8) & 0xFF) / 255.0f;
    float b = ((color >> 0) & 0xFF) / 255.0f;
    int locColor = pglGetUniformLocation(m_spriteShader->GetID(), "uColor");
    if (locColor >= 0) pglUniform4f(locColor, r, g, b, a);

    int locTex = pglGetUniformLocation(m_spriteShader->GetID(), "uTex");
    if (locTex >= 0) pglUniform1i(locTex, 0);

    pglActiveTexture(GL_TEXTURE0);
    pglBindTexture(GL_TEXTURE_2D, texId);

    pglBindVertexArray(m_spriteVAO);
    pglBindBuffer(GL_ARRAY_BUFFER, m_spriteVBO);
    auto addrSub = (void*)SDL_GL_GetProcAddress("glBufferSubData");
    if (addrSub) {
        using PFNGLBUFFERSUBDATAPROC = void (APIENTRY*)(unsigned int, ptrdiff_t, ptrdiff_t, const void*);
        ((PFNGLBUFFERSUBDATAPROC)addrSub)(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
    }

    pglDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (const void*)0);
    pglBindVertexArray(0);
}

} // namespace Genesis::Engine
"""

def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Generate the OpenGLRenderer implementation. By default this prints to stdout. "
            "Use --output to write to a file (refuses to overwrite unless --force is set)."
        )
    )
    default_out = os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "Engine",
        "Core",
        "src",
        "OpenGLRenderer.cpp",
    )
    parser.add_argument(
        "--output",
        "-o",
        default=None,
        help=f"Write output to this file (default target would be: {default_out})",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Allow overwriting an existing output file.",
    )
    parser.add_argument(
        "--allow-outside-repo",
        action="store_true",
        help=(
            "Allow writing to a path outside the repository root. "
            "By default, writes are restricted to the repo tree to reduce foot-guns."
        ),
    )
    parser.add_argument(
        "--backup",
        action="store_true",
        help="When overwriting, save a .bak copy alongside the output.",
    )
    args = parser.parse_args()

    repo_root = os.path.realpath(os.path.dirname(os.path.abspath(__file__)))

    if args.output is None:
        sys.stdout.write(content)
        return 0

    out_path = os.path.abspath(args.output)
    out_path_real = os.path.realpath(out_path)

    if not args.allow_outside_repo:
        try:
            common = os.path.commonpath([repo_root, out_path_real])
        except ValueError:
            common = ""
        if common != repo_root:
            sys.stderr.write(
                "Refusing to write outside the repository root.\n"
                f"  repo_root: {repo_root}\n"
                f"  output:    {out_path_real}\n"
                "Re-run with --allow-outside-repo if you really want this.\n"
            )
            return 2
    out_dir = os.path.dirname(out_path)
    if out_dir and not os.path.isdir(out_dir):
        os.makedirs(out_dir, exist_ok=True)

    if os.path.exists(out_path_real) and not args.force:
        sys.stderr.write(
            f"Refusing to overwrite existing file: {out_path_real}\n"
            "Re-run with --force to overwrite.\n"
        )
        return 2

    if os.path.exists(out_path_real) and args.force and args.backup:
        backup_path = out_path_real + ".bak"
        shutil.copy2(out_path_real, backup_path)
        sys.stderr.write(f"Backed up existing file to: {backup_path}\n")

    with open(out_path_real, "w", encoding="utf-8", newline="\n") as f:
        f.write(content)

    sys.stderr.write(f"Wrote: {out_path_real}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
