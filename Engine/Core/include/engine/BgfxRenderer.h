#pragma once

#include "engine/IGraphics.h"

#ifdef _WIN32
struct HWND__;
typedef HWND__* HWND;
#endif

#ifdef HAVE_BGFX
#include <bgfx/bgfx.h>
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

#ifdef HAVE_BGFX
    // bgfx resources for optional GPU triangle smoke test
    bgfx::VertexLayout m_layout;
    bgfx::VertexBufferHandle m_vbh = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle m_ibh = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_program = BGFX_INVALID_HANDLE;
    bool m_hasTriangle = false;
#endif
#endif
};

} // namespace Genesis::Engine
