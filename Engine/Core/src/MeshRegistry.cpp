#include "engine/MeshRegistry.h"
#include "engine/Mesh.h"
#include "engine/IGraphics.h"
#include <algorithm>
#include <iostream>

namespace Genesis::Engine {

MeshRegistry& MeshRegistry::Instance() {
    static MeshRegistry inst;
    return inst;
}

void MeshRegistry::Register(Mesh* m) {
    if (!m) return;
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = std::find(m_meshes.begin(), m_meshes.end(), m);
    if (it == m_meshes.end()) m_meshes.push_back(m);
}

void MeshRegistry::Unregister(Mesh* m) {
    if (!m) return;
    std::lock_guard<std::mutex> lk(m_mutex);
    m_meshes.erase(std::remove(m_meshes.begin(), m_meshes.end(), m), m_meshes.end());
}

void MeshRegistry::DestroyAllOnRenderer(IGraphicsAPI* renderer) {
    std::lock_guard<std::mutex> lk(m_mutex);
    for (Mesh* m : m_meshes) {
        if (m) m->DestroyOnRenderer(renderer);
    }
}

void MeshRegistry::UploadAllToRenderer(IGraphicsAPI* renderer) {
    std::lock_guard<std::mutex> lk(m_mutex);
    for (Mesh* m : m_meshes) {
        if (m) m->UploadToRenderer(renderer);
    }
}

} // namespace Genesis::Engine