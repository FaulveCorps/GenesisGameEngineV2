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

    // Gather handles to destroy (call without holding the lock)
    std::vector<std::pair<IGraphicsAPI*, IGraphicsAPI::TextureHandle>> toDestroy;

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        // remove from texture list
        m_textures.erase(std::remove(m_textures.begin(), m_textures.end(), t), m_textures.end());

        // Remove any handles associated with this texture across all owners
        for (auto it = m_handlesByOwner.begin(); it != m_handlesByOwner.end();) {
            auto& vec = it->second;
            // Collect handles for this texture
            for (const auto& p : vec) {
                if (p.first == t) {
                    toDestroy.emplace_back(it->first, p.second);
                }
            }
            // Erase entries for this texture
            vec.erase(std::remove_if(vec.begin(), vec.end(), [t](const auto& p){ return p.first == t; }), vec.end());
            if (vec.empty()) it = m_handlesByOwner.erase(it);
            else ++it;
        }
    }

    // Now destroy native handles via their owners (outside lock)
    for (auto& p : toDestroy) {
        if (p.first && p.second.IsValid()) {
            try {
                p.first->DestroyTexture(p.second);
            } catch (...) {
                std::cerr << "TextureRegistry::Unregister -> exception while destroying native texture handle" << std::endl;
            }
        } else {
            // If owner pointer null or invalid, best-effort fallback not attempted here
        }
    }
}

void TextureRegistry::RegisterHandle(Texture* t, IGraphicsAPI* owner, const IGraphicsAPI::TextureHandle& h) {
    if (!t || !owner || !h.IsValid()) return;
    std::lock_guard<std::mutex> lk(m_mutex);
    auto& vec = m_handlesByOwner[owner];
    // avoid duplicates
    for (const auto& p : vec) {
        if (p.first == t && p.second.id == h.id) return;
    }
    vec.emplace_back(t, h);
}

void TextureRegistry::UnregisterHandle(Texture* t, IGraphicsAPI* owner, const IGraphicsAPI::TextureHandle& h) {
    if (!t) return;
    std::lock_guard<std::mutex> lk(m_mutex);
    if (owner) {
        auto it = m_handlesByOwner.find(owner);
        if (it == m_handlesByOwner.end()) return;
        auto& vec = it->second;
        vec.erase(std::remove_if(vec.begin(), vec.end(), [&](const auto& p){
            return p.first == t && (!h.IsValid() || p.second.id == h.id);
        }), vec.end());
        if (vec.empty()) m_handlesByOwner.erase(it);
    } else {
        // remove entries for this texture across all owners (matching handle if provided)
        for (auto it = m_handlesByOwner.begin(); it != m_handlesByOwner.end();) {
            auto& vec = it->second;
            vec.erase(std::remove_if(vec.begin(), vec.end(), [&](const auto& p){
                return p.first == t && (!h.IsValid() || p.second.id == h.id);
            }), vec.end());
            if (vec.empty()) it = m_handlesByOwner.erase(it);
            else ++it;
        }
    }
}

void TextureRegistry::DestroyAllOnRenderer(IGraphicsAPI* renderer) {
    if (!renderer) {
        // Fallback: destroy all handles via per-texture DestroyOnRenderer (no optimization)
        std::vector<Texture*> copy;
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            copy = m_textures;
        }
        for (auto* t : copy) {
            if (t) t->DestroyOnRenderer(nullptr);
        }
        return;
    }

    // Grab list of handles for this owner then release lock to avoid reentrant locking
    std::vector<std::pair<Texture*, IGraphicsAPI::TextureHandle>> toDestroy;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_handlesByOwner.find(renderer);
        if (it != m_handlesByOwner.end()) {
            toDestroy = it->second;
            m_handlesByOwner.erase(it);
        }
    }

    for (auto& p : toDestroy) {
        if (p.first) p.first->DestroyOnRenderer(renderer);
    }
}

void TextureRegistry::UploadAllToRenderer(IGraphicsAPI* renderer) {
    std::vector<Texture*> copy;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        copy = m_textures;
    }
    for (auto* t : copy) {
        if (t) t->UploadToRenderer(renderer);
    }
}

std::vector<Texture*> TextureRegistry::GetAllTextures() {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_textures;
}

} // namespace Genesis::Engine