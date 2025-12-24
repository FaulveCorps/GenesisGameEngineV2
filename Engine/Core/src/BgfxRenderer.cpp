#include "engine/BgfxRenderer.h"
#include <iostream>
#include <SDL_syswm.h>

#ifdef HAVE_BGFX
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#endif

#ifdef _WIN32
#include <Windows.h>
#ifdef HAVE_BGFX
#pragma comment(lib, "bgfx.lib")
#endif
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

    m_initialized = true;
    std::cout << "BgfxRenderer: initialized (bgfx)" << std::endl;
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
    bgfx::shutdown();
#endif
    m_initialized = false;
    std::cout << "BgfxRenderer: shutdown" << std::endl;
    std::cout << "BgfxRenderer::Shutdown -> exit" << std::endl;
#endif
}

} // namespace Genesis::Engine
