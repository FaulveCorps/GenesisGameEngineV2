#include "engine/TextureRegistry.h"
#include "engine/Texture.h"
#include "engine/IGraphics.h"
#include <algorithm>
#include <iostream>

namespace Genesis::Engine {

TextureRegistry& TextureRegistry::Instance() {
    static TextureRegistry inst;
    return inst;
}

void TextureRegistry::Register(Texture* t) {
    if (!t) return;
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = std::find(m_textures.begin(), m_textures.end(), t);
    if (it == m_textures.end()) m_textures.push_back(t);
}

void TextureRegistry::Unregister(Texture* t) {
    if (!t) return;
    std::lock_guard<std::mutex> lk(m_mutex);
    m_textures.erase(std::remove(m_textures.begin(), m_textures.end(), t), m_textures.end());
}

void TextureRegistry::DestroyAllOnRenderer(IGraphicsAPI* renderer) {
    std::lock_guard<std::mutex> lk(m_mutex);
    for (auto* t : m_textures) {
        if (t) t->DestroyOnRenderer(renderer);
    }
}

void TextureRegistry::UploadAllToRenderer(IGraphicsAPI* renderer) {
    std::lock_guard<std::mutex> lk(m_mutex);
    for (auto* t : m_textures) {
        if (t) t->UploadToRenderer(renderer);
    }
}

} // namespace Genesis::Engine