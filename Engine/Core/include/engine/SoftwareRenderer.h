#pragma once

#include "engine/IGraphics.h"
#include <SDL.h>
#include <vector>
#include <cstdint>

namespace Genesis::Engine {

class SoftwareRenderer : public IGraphicsAPI {
public:
    SoftwareRenderer() = default;
    ~SoftwareRenderer() override = default;

    bool Init(SDL_Window* window, SDL_GLContext glContext) override;
    void BeginFrame() override;
    void EndFrame() override;
    void Shutdown() override;

    // Render to a BGRA8 buffer of size width x height and return it in 'out'
    bool ReadbackOffscreen(uint32_t width, uint32_t height, std::vector<uint8_t>& out);

private:
    bool m_initialized = false;
};

} // namespace Genesis::Engine
