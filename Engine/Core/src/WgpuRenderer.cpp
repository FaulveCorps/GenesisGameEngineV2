#include "engine/WgpuRenderer.h"
#include <iostream>
#include <SDL_syswm.h>

namespace Genesis::Engine {

bool WgpuRenderer::Init(SDL_Window* window, SDL_GLContext /*glContext*/) {
#ifdef HAVE_WGPU
    // Initialization for wgpu would go here. For now we only provide a guarded skeleton.
    if (!window) return false;

    // TODO: implement wgpu-native / Dawn initialization path (device, surface, swapchain)
    m_initialized = true;
    std::cout << "WgpuRenderer: initialized (stub)" << std::endl;
    return true;
#else
    std::cerr << "WgpuRenderer: wgpu not available (HAVE_WGPU not defined)" << std::endl;
    return false;
#endif
}

void WgpuRenderer::BeginFrame() {
#ifdef HAVE_WGPU
    if (!m_initialized) return;
    // set up frames, etc.
#else
    (void)0;
#endif
}

void WgpuRenderer::EndFrame() {
#ifdef HAVE_WGPU
    if (!m_initialized) return;
    // submit frame
    std::cout << "WgpuRenderer::EndFrame -> frame submitted (stub)" << std::endl;
#else
    (void)0;
#endif
}

void WgpuRenderer::Shutdown() {
#ifdef HAVE_WGPU
    if (!m_initialized) return;
    // cleanup device/swapchain
    m_initialized = false;
    std::cout << "WgpuRenderer: shutdown" << std::endl;
#else
    std::cerr << "WgpuRenderer: not initialized or not available" << std::endl;
#endif
}

} // namespace Genesis::Engine
