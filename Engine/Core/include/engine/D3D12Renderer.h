#pragma once

#include "engine/IGraphics.h"

#ifdef _WIN32
// Forward declarations for D3D12 COM interfaces
struct IDXGIFactory4;
struct ID3D12Device;
struct ID3D12CommandQueue;
struct IDXGISwapChain3;
struct ID3D12DescriptorHeap;
struct ID3D12Resource;
struct ID3D12CommandAllocator;
struct ID3D12GraphicsCommandList;
struct ID3D12Fence;
#endif

namespace Genesis::Engine {

class D3D12Renderer : public IGraphicsAPI {
public:
    D3D12Renderer() = default;
    ~D3D12Renderer() override = default;

    bool Init(SDL_Window* window, SDL_GLContext glContext) override;
    void BeginFrame() override;
    void EndFrame() override;
    void Shutdown() override;

private:
#ifdef _WIN32
    bool m_initialized = false;

    IDXGIFactory4* m_factory = nullptr;
    ID3D12Device* m_device = nullptr;
    ID3D12CommandQueue* m_commandQueue = nullptr;
    IDXGISwapChain3* m_swapChain = nullptr;
    ID3D12DescriptorHeap* m_rtvHeap = nullptr;
    ID3D12Resource* m_renderTargets[2] = { nullptr, nullptr };
    UINT m_rtvDescriptorSize = 0;

    ID3D12CommandAllocator* m_commandAllocator = nullptr;
    ID3D12GraphicsCommandList* m_commandList = nullptr;
    ID3D12Fence* m_fence = nullptr;
    HANDLE m_fenceEvent = nullptr;
    UINT64 m_fenceValue = 0;
    UINT m_frameIndex = 0;
    const UINT m_frameCount = 2;

#endif
};

} // namespace Genesis::Engine
