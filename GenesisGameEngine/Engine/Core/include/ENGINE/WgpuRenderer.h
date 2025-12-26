#pragma once

#include "engine/IGraphics.h"
#include <string>

#ifdef _WIN32
struct HWND__;
typedef HWND__* HWND;
#endif

#ifdef HAVE_WGPU
#if defined(__has_include)
  #if __has_include(<wgpu.h>)
    #include <wgpu.h>
  #elif __has_include(<webgpu/webgpu.h>)
    #include <webgpu/webgpu.h>
  #elif __has_include(<dawn/webgpu.h>)
    #include <dawn/webgpu.h>
  #else
    #error "WGPU header not found"
  #endif
#else
  /* Fallback for compilers without __has_include */
  #include <webgpu/webgpu.h>
#endif
#include <dawn/native/DawnNative.h>
#endif

#include <vector>

namespace Genesis::Engine {

class WgpuRenderer : public IGraphicsAPI {
public:
    WgpuRenderer() = default;
    ~WgpuRenderer() override = default;

    bool Init(SDL_Window* window, SDL_GLContext glContext) override;
    void BeginFrame() override;
    void EndFrame() override;
    void Shutdown() override;

    std::string GetName() const override { return std::string("wgpu"); }

    // Render an offscreen image and read back pixels (RGBA8); returns true on success
    bool ReadbackOffscreen(uint32_t width, uint32_t height, std::vector<uint8_t>& out);

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

    // Internal handlers for uncaptured errors and device lost notifications
    void HandleUncapturedDeviceError(WGPUErrorType type, WGPUStringView message);
    void HandleDeviceLost(WGPUDeviceLostReason reason, WGPUStringView message);

    // Static C-compatible callbacks wired into WGPU device descriptor
    static void OnUncapturedErrorCallback(const WGPUDevice* device, WGPUErrorType type, WGPUStringView message, void* userdata1, void* userdata2);
    static void OnDeviceLostCallback(const WGPUDevice* device, WGPUDeviceLostReason reason, WGPUStringView message, void* userdata1, void* userdata2);
#endif
};

} // namespace Genesis::Engine
