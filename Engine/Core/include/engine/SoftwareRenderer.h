#pragma once

#include "engine/IGraphics.h"
#include <SDL.h>
#include <vector>
#include <cstdint>
#include <unordered_map>

namespace Genesis::Engine {

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

private:
    bool m_initialized = false;
    // Simple software mesh storage: id -> MeshDesc
    std::unordered_map<uint64_t, MeshDesc> m_meshes;
    uint64_t m_drawnMesh = 0;
};

} // namespace Genesis::Engine
