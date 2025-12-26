#pragma once

#include "engine/IGraphics.h"

#ifdef _WIN32
struct HWND__;
typedef HWND__* HWND;
#endif

/* bgfx header removed: bgfx is no longer a supported backend */

namespace Genesis::Engine {

// BgfxRenderer removed: placeholder to avoid refactor churn
class BgfxRenderer : public IGraphicsAPI {
public:
    BgfxRenderer() = default;
    ~BgfxRenderer() override = default;

    bool Init(SDL_Window* /*window*/, SDL_GLContext /*glContext*/) override { return false; }
    void BeginFrame() override {}
    void EndFrame() override {}
    void Shutdown() override {}

private:
    bool m_initialized = false;
};

} // namespace Genesis::Engine
