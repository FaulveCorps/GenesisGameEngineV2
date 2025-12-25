#pragma once

#include <SDL.h>
#include <vector>
#include <cstdint>

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
};

} // namespace Genesis::Engine
