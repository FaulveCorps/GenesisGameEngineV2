#pragma once

#include "engine/IGraphics.h"

namespace Genesis::Engine {

class DirectXRenderer : public IGraphicsAPI {
public:
    DirectXRenderer() = default;
    ~DirectXRenderer() override = default;

    bool Init(SDL_Window* window, SDL_GLContext glContext) override;
    void BeginFrame() override;
    void EndFrame() override;
    void Shutdown() override;

private:
    // Platform-specific members will be added in future (ID3D11Device*, IDXGISwapChain*, etc.)
    bool m_initialized = false;
};

} // namespace Genesis::Engine
