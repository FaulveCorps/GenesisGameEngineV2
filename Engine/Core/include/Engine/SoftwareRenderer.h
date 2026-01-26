#pragma once

#include "engine/IGraphics.h"
#include <SDL.h>
#include <vector>
#include <cstdint>
#include <unordered_map>

namespace Genesis::Engine {

class Texture;

class SoftwareRenderer : public IGraphicsAPI {
public:
    SoftwareRenderer() = default;
    ~SoftwareRenderer() override = default;

    bool Init(SDL_Window* window, SDL_GLContext glContext) override;
    void BeginFrame() override;
    void EndFrame() override;
    void Shutdown() override;

    std::string GetName() const override { return std::string("software"); }

    // Render to a BGRA8 buffer of size width x height and return it in 'out'
    bool ReadbackOffscreen(uint32_t width, uint32_t height, std::vector<uint8_t>& out);

    // Mesh API
    MeshHandle CreateMesh(const MeshDesc& desc) override;
    void DestroyMesh(const MeshHandle& h) override;
    void DrawMesh(const MeshHandle& h) override;

    // Renderer-managed textures
    TextureHandle CreateTexture(const TextureCreateDesc& desc) override;
    void DestroyTexture(const TextureHandle& h) override;

    // 2D immediate-mode draw
    void DrawTexture(Texture* tex, float x, float y, float w, float h,
                     float u0 = 0.f, float v0 = 0.f, float u1 = 1.f, float v1 = 1.f,
                     uint32_t color = 0xFFFFFFFF) override;

private:
    bool m_initialized = false;
    // Simple software mesh storage: id -> MeshDesc
    std::unordered_map<uint64_t, MeshDesc> m_meshes;
    uint64_t m_drawnMesh = 0;

    struct SWSprite { Texture* tex; float x,y,w,h,u0,v0,u1,v1; uint32_t color; };
    std::vector<SWSprite> m_sprites; // sprites for the current frame

    // Software renderer texture store: id -> pixels + size
    struct SWTexture { uint32_t w=0, h=0; std::vector<uint8_t> pixels; };
    std::unordered_map<uint64_t, SWTexture> m_textures;
};

} // namespace Genesis::Engine
