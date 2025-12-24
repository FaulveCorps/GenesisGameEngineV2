#include "engine/BgfxRenderer.h"
#include <iostream>
#include <SDL_syswm.h>

#ifdef HAVE_BGFX
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <vector>
#include <fstream>
#include <filesystem>
#endif

#ifdef _WIN32
#include <Windows.h>

#endif

namespace Genesis::Engine {

bool BgfxRenderer::Init(SDL_Window* window, SDL_GLContext /*glContext*/) {
#ifdef _WIN32
    if (!window) return false;

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(window, &wmInfo)) {
        std::cerr << "BgfxRenderer: SDL_GetWindowWMInfo failed: " << SDL_GetError() << std::endl;
        return false;
    }
    m_hwnd = wmInfo.info.win.window;

#ifdef HAVE_BGFX
    bgfx::PlatformData pd;
    pd.ndt = nullptr;
    pd.nwh = m_hwnd;
    pd.context = nullptr;
    pd.backBuffer = nullptr;
    pd.backBufferDS = nullptr;

    bgfx::Init init;
    init.type = bgfx::RendererType::Direct3D12; // prefer D3D12 on Windows when available
    init.platformData = pd;

    if (!bgfx::init(init)) {
        std::cerr << "BgfxRenderer: bgfx::init failed" << std::endl;
        return false;
    }

    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    bgfx::reset(w, h, BGFX_RESET_NONE);

    // Attempt to load compiled shader binaries from assets/shaders/bgfx/<renderer>/*
    // We'll look for vs_triangle.bin and fs_triangle.bin under any subdirectory.
    namespace fs = std::filesystem;
    fs::path shaderRoot = fs::path("assets") / "shaders" / "bgfx";
    if (fs::exists(shaderRoot) && fs::is_directory(shaderRoot)) {
        for (auto &d : fs::directory_iterator(shaderRoot)) {
            if (!d.is_directory()) continue;
            fs::path vs = d.path() / "vs_triangle.bin";
            fs::path fsb = d.path() / "fs_triangle.bin";
            if (fs::exists(vs) && fs::exists(fsb)) {
                std::ifstream vifs(vs, std::ios::binary);
                std::ifstream fifs(fsb, std::ios::binary);
                if (vifs && fifs) {
                    std::vector<uint8_t> vbuf((std::istreambuf_iterator<char>(vifs)), std::istreambuf_iterator<char>());
                    std::vector<uint8_t> fbuf((std::istreambuf_iterator<char>(fifs)), std::istreambuf_iterator<char>());
                    auto vsh = bgfx::createShader(bgfx::makeRef(vbuf.data(), (uint32_t)vbuf.size()));
                    auto fsh = bgfx::createShader(bgfx::makeRef(fbuf.data(), (uint32_t)fbuf.size()));
                    auto prog = bgfx::createProgram(vsh, fsh, true);
                    if (bgfx::isValid(prog)) {
                        std::cout << "BgfxRenderer: loaded triangle shaders from " << d.path().string() << std::endl;
                        // create simple triangle vertex/index buffers
                        struct Vertex { float x,y,z; uint32_t abgr; };
                        Vertex verts[3] = { {0.0f, 0.5f, 0.0f, 0xff0000ff}, {-0.5f, -0.5f, 0.0f, 0xff00ff00}, {0.5f, -0.5f, 0.0f, 0xffff0000} };
                        uint16_t idx[3] = {0,1,2};

                        m_layout.begin();
                        m_layout.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float);
                        m_layout.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true);
                        m_layout.end();

                        m_vbh = bgfx::createVertexBuffer(bgfx::makeRef(verts, sizeof(verts)), m_layout);
                        m_ibh = bgfx::createIndexBuffer(bgfx::makeRef(idx, sizeof(idx)));
                        m_program = prog;
                        m_hasTriangle = true;
                        break;
                    } else {
                        // destroy shader handles if program invalid
                        if (bgfx::isValid(vsh)) bgfx::destroy(vsh);
                        if (bgfx::isValid(fsh)) bgfx::destroy(fsh);
                    }
                }
            }
        }
    }

    m_initialized = true;
    std::cout << "BgfxRenderer: initialized (bgfx)" << std::endl;
    if (!m_hasTriangle) std::cout << "BgfxRenderer: triangle shaders not found; running clear-only smoke test" << std::endl;
    return true;
#else
    std::cerr << "BgfxRenderer: bgfx not available (HAVE_BGFX not defined)" << std::endl;
    return false;
#endif
#else
    std::cerr << "BgfxRenderer: not supported on this platform" << std::endl;
    return false;
#endif
}

void BgfxRenderer::BeginFrame() {
#ifdef _WIN32
    if (!m_initialized) return;
#ifdef HAVE_BGFX
    // set view 0 clear
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x103040ff, 1.0f, 0);
    bgfx::setViewRect(0, 0, 0, bgfx::BackbufferRatio::Equal);
#endif
#endif
}

void BgfxRenderer::EndFrame() {
#ifdef _WIN32
    if (!m_initialized) return;
#ifdef HAVE_BGFX
    // If we loaded triangle shaders, submit a simple draw for view 0
    if (m_hasTriangle && bgfx::isValid(m_program)) {
        float mat[16] = {0};
        mat[0] = 1.0f; mat[5] = 1.0f; mat[10] = 1.0f; mat[15] = 1.0f;
        bgfx::setTransform(mat);
        bgfx::setVertexBuffer(0, m_vbh);
        bgfx::setIndexBuffer(m_ibh);
        bgfx::submit(0, m_program);
    }

    bgfx::frame();
    std::cout << "BgfxRenderer::EndFrame -> frame submitted" << std::endl;
#endif
#endif
}

void BgfxRenderer::Shutdown() {
#ifdef _WIN32
    if (!m_initialized) return;
    std::cout << "BgfxRenderer::Shutdown -> enter" << std::endl;
#ifdef HAVE_BGFX
    if (m_hasTriangle) {
        if (bgfx::isValid(m_vbh)) bgfx::destroy(m_vbh);
        if (bgfx::isValid(m_ibh)) bgfx::destroy(m_ibh);
        if (bgfx::isValid(m_program)) bgfx::destroy(m_program);
        m_hasTriangle = false;
    }
    bgfx::shutdown();
#endif
    m_initialized = false;
    std::cout << "BgfxRenderer: shutdown" << std::endl;
    std::cout << "BgfxRenderer::Shutdown -> exit" << std::endl;
#endif
}

} // namespace Genesis::Engine
