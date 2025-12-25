#pragma once

#include <vector>
#include <mutex>

namespace Genesis::Engine {
class Mesh;
class IGraphicsAPI;

class MeshRegistry {
public:
    static MeshRegistry& Instance();

    void Register(Mesh* m);
    void Unregister(Mesh* m);

    // Called during renderer switch: destroy per-renderer resources on the specified renderer
    void DestroyAllOnRenderer(IGraphicsAPI* renderer);

    // After setting a new renderer, upload all meshes into the new renderer
    void UploadAllToRenderer(IGraphicsAPI* renderer);

private:
    MeshRegistry() = default;
    std::vector<Mesh*> m_meshes;
    std::mutex m_mutex;
};

} // namespace Genesis::Engine