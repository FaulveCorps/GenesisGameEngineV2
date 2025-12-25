#pragma once

#include "engine/IGraphics.h"
#include "engine/Shader.h"
#include <memory>

namespace Genesis::Engine {

class OpenGLRenderer : public IGraphicsAPI {
public:
    OpenGLRenderer() = default;
    ~OpenGLRenderer() override = default;

    bool Init(SDL_Window* window, SDL_GLContext glContext) override;
    void BeginFrame() override;
    void EndFrame() override;
    void Shutdown() override;

    std::string GetName() const override { return std::string("opengl"); }

private:
    SDL_Window* m_window = nullptr;
    SDL_GLContext m_context = nullptr;
    std::shared_ptr<Shader> m_defaultShader;

    // Debug draw resources
    unsigned int m_debugVAO = 0;
    unsigned int m_debugVBO = 0;
};

} // namespace Genesis::Engine
