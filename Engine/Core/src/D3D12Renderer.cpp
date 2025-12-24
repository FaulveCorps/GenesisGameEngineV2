#include "engine/D3D12Renderer.h"
#include <iostream>

#ifdef _WIN32
#include <Windows.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#endif

namespace Genesis::Engine {

bool D3D12Renderer::Init(SDL_Window* window, SDL_GLContext /*glContext*/) {
#ifdef _WIN32
    std::cout << "D3D12Renderer: initializing" << std::endl;

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(window, &wmInfo)) {
        std::cerr << "D3D12Renderer: SDL_GetWindowWMInfo failed: " << SDL_GetError() << std::endl;
        return false;
    }
    HWND hwnd = wmInfo.info.win.window;

    // Create DXGI factory
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&m_factory)))) {
        std::cerr << "D3D12Renderer: CreateDXGIFactory1 failed" << std::endl;
        return false;
    }

    // Create D3D12 device
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)))) {
        std::cerr << "D3D12Renderer: D3D12CreateDevice failed" << std::endl;
        return false;
    }

    // Create command queue
    D3D12_COMMAND_QUEUE_DESC cqDesc = {};
    cqDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    cqDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    cqDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    if (FAILED(m_device->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&m_commandQueue)))) {
        std::cerr << "D3D12Renderer: CreateCommandQueue failed" << std::endl;
        return false;
    }

    // Create swapchain
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = m_frameCount;
    sd.BufferDesc.Width = 0; // automatic sizing
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    IDXGISwapChain* tmpSwap = nullptr;
    if (FAILED(m_factory->CreateSwapChain(m_commandQueue, &sd, &tmpSwap))) {
        std::cerr << "D3D12Renderer: CreateSwapChain failed" << std::endl;
        return false;
    }

    if (FAILED(tmpSwap->QueryInterface(IID_PPV_ARGS(&m_swapChain)))) {
        std::cerr << "D3D12Renderer: QueryInterface for IDXGISwapChain3 failed" << std::endl;
        tmpSwap->Release();
        return false;
    }
    tmpSwap->Release();

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Create descriptor heap for RTVs
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = m_frameCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if (FAILED(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)))) {
        std::cerr << "D3D12Renderer: CreateDescriptorHeap failed" << std::endl;
        return false;
    }
    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // Create render target views for each back buffer
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < m_frameCount; ++i) {
        if (FAILED(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])))) {
            std::cerr << "D3D12Renderer: GetBuffer failed for index " << i << std::endl;
            return false;
        }
        m_device->CreateRenderTargetView(m_renderTargets[i], nullptr, rtvHandle);
        rtvHandle.ptr += m_rtvDescriptorSize;
    }

    // Create command allocator and command list
    if (FAILED(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)))) {
        std::cerr << "D3D12Renderer: CreateCommandAllocator failed" << std::endl;
        return false;
    }
    if (FAILED(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator, nullptr, IID_PPV_ARGS(&m_commandList)))) {
        std::cerr << "D3D12Renderer: CreateCommandList failed" << std::endl;
        return false;
    }
    // Command lists are created in recording state; close it for now
    m_commandList->Close();

    // Create fence for sync
    if (FAILED(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)))) {
        std::cerr << "D3D12Renderer: CreateFence failed" << std::endl;
        return false;
    }
    m_fenceValue = 1;
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent) {
        std::cerr << "D3D12Renderer: CreateEvent failed" << std::endl;
        return false;
    }

    m_initialized = true;
    std::cout << "D3D12Renderer: initialized (d3d12)" << std::endl;
    return true;
#else
    std::cerr << "D3D12Renderer: not supported on this platform" << std::endl;
    return false;
#endif
}

void D3D12Renderer::BeginFrame() {
#ifdef _WIN32
    if (!m_initialized) return;

    // Reset allocator and command list
    m_commandAllocator->Reset();
    m_commandList->Reset(m_commandAllocator, nullptr);

    // Transition back buffer to render target
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = m_renderTargets[m_frameIndex];
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &barrier);

    // Clear render target
    FLOAT clearColor[4] = { 0.08f, 0.1f, 0.12f, 1.0f };
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += (SIZE_T)m_frameIndex * m_rtvDescriptorSize;
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    std::cout << "D3D12Renderer: recorded frame commands (frame " << m_frameIndex << ")" << std::endl;
#endif
}

void D3D12Renderer::EndFrame() {
#ifdef _WIN32
    if (!m_initialized) return;

    // Transition render target -> present
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = m_renderTargets[m_frameIndex];
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &barrier);

    // Close and execute
    m_commandList->Close();
    ID3D12CommandList* ppLists[] = { m_commandList };
    m_commandQueue->ExecuteCommandLists(1, ppLists);

    // Present
    HRESULT hr = m_swapChain->Present(1, 0);
    if (FAILED(hr)) {
        std::cerr << "D3D12Renderer::EndFrame -> Present failed HRESULT=0x" << std::hex << hr << std::dec << std::endl;
    }

    // Signal and wait for GPU
    const UINT64 fenceToWait = m_fenceValue;
    if (FAILED(m_commandQueue->Signal(m_fence, fenceToWait))) {
        std::cerr << "D3D12Renderer: Signal failed" << std::endl;
    }
    m_fenceValue++;

    if (m_fence->GetCompletedValue() < fenceToWait) {
        m_fence->SetEventOnCompletion(fenceToWait, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    // Advance frame index
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
    std::cout << "D3D12Renderer::EndFrame -> frame presented" << std::endl;
#endif
}

void D3D12Renderer::Shutdown() {
#ifdef _WIN32
    if (!m_initialized) return;
    std::cout << "D3D12Renderer::Shutdown -> enter" << std::endl;

    // Ensure GPU idle
    if (m_commandQueue && m_fence) {
        const UINT64 fenceToWait = m_fenceValue;
        if (SUCCEEDED(m_commandQueue->Signal(m_fence, fenceToWait))) {
            m_fenceValue++;
            if (m_fence->GetCompletedValue() < fenceToWait) {
                m_fence->SetEventOnCompletion(fenceToWait, m_fenceEvent);
                WaitForSingleObject(m_fenceEvent, INFINITE);
            }
        }
    }

    if (m_fenceEvent) { CloseHandle(m_fenceEvent); m_fenceEvent = nullptr; }
    if (m_fence) { m_fence->Release(); m_fence = nullptr; }
    if (m_commandList) { m_commandList->Release(); m_commandList = nullptr; }
    if (m_commandAllocator) { m_commandAllocator->Release(); m_commandAllocator = nullptr; }
    for (UINT i = 0; i < m_frameCount; ++i) {
        if (m_renderTargets[i]) { m_renderTargets[i]->Release(); m_renderTargets[i] = nullptr; }
    }
    if (m_rtvHeap) { m_rtvHeap->Release(); m_rtvHeap = nullptr; }
    if (m_swapChain) { m_swapChain->Release(); m_swapChain = nullptr; }
    if (m_commandQueue) { m_commandQueue->Release(); m_commandQueue = nullptr; }
    if (m_device) { m_device->Release(); m_device = nullptr; }
    if (m_factory) { m_factory->Release(); m_factory = nullptr; }

    m_initialized = false;
    std::cout << "D3D12Renderer: shutdown" << std::endl;
    std::cout << "D3D12Renderer::Shutdown -> exit" << std::endl;
#endif
}

} // namespace Genesis::Engine
