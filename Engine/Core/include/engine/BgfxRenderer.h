#pragma once

#include "engine/IGraphics.h"

#ifdef _WIN32
struct HWND__;
typedef HWND__* HWND;
#endif

namespace Genesis::Engine {

class BgfxRenderer : public IGraphicsAPI {
public:
    BgfxRenderer() = default;
    ~BgfxRenderer() override = default;

    bool Init(SDL_Window* window, SDL_GLContext glContext) override;
    void BeginFrame() override;
    void EndFrame() override;
    void Shutdown() override;

private:
#ifdef _WIN32
    bool m_initialized = false;
    HWND m_hwnd = nullptr;
#endif
};

} // namespace Genesis::Engine
