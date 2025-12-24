#include "engine/D3D12Renderer.h"
#include <iostream>
#include <SDL_syswm.h>

#ifdef _WIN32
#include <Windows.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
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
    // Save for viewport queries
    m_hwnd = hwnd;

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

    // Create swapchain using modern CreateSwapChainForHwnd (DXGI 1.4)
    DXGI_SWAP_CHAIN_DESC1 sd1 = {};
    sd1.Width = 0; // automatic sizing
    sd1.Height = 0;
    sd1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd1.Stereo = FALSE;
    sd1.SampleDesc.Count = 1;
    sd1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd1.BufferCount = m_frameCount;
    sd1.Scaling = DXGI_SCALING_NONE;
    sd1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    IDXGISwapChain1* tmpSwap1 = nullptr;
    HRESULT hr = m_factory->CreateSwapChainForHwnd(m_commandQueue, hwnd, &sd1, nullptr, nullptr, &tmpSwap1);
    if (FAILED(hr) || !tmpSwap1) {
        std::cerr << "D3D12Renderer: CreateSwapChainForHwnd failed: HRESULT=0x" << std::hex << hr << std::dec << std::endl;
        return false;
    }

    if (FAILED(tmpSwap1->QueryInterface(IID_PPV_ARGS(&m_swapChain)))) {
        std::cerr << "D3D12Renderer: QueryInterface for IDXGISwapChain3 failed" << std::endl;
        tmpSwap1->Release();
        return false;
    }
    tmpSwap1->Release();

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

    // Create fence for sync (needed for initial uploads)
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

    // Create a simple root signature (empty, allowing IA input layout)
    D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
    rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ID3DBlob* rsBlob = nullptr; ID3DBlob* errBlob = nullptr;
    if (FAILED(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rsBlob, &errBlob))) {
        if (errBlob) std::cerr << "D3D12Renderer: Root signature serialize error: " << (char*)errBlob->GetBufferPointer() << std::endl;
        return false;
    }
    if (FAILED(m_device->CreateRootSignature(0, rsBlob->GetBufferPointer(), rsBlob->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)))) {
        std::cerr << "D3D12Renderer: CreateRootSignature failed" << std::endl;
        if (rsBlob) rsBlob->Release(); if (errBlob) errBlob->Release();
        return false;
    }
    if (rsBlob) rsBlob->Release(); if (errBlob) errBlob->Release();

    // Compile simple vertex/pixel shaders
    const char* vsSrc = R"(
        struct VS_IN { float3 pos : POSITION; float4 col : COLOR; };
        struct PS_IN { float4 pos : SV_POSITION; float4 col : COLOR; };
        PS_IN VS(VS_IN input) { PS_IN o; o.pos = float4(input.pos, 1.0); o.col = input.col; return o; }
    )";
    const char* psSrc = R"(
        struct PS_IN { float4 pos : SV_POSITION; float4 col : COLOR; };
        float4 PS(PS_IN input) : SV_TARGET { return input.col; }
    )";
    ID3DBlob* vsBlob = nullptr; ID3DBlob* psBlob = nullptr; ID3DBlob* shaderErr = nullptr;
    if (FAILED(D3DCompile(vsSrc, strlen(vsSrc), nullptr, nullptr, nullptr, "VS", "vs_5_0", 0, 0, &vsBlob, &shaderErr))) {
        if (shaderErr) std::cerr << "D3D12Renderer: VS compile error: " << (char*)shaderErr->GetBufferPointer() << std::endl;
        return false;
    }
    if (FAILED(D3DCompile(psSrc, strlen(psSrc), nullptr, nullptr, nullptr, "PS", "ps_5_0", 0, 0, &psBlob, &shaderErr))) {
        if (shaderErr) std::cerr << "D3D12Renderer: PS compile error: " << (char*)shaderErr->GetBufferPointer() << std::endl;
        if (vsBlob) vsBlob->Release();
        return false;
    }

    // Create PSO
    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = m_rootSignature;
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.NumRenderTargets = 1;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };

    // Minimal rasterizer/blend/depth state defaults
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;
    psoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;

    HRESULT psoHr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState));
    if (FAILED(psoHr)) {
        std::cerr << "D3D12Renderer: CreateGraphicsPipelineState failed: HRESULT=0x" << std::hex << psoHr << std::dec << std::endl;
        if (vsBlob) vsBlob->Release(); if (psBlob) psBlob->Release();
        return false;
    }
    if (vsBlob) vsBlob->Release(); if (psBlob) psBlob->Release();

    // Simple triangle vertex data
    struct Vertex { float pos[3]; float col[4]; };
    Vertex triVerts[] = {
        {{ 0.0f,  0.8f, 0.0f }, {1,1,1,1}},
        {{-0.8f, -0.8f, 0.0f }, {1,1,1,1}},
        {{ 0.8f, -0.8f, 0.0f }, {1,1,1,1}},
    };
    const UINT vbSize = sizeof(triVerts);

    // Create default heap for VB
    D3D12_HEAP_PROPERTIES heapDefault = {};
    heapDefault.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Alignment = 0;
    bufDesc.Width = vbSize;
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.SampleDesc.Quality = 0;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    if (FAILED(m_device->CreateCommittedResource(&heapDefault, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_vertexBuffer)))) {
        std::cerr << "D3D12Renderer: CreateCommittedResource (VB default) failed" << std::endl;
        return false;
    }

    // Create upload heap
    D3D12_HEAP_PROPERTIES heapUpload = {};
    heapUpload.Type = D3D12_HEAP_TYPE_UPLOAD;
    if (FAILED(m_device->CreateCommittedResource(&heapUpload, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_vbUpload)))) {
        std::cerr << "D3D12Renderer: CreateCommittedResource (VB upload) failed" << std::endl;
        return false;
    }

    // Copy data to upload heap
    UINT8* pData = nullptr;
    D3D12_RANGE range = {0, 0};
    if (FAILED(m_vbUpload->Map(0, &range, reinterpret_cast<void**>(&pData)))) {
        std::cerr << "D3D12Renderer: VB upload map failed" << std::endl;
        return false;
    }
    memcpy(pData, triVerts, vbSize);
    m_vbUpload->Unmap(0, nullptr);

    // Use command list to copy from upload -> default
    m_commandAllocator->Reset();
    m_commandList->Reset(m_commandAllocator, nullptr);
    m_commandList->CopyBufferRegion(m_vertexBuffer, 0, m_vbUpload, 0, vbSize);

    // Transition VB to VERTEX_AND_CONSTANT_BUFFER
    D3D12_RESOURCE_BARRIER vbBarrier = {};
    vbBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    vbBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    vbBarrier.Transition.pResource = m_vertexBuffer;
    vbBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    vbBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    vbBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &vbBarrier);

    // Close and execute upload command list
    m_commandList->Close();
    ID3D12CommandList* ppLists[] = { m_commandList };
    m_commandQueue->ExecuteCommandLists(1, ppLists);

    // Signal and wait
    const UINT64 fenceToWait = m_fenceValue;
    if (FAILED(m_commandQueue->Signal(m_fence, fenceToWait))) {
        std::cerr << "D3D12Renderer: Signal failed during VB upload" << std::endl;
        return false;
    }
    m_fenceValue++;
    if (m_fence->GetCompletedValue() < fenceToWait) {
        m_fence->SetEventOnCompletion(fenceToWait, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    // Prepare VB view
    m_vbv.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vbv.SizeInBytes = vbSize;
    m_vbv.StrideInBytes = sizeof(Vertex);

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

    // Set render target for OM
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // Set viewport/scissor from window size (if available)
    if (m_hwnd) {
        RECT rc; GetClientRect(m_hwnd, &rc);
        D3D12_VIEWPORT vp = { 0.0f, 0.0f, (FLOAT)(rc.right - rc.left), (FLOAT)(rc.bottom - rc.top), 0.0f, 1.0f };
        D3D12_RECT sc = { 0, 0, rc.right - rc.left, rc.bottom - rc.top };
        m_commandList->RSSetViewports(1, &vp);
        m_commandList->RSSetScissorRects(1, &sc);
    }

    // Issue a simple draw using the created pipeline and vertex buffer
    if (m_rootSignature && m_pipelineState && m_vertexBuffer) {
        m_commandList->SetGraphicsRootSignature(m_rootSignature);
        m_commandList->SetPipelineState(m_pipelineState);
        m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_commandList->IASetVertexBuffers(0, 1, &m_vbv);
        m_commandList->DrawInstanced(3, 1, 0, 0);
    }

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

    // Release D3D12 pipeline and buffers
    if (m_pipelineState) { m_pipelineState->Release(); m_pipelineState = nullptr; }
    if (m_rootSignature) { m_rootSignature->Release(); m_rootSignature = nullptr; }
    if (m_vertexBuffer) { m_vertexBuffer->Release(); m_vertexBuffer = nullptr; }
    if (m_vbUpload) { m_vbUpload->Release(); m_vbUpload = nullptr; }

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
