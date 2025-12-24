#pragma once

#include "engine/IGraphics.h"

#ifdef _WIN32
struct HWND__;
typedef HWND__* HWND;
#endif

#ifdef HAVE_WGPU
#include <wgpu.h>
#include <dawn/native/DawnNative.h>
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
    // Dawn native instance and selected adapter
    dawn::native::Instance m_instance;
    dawn::native::Adapter m_adapter;

    // Core WebGPU/Dawn handles
    WGPUDevice m_device = nullptr;
    WGPUQueue m_queue = nullptr;
    WGPUSurface m_surface = nullptr;
    WGPUTextureFormat m_surfaceFormat = WGPUTextureFormat_BGRA8Unorm;
    int m_surfaceWidth = 0;
    int m_surfaceHeight = 0;
    bool m_surfaceConfigured = false;

    // Keep the SDL window around
    SDL_Window* m_window = nullptr;

    // Pipeline & shader modules
    WGPURenderPipeline m_pipeline = nullptr;
    WGPUShaderModule m_vsModule = nullptr;
    WGPUShaderModule m_fsModule = nullptr;

    // Device lost handling
    bool m_deviceLost = false;

    // Internal handler for uncaptured device errors (invoked from C callback)
    void HandleUncapturedDeviceError(WGPUErrorType type, WGPUStringView message);
#endif
};

} // namespace Genesis::Engine
