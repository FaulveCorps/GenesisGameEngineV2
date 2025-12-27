#pragma once

#include <SDL.h>
#include <vector>
#include <cstdint>
#include <string>

namespace Genesis::Engine {

struct MeshHandle {
    uint64_t id = 0;
    bool IsValid() const { return id != 0; }
};

struct MeshDesc {
    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<uint32_t> indices;
};

class Texture;

class IGraphicsAPI {
public:
    virtual ~IGraphicsAPI() = default;

    // Main lifecycle
    virtual bool Init(SDL_Window* window, SDL_GLContext glContext) = 0;
    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;
    virtual void Shutdown() = 0;

    // Optional (default no-op) resource APIs for runtime switching
    virtual MeshHandle CreateMesh(const MeshDesc& /*desc*/) { return MeshHandle{}; }
    virtual void DestroyMesh(const MeshHandle& /*h*/) { }
    virtual void DrawMesh(const MeshHandle& /*h*/) { }

    // Texture handle for renderer-managed textures
    struct TextureHandle {
        uint64_t id = 0; // renderer-specific id (opaque)
        bool IsValid() const { return id != 0; }
    };

    // Optional texture lifecycle hooks (renderer can implement to manage GPU-side resources)
    virtual TextureHandle CreateTexture(uint32_t /*width*/, uint32_t /*height*/, const uint8_t* /*pixels*/) { return TextureHandle{}; }
    virtual void DestroyTexture(const TextureHandle& /*h*/) { }

    // Immediate-mode 2D texture draw (coordinates in pixels, UV in 0..1, color ARGB)
    virtual void DrawTexture(Texture* /*tex*/, float /*x*/, float /*y*/, float /*w*/, float /*h*/,
                             float /*u0*/ = 0.f, float /*v0*/ = 0.f, float /*u1*/ = 1.f, float /*v1*/ = 1.f,
                             uint32_t /*color*/ = 0xFFFFFFFF) { }

    // Human-readable renderer name for UI/debugging
    virtual std::string GetName() const { return std::string("unknown"); }
};

} // namespace Genesis::Engine
