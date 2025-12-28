#pragma once

#include "Engine/IGraphics.h"
#include "Engine/Shader.h"
#include "Engine/Texture.h"
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

    // Renderer-managed texture lifecycle
    TextureHandle CreateTexture(uint32_t width, uint32_t height, const uint8_t* pixels) override;
    void DestroyTexture(const TextureHandle& h) override;

    // 2D immediate texture draw
    void DrawTexture(Texture* tex, float x, float y, float w, float h,
                     float u0 = 0.f, float v0 = 0.f, float u1 = 1.f, float v1 = 1.f,
                     uint32_t color = 0xFFFFFFFF) override;

private:
    SDL_Window* m_window = nullptr;
    SDL_GLContext m_context = nullptr;
    std::shared_ptr<Shader> m_defaultShader;

    // Sprite shader & buffers
    std::shared_ptr<Shader> m_spriteShader;
    unsigned int m_spriteVAO = 0;
    unsigned int m_spriteVBO = 0;
    unsigned int m_spriteEBO = 0;

    // Debug draw resources
    unsigned int m_debugVAO = 0;
    unsigned int m_debugVBO = 0;
};

} // namespace Genesis::Engine
