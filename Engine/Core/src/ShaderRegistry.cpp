#include "engine/ShaderRegistry.h"
#include "engine/Shader.h"
#include "engine/IGraphics.h"
#include <algorithm>
#include <iostream>

namespace Genesis::Engine {

ShaderRegistry& ShaderRegistry::Instance() {
    static ShaderRegistry inst;
    return inst;
}

void ShaderRegistry::Register(Shader* s) {
    if (!s) return;
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = std::find(m_shaders.begin(), m_shaders.end(), s);
    if (it == m_shaders.end()) m_shaders.push_back(s);
}

void ShaderRegistry::Unregister(Shader* s) {
    if (!s) return;
    std::lock_guard<std::mutex> lk(m_mutex);
    m_shaders.erase(std::remove(m_shaders.begin(), m_shaders.end(), s), m_shaders.end());
}

void ShaderRegistry::DestroyAllOnRenderer(IGraphicsAPI* renderer) {
    std::lock_guard<std::mutex> lk(m_mutex);
    for (auto* s : m_shaders) {
        if (s) s->DestroyOnRenderer(renderer);
    }
}

void ShaderRegistry::UploadAllToRenderer(IGraphicsAPI* renderer) {
    std::lock_guard<std::mutex> lk(m_mutex);
    for (auto* s : m_shaders) {
        if (s) s->UploadToRenderer(renderer);
    }
}

} // namespace Genesis::Engine