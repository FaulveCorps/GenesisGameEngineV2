#include "engine/DirectXRenderer.h"
#include <iostream>

#ifdef _WIN32
// Include DirectX headers when on Windows; these require Windows SDK availability
#include <Windows.h>
#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")
#endif

namespace Genesis::Engine {

bool DirectXRenderer::Init(SDL_Window* /*window*/, SDL_GLContext /*glContext*/) {
#ifdef _WIN32
    // Minimal skeleton: a real implementation would create D3D device, swapchain, render target, etc.
    // For now, mark initialized on Windows so the backend can exist as a selectable renderer.
    m_initialized = true;
    std::cout << "DirectXRenderer: initialized (skeleton)" << std::endl;
    return true;
#else
    std::cerr << "DirectXRenderer: not supported on this platform" << std::endl;
    return false;
#endif
}

void DirectXRenderer::BeginFrame() {
    if (!m_initialized) return;
    // Stub: clear would be implemented here
}

void DirectXRenderer::EndFrame() {
    if (!m_initialized) return;
    // Stub: present swapchain here
}

void DirectXRenderer::Shutdown() {
    if (!m_initialized) return;
    // Stub: release D3D resources here
    m_initialized = false;
    std::cout << "DirectXRenderer: shutdown" << std::endl;
}

} // namespace Genesis::Engine
