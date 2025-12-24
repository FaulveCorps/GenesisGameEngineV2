#pragma once

#include "engine/IGraphics.h"

#ifdef _WIN32
struct HWND__;
typedef HWND__* HWND;
#endif

#ifdef HAVE_WGPU
#include <wgpu.h>
#endif

namespace Genesis::Engine {

class WgpuRenderer : public IGraphicsAPI {
public:
    WgpuRenderer() = default;
    ~WgpuRenderer() override = default;

    bool Init(SDL_Window* window, SDL_GLContext glContext) override;
    void BeginFrame() override;
    void EndFrame() override;
    void Shutdown() override;

private:
#ifdef HAVE_WGPU
    bool m_initialized = false;
    // Opaque handles to keep header compiling when HAVE_WGPU is not set
    WGPUDevice m_device = nullptr;
    WGPUSwapChain m_swapchain = nullptr;
#endif
};

} // namespace Genesis::Engine
