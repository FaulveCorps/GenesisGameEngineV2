#pragma once

#include "engine/IGraphics.h"
#include <string>

#ifdef _WIN32
// Forward declarations for D3D11 COM interfaces in the global namespace (avoid including heavy headers in header)
struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;
struct ID3D11RenderTargetView;
struct ID3D11Buffer;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11InputLayout;
#endif

namespace Genesis::Engine {

class DirectXRenderer : public IGraphicsAPI {
public:
    DirectXRenderer() = default;
    ~DirectXRenderer() override = default;

    bool Init(SDL_Window* window, SDL_GLContext glContext) override;
    void BeginFrame() override;
    void EndFrame() override;
    void Shutdown() override;

    std::string GetName() const override { return std::string("directx"); }

private:
    // Platform-specific members (D3D11)
#ifdef _WIN32
    bool m_initialized = false;
    // D3D11 core objects
    ID3D11Device* m_d3dDevice = nullptr;
    ID3D11DeviceContext* m_d3dContext = nullptr;
    IDXGISwapChain* m_swapChain = nullptr;
    ID3D11RenderTargetView* m_renderTargetView = nullptr;

    // Simple triangle resources
    ID3D11Buffer* m_vertexBuffer = nullptr;
    ID3D11VertexShader* m_vertexShader = nullptr;
    ID3D11PixelShader* m_pixelShader = nullptr;
    ID3D11InputLayout* m_inputLayout = nullptr;
#else
    bool m_initialized = false;
#endif
};

} // namespace Genesis::Engine
