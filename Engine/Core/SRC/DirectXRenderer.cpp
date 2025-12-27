#include "engine/DirectXRenderer.h"
#include <iostream>

#ifdef _WIN32
// Include DirectX headers when on Windows; these require Windows SDK availability
#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <SDL_syswm.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#endif

namespace Genesis::Engine {

bool DirectXRenderer::Init(SDL_Window* window, SDL_GLContext /*glContext*/) {
#ifdef _WIN32
    // Create D3D11 device and swap chain using SDL window handle
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(window, &wmInfo)) {
        std::cerr << "DirectXRenderer: SDL_GetWindowWMInfo failed: " << SDL_GetError() << std::endl;
        return false;
    }
    HWND hwnd = wmInfo.info.win.window;

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 0; // use automatic sizing
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        createDeviceFlags, nullptr, 0, D3D11_SDK_VERSION,
        &sd, &m_swapChain, &m_d3dDevice, &featureLevel, &m_d3dContext);
    if (FAILED(hr)) {
        std::cerr << "DirectXRenderer: D3D11CreateDeviceAndSwapChain failed: " << std::hex << hr << std::dec << std::endl;
        return false;
    }

    // Create render target view
    ID3D11Texture2D* pBackBuffer = nullptr;
    hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    if (FAILED(hr) || !pBackBuffer) {
        std::cerr << "DirectXRenderer: GetBuffer failed" << std::endl;
        return false;
    }
    hr = m_d3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_renderTargetView);
    pBackBuffer->Release();
    if (FAILED(hr)) {
        std::cerr << "DirectXRenderer: CreateRenderTargetView failed" << std::endl;
        return false;
    }

    // Simple triangle vertex data (pos, color)
    struct Vertex { float pos[3]; float col[4]; };
    Vertex triVerts[] = {
        {{ 0.0f,  0.8f, 0.0f }, {1,1,1,1}},
        {{-0.8f, -0.8f, 0.0f }, {1,1,1,1}},
        {{ 0.8f, -0.8f, 0.0f }, {1,1,1,1}},
    };

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(triVerts);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = triVerts;
    hr = m_d3dDevice->CreateBuffer(&bd, &initData, &m_vertexBuffer);
    if (FAILED(hr)) {
        std::cerr << "DirectXRenderer: CreateBuffer failed" << std::endl;
        return false;
    }

    // Compile simple shaders
    const char* vsSrc = R"(
        struct VS_IN { float3 pos : POSITION; float4 col : COLOR; };
        struct PS_IN { float4 pos : SV_POSITION; float4 col : COLOR; };
        PS_IN VS(VS_IN input) { PS_IN o; o.pos = float4(input.pos, 1.0); o.col = input.col; return o; }
    )";
    const char* psSrc = R"(
        struct PS_IN { float4 pos : SV_POSITION; float4 col : COLOR; };
        float4 PS(PS_IN input) : SV_TARGET { return input.col; }
    )";

    ID3DBlob* vsBlob = nullptr; ID3DBlob* psBlob = nullptr; ID3DBlob* errBlob = nullptr;
    hr = D3DCompile(vsSrc, strlen(vsSrc), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, &errBlob);
    if (FAILED(hr)) {
        if (errBlob) std::cerr << "Vertex shader compile error: " << (char*)errBlob->GetBufferPointer() << std::endl;
        return false;
    }
    hr = D3DCompile(psSrc, strlen(psSrc), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, &errBlob);
    if (FAILED(hr)) {
        if (errBlob) std::cerr << "Pixel shader compile error: " << (char*)errBlob->GetBufferPointer() << std::endl;
        return false;
    }

    hr = m_d3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vertexShader);
    if (FAILED(hr)) return false;
    hr = m_d3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pixelShader);
    if (FAILED(hr)) return false;

    // Input layout
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    hr = m_d3dDevice->CreateInputLayout(layoutDesc, ARRAYSIZE(layoutDesc), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_inputLayout);
    if (FAILED(hr)) return false;

    if (vsBlob) vsBlob->Release(); if (psBlob) psBlob->Release(); if (errBlob) errBlob->Release();

    m_initialized = true;
    std::cout << "DirectXRenderer: initialized (d3d11)" << std::endl;
    return true;
#else
    std::cerr << "DirectXRenderer: not supported on this platform" << std::endl;
    return false;
#endif
}

void DirectXRenderer::BeginFrame() {
#ifdef _WIN32
    if (!m_initialized) return;
    // Set render target
    m_d3dContext->OMSetRenderTargets(1, &m_renderTargetView, nullptr);
    // Clear to dark gray
    float clearColor[4] = {0.1f, 0.12f, 0.15f, 1.0f};
    m_d3dContext->ClearRenderTargetView(m_renderTargetView, clearColor);

    // Setup IA and shaders
    UINT stride = sizeof(float) * 7; // 3 pos + 4 color
    UINT offset = 0;
    m_d3dContext->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
    m_d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_d3dContext->IASetInputLayout(m_inputLayout);
    m_d3dContext->VSSetShader(m_vertexShader, nullptr, 0);
    m_d3dContext->PSSetShader(m_pixelShader, nullptr, 0);

    m_d3dContext->Draw(3, 0);
    std::cout << "DirectXRenderer: Draw call issued" << std::endl;
#endif
}

void DirectXRenderer::EndFrame() {
#ifdef _WIN32
    if (!m_initialized) return;
    std::cout << "DirectXRenderer::EndFrame -> enter" << std::endl;
    if (m_swapChain) {
        HRESULT hr = m_swapChain->Present(1, 0);
        if (FAILED(hr)) std::cerr << "DirectXRenderer::EndFrame -> Present failed HRESULT=0x" << std::hex << hr << std::dec << std::endl;
    }
    std::cout << "DirectXRenderer::EndFrame -> exit" << std::endl;
#endif
}

void DirectXRenderer::Shutdown() {
#ifdef _WIN32
    if (!m_initialized) return;
    std::cout << "DirectXRenderer::Shutdown -> enter" << std::endl;
    if (m_vertexBuffer) { m_vertexBuffer->Release(); m_vertexBuffer = nullptr; }
    if (m_inputLayout) { m_inputLayout->Release(); m_inputLayout = nullptr; }
    if (m_vertexShader) { m_vertexShader->Release(); m_vertexShader = nullptr; }
    if (m_pixelShader) { m_pixelShader->Release(); m_pixelShader = nullptr; }
    if (m_renderTargetView) { m_renderTargetView->Release(); m_renderTargetView = nullptr; }
    if (m_swapChain) { m_swapChain->Release(); m_swapChain = nullptr; }
    if (m_d3dContext) { m_d3dContext->Release(); m_d3dContext = nullptr; }
    if (m_d3dDevice) { m_d3dDevice->Release(); m_d3dDevice = nullptr; }
    m_initialized = false;
    std::cout << "DirectXRenderer: shutdown" << std::endl;
    std::cout << "DirectXRenderer::Shutdown -> exit" << std::endl;
#endif
}

} // namespace Genesis::Engine
