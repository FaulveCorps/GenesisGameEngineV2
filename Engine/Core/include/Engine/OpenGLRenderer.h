#pragma once

#include "Engine/IGraphics.h"
#include "Engine/Shader.h"
#include "Engine/Texture.h"
#include <memory>
#include <unordered_map>

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

    // Mesh API
    MeshHandle CreateMesh(const MeshDesc& desc) override;
    void DestroyMesh(const MeshHandle& h) override;
    void DrawMesh(const MeshHandle& h) override;
    void DrawMesh(const MeshHandle& h, Material* material, const float* transform) override;

    void SetGlobalLight(const float direction[3], const float color[3], float intensity) override;

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

    // Simple PBR shader (prototype)
    std::shared_ptr<Shader> m_pbrShader;

    // Debug draw resources
    unsigned int m_debugVAO = 0;
    unsigned int m_debugVBO = 0;

    struct GLMesh {
        unsigned int vao = 0;
        unsigned int vbo = 0;
        unsigned int ebo = 0;
        size_t indexCount = 0;
        unsigned int indexType = 0; // GL_UNSIGNED_INT or GL_UNSIGNED_SHORT
    };
    std::unordered_map<uint64_t, GLMesh> m_meshes;
    uint64_t m_nextMeshId = 1;

    // Global light data
    float m_lightDir[3] = {0.5f, 0.5f, 0.8f};
    float m_lightColor[3] = {1.0f, 1.0f, 1.0f};
    float m_lightIntensity = 1.0f;
};

} // namespace Genesis::Engine
