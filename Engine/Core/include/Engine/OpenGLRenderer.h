#pragma once

#include "Engine/IGraphics.h"
#include "Engine/Shader.h"
#include "Engine/Texture.h"
#include <memory>
#include <unordered_map>
#include <vector>

namespace Genesis::Engine {

struct DrawCommand {
    MeshHandle mesh;
    Material* material;
    float transform[16];
};

class OpenGLRenderer : public IGraphicsAPI {
public:
    OpenGLRenderer() = default;
    ~OpenGLRenderer() override = default;

    bool Init(SDL_Window* window, SDL_GLContext glContext) override;
    void BeginFrame() override;
    void EndFrame() override;
    void Present() override;
    void Shutdown() override;

    std::string GetName() const override { return std::string("opengl"); }

    // Mesh API
    MeshHandle CreateMesh(const MeshDesc& desc) override;
    void DestroyMesh(const MeshHandle& h) override;
    void DrawMesh(const MeshHandle& h) override;
    void DrawMesh(const MeshHandle& h, Material* material, const float* transform) override;

    void SetGlobalLight(const float direction[3], const float color[3], float intensity) override;
    void AddPointLight(const PointLightData& light) override;
    void ClearPointLights() override;
    void SetPostProcessParams(float exposure, float gamma) override;
    void SetPostProcessBloom(bool enabled) override;
    void SetPostProcessBloomThreshold(float threshold) override;
    void SetPostProcessVignette(bool enabled, float intensity, float radius, float softness) override;
    void SetPostProcessLUT(Texture* texture, bool enabled, float intensity) override;
    void SetViewProjection(const float* view, const float* projection) override;

    // Renderer-managed texture lifecycle
    TextureHandle CreateTexture(const TextureCreateDesc& desc) override;
    void DestroyTexture(const TextureHandle& h) override;

    // 2D immediate texture draw
    void DrawTexture(Texture* tex, float x, float y, float w, float h,
                     float u0 = 0.f, float v0 = 0.f, float u1 = 1.f, float v1 = 1.f,
                     uint32_t color = 0xFFFFFFFF) override;

    void SetPresentEnabled(bool enabled) { m_presentEnabled = enabled; }
    
    // Returns the texture ID of the final rendered frame (for Editor Viewport)
    // If 0, the frame was rendered to the default framebuffer.
    uint64_t GetFinalTextureID() const { return (uint64_t)m_finalTexture; }

    // Sample depth from the scene G-Buffer at normalized viewport coords (u,v in [0,1]).
    // Returns true if a depth sample was retrieved.
    bool SampleSceneDepth(float u, float v, float& outDepth);

    void BindDefaultFramebuffer();
    void Clear(float r, float g, float b, float a);

private:
    bool m_presentEnabled = true;
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
    std::vector<PointLightData> m_pointLights;

    // Camera data
    float m_view[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };
    float m_projection[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };

    // Post-processing params
    float m_exposure = 1.0f;
    float m_gamma = 2.2f;
    float m_bloomThreshold = 1.0f;
    bool m_vignetteEnabled = false;
    float m_vignetteIntensity = 0.35f;
    float m_vignetteRadius = 0.75f;
    float m_vignetteSoftness = 0.25f;
    bool m_lutEnabled = false;
    float m_lutIntensity = 1.0f;
    float m_lutSize = 16.0f;
    Texture* m_lutTexture = nullptr;

    // Post-processing resources
    unsigned int m_fbo = 0;
    unsigned int m_screenTexture = 0;
    unsigned int m_rbo = 0;
    unsigned int m_screenQuadVAO = 0;
    unsigned int m_screenQuadVBO = 0;
    std::shared_ptr<Shader> m_postProcessShader;
    int m_screenWidth = 0;
    int m_screenHeight = 0;

    // Final Output FBO (for Editor Viewport)
    unsigned int m_finalFBO = 0;
    unsigned int m_finalTexture = 0;

    // Bloom resources
    unsigned int m_brightTexture = 0;
    unsigned int m_pingPongFBO[2] = {0, 0};
    unsigned int m_pingPongTexture[2] = {0, 0};
    std::shared_ptr<Shader> m_blurShader;
    bool m_bloom = true;

    // G-Buffer resources (Deferred Rendering)
    unsigned int m_gBuffer = 0;
    unsigned int m_gPosition = 0;
    unsigned int m_gNormal = 0;
    unsigned int m_gAlbedoSpec = 0;
    unsigned int m_gDepthRBO = 0;
    std::shared_ptr<Shader> m_gBufferShader;
    std::shared_ptr<Shader> m_deferredLightingShader;
    
    void InitGBuffer(int width, int height);
    void ResizeGBuffer(int width, int height);

    void InitPostProcessing(int width, int height);
    void ResizePostProcessing(int width, int height);

    // Shadow Mapping resources
    unsigned int m_shadowMapFBO = 0;
    unsigned int m_shadowMapTexture = 0;
    std::shared_ptr<Shader> m_shadowShader;
    const unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;
    float m_lightSpaceMatrix[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    }; // To pass to PBR shader

    void InitShadowMap();
    void RenderShadowPass();
    void ExecuteDraw(const DrawCommand& cmd, Shader* overrideShader = nullptr);

    struct TextureDrawCommand {
        Texture* texture;
        float x, y, w, h;
        float u0, v0, u1, v1;
        uint32_t color;
    };
    std::vector<TextureDrawCommand> m_textureDrawQueue;
    void ExecuteDrawTexture(const TextureDrawCommand& cmd);

    std::vector<DrawCommand> m_drawQueue;
};

} // namespace Genesis::Engine
