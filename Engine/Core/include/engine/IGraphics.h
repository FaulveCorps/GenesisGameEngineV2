#pragma once

#include <SDL.h>

namespace Genesis::Engine {

class IGraphicsAPI {
public:
    virtual ~IGraphicsAPI() = default;
    virtual bool Init(SDL_Window* window, SDL_GLContext glContext) = 0;
    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;
    virtual void Shutdown() = 0;
};

} // namespace Genesis::Engine
