#include "Engine/Texture.h"
#include "Engine/AssetDatabase.h"
#include "Engine/TextureRegistry.h"
#include "Engine/IGraphics.h"
#include "Engine/RendererManager.h"
#include <SDL.h>
#include <iostream>
#include <cstring>
#include <algorithm>
#include <filesystem>
#ifdef HAVE_STB_IMAGE
#include <stb_image.h>
#endif

namespace Genesis::Engine {

#ifdef _WIN32
#define APIENTRY __stdcall
#endif

using PFNGLGENTEXTURESPROC = void (APIENTRY*)(int, unsigned int*);
using PFNGLBINDTEXTUREPROC = void (APIENTRY*)(unsigned int, unsigned int);
using PFNGLTEXPARAMETERIPROC = void (APIENTRY*)(unsigned int, int, int);
using PFNGLTEXPARAMETERFPROC = void (APIENTRY*)(unsigned int, int, float);
using PFNGLTEXIMAGE2DPROC = void (APIENTRY*)(unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void*);
using PFNGLGENERATEMIPMAPPROC = void (APIENTRY*)(unsigned int);
using PFNGLDELETETEXTURESPROC = void (APIENTRY*)(int, const unsigned int*);

static PFNGLGENTEXTURESPROC pglGenTextures = nullptr;
static PFNGLBINDTEXTUREPROC pglBindTexture = nullptr;
static PFNGLTEXPARAMETERIPROC pglTexParameteri = nullptr;
static PFNGLTEXPARAMETERFPROC pglTexParameterf = nullptr;
static PFNGLTEXIMAGE2DPROC pglTexImage2D = nullptr;
static PFNGLGENERATEMIPMAPPROC pglGenerateMipmap = nullptr;
static PFNGLDELETETEXTURESPROC pglDeleteTextures = nullptr;

static bool ResolveGL(void** fnPtr, const char* name) {
    if (*fnPtr) return true;
    auto addr = (void*)SDL_GL_GetProcAddress(name);
    if (!addr) return false;
    *fnPtr = addr;
    return true;
}

// GL enums used (avoids requiring gl.h)
#define GL_TEXTURE_2D        0x0DE1
#define GL_RGBA              0x1908
#define GL_UNSIGNED_BYTE     0x1401
#define GL_NEAREST           0x2600
#define GL_LINEAR            0x2601
#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_TEXTURE_WRAP_S    0x2802
#define GL_TEXTURE_WRAP_T    0x2803
#define GL_REPEAT            0x2901
#define GL_CLAMP_TO_EDGE     0x812F
#define GL_MIRRORED_REPEAT   0x8370
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_SRGB8_ALPHA8      0x8C43

static bool ParseBoolSetting(const std::string& value, bool fallback) {
    if (value.empty()) return fallback;
    std::string v = value;
    std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    if (v == "1" || v == "true" || v == "yes" || v == "on") return true;
    if (v == "0" || v == "false" || v == "no" || v == "off") return false;
    return fallback;
}

static TextureWrap ParseWrapSetting(const std::string& value, TextureWrap fallback) {
    std::string v = value;
    std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    if (v == "repeat") return TextureWrap::Repeat;
    if (v == "clamp" || v == "clamp_to_edge") return TextureWrap::Clamp;
    if (v == "mirror" || v == "mirrored") return TextureWrap::Mirror;
    return fallback;
}

static TextureFilter ParseFilterSetting(const std::string& value, TextureFilter fallback) {
    std::string v = value;
    std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    if (v == "nearest") return TextureFilter::Nearest;
    if (v == "linear") return TextureFilter::Linear;
    if (v == "anisotropic" || v == "aniso") return TextureFilter::Anisotropic;
    return fallback;
}

static TextureSettings ReadSettingsFromMeta(const std::string& path) {
    TextureSettings settings;
    AssetMeta meta;
    if (!AssetDatabase::LoadMeta(path, meta)) {
        return settings;
    }

    std::string value;
    if (AssetDatabase::GetImportSetting(meta, "srgb", value)) {
        settings.srgb = ParseBoolSetting(value, settings.srgb);
    }
    if (AssetDatabase::GetImportSetting(meta, "mipmaps", value)) {
        settings.mipmaps = ParseBoolSetting(value, settings.mipmaps);
    }
    if (AssetDatabase::GetImportSetting(meta, "normal_map", value)) {
        settings.normalMap = ParseBoolSetting(value, settings.normalMap);
        if (settings.normalMap && !AssetDatabase::GetImportSetting(meta, "srgb", value)) {
            settings.srgb = false;
        }
    }
    if (AssetDatabase::GetImportSetting(meta, "wrap", value)) {
        settings.wrap = ParseWrapSetting(value, settings.wrap);
    }
    if (AssetDatabase::GetImportSetting(meta, "filter", value)) {
        settings.filter = ParseFilterSetting(value, settings.filter);
    }

    return settings;
}

static void EnsureNormalMapMeta(const std::string& path) {
    AssetMeta meta;
    bool hasMeta = AssetDatabase::LoadMeta(path, meta);
    if (!hasMeta) {
        meta = AssetDatabase::EnsureMeta(path, std::filesystem::current_path());
    }

    std::string value;
    bool changed = false;
    if (!AssetDatabase::GetImportSetting(meta, "normal_map", value) || value != "1") {
        AssetDatabase::SetImportSetting(meta, "normal_map", "1");
        changed = true;
    }
    if (!AssetDatabase::GetImportSetting(meta, "srgb", value) || value != "0") {
        AssetDatabase::SetImportSetting(meta, "srgb", "0");
        changed = true;
    }
    if (changed) {
        AssetDatabase::SaveMeta(path, meta);
    }
}

static std::string NormalizeTexturePath(const std::string& path) {
    std::error_code ec;
    auto canon = std::filesystem::weakly_canonical(path, ec);
    if (!ec) return canon.string();
    return path;
}

static bool LoadPixelsFromFile(const std::string& path, uint32_t& outW, uint32_t& outH, std::vector<uint8_t>& outPixels) {
#ifdef HAVE_STB_IMAGE
    int x = 0, y = 0, n = 0;
    unsigned char* data = stbi_load(path.c_str(), &x, &y, &n, 4);
    if (data) {
        size_t sz = static_cast<size_t>(x) * y * 4;
        outPixels.resize(sz);
        std::memcpy(outPixels.data(), data, sz);
        stbi_image_free(data);
        outW = static_cast<uint32_t>(x);
        outH = static_cast<uint32_t>(y);
        return true;
    }
#endif

    SDL_Surface* surf = SDL_LoadBMP(path.c_str());
    if (!surf) {
        return false;
    }
    SDL_Surface* conv = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(surf);
    if (!conv) {
        return false;
    }
    outW = static_cast<uint32_t>(conv->w);
    outH = static_cast<uint32_t>(conv->h);
    outPixels.resize(static_cast<size_t>(outW) * outH * 4);
    std::memcpy(outPixels.data(), conv->pixels, outPixels.size());
    SDL_DestroySurface(conv);
    return true;
}

std::shared_ptr<Texture> Texture::CreateFromMemory(uint32_t width, uint32_t height, const std::vector<uint8_t>& pixels) {
    // Use direct new here so private constructor access is honored
    auto t = std::shared_ptr<Texture>(new Texture());
    t->width_ = width;
    t->height_ = height;
    t->pixels_ = pixels;
    TextureRegistry::Instance().Register(t.get());
    return t;
}

std::shared_ptr<Texture> Texture::CreateFromFile(const std::string& path) {
    TextureSettings settings = ReadSettingsFromMeta(path);
    uint32_t w = 0, h = 0;
    std::vector<uint8_t> pixels;
    if (!LoadPixelsFromFile(path, w, h, pixels)) {
        std::cerr << "Texture::CreateFromFile -> failed for '" << path << "': " << SDL_GetError() << std::endl;
        return nullptr;
    }

    auto t = std::shared_ptr<Texture>(new Texture());
    t->width_ = w;
    t->height_ = h;
    t->pixels_ = std::move(pixels);
    t->settings_ = settings;
    t->sourcePath_ = NormalizeTexturePath(path);
    TextureRegistry::Instance().Register(t.get());
    return t;
} 

std::shared_ptr<Texture> Texture::CreateFromFileAsNormalMap(const std::string& path) {
    EnsureNormalMapMeta(path);
    return CreateFromFile(path);
}

bool Texture::ReloadFromFile() {
    if (sourcePath_.empty()) return false;

    TextureSettings settings = ReadSettingsFromMeta(sourcePath_);
    uint32_t w = 0, h = 0;
    std::vector<uint8_t> pixels;
    if (!LoadPixelsFromFile(sourcePath_, w, h, pixels)) {
        std::cerr << "Texture::ReloadFromFile -> failed for '" << sourcePath_ << "': " << SDL_GetError() << std::endl;
        return false;
    }

    DestroyOnRenderer(nullptr);
    width_ = w;
    height_ = h;
    pixels_ = std::move(pixels);
    settings_ = settings;
    return true;
}

Texture::~Texture() {
    TextureRegistry::Instance().Unregister(this);
    DestroyOnRenderer(nullptr);
}

void Texture::UploadToRenderer(IGraphicsAPI* renderer) {
    // If already uploaded to this renderer, nothing to do
    if (renderer && rendererHandle_.IsValid() && rendererOwnerPtr_ == renderer) return;
    if (pixels_.empty() || width_ == 0 || height_ == 0) return;

    // If a renderer is provided, ask it to create a texture handle
    if (renderer) {
        // If we have an existing handle, try to destroy it via its owner first
        if (rendererHandle_.IsValid()) {
            if (rendererOwnerPtr_) {
                if (rendererOwnerPtr_ == renderer) {
                    rendererOwnerPtr_->DestroyTexture(rendererHandle_);
                } else {
                    // Best-effort: if the owner is still available use it
                    if (auto owner = RendererManager::GetRenderer()) {
                        if (owner == rendererOwnerPtr_) owner->DestroyTexture(rendererHandle_);
                    }
                }
                // Unregister the old handle
                TextureRegistry::Instance().UnregisterHandle(this, rendererOwnerPtr_, rendererHandle_);
            }
            rendererHandle_ = {};
            rendererOwner_.clear();
            rendererOwnerPtr_ = nullptr;
            textureID_ = 0;
        }

        IGraphicsAPI::TextureCreateDesc desc{};
        desc.width = width_;
        desc.height = height_;
        desc.pixels = pixels_.empty() ? nullptr : pixels_.data();
        desc.srgb = settings_.srgb;
        desc.mipmaps = settings_.mipmaps;
        desc.wrap = settings_.wrap;
        desc.filter = settings_.filter;

        auto h = renderer->CreateTexture(desc);
        if (h.IsValid()) {
            rendererHandle_ = h;
            rendererOwner_ = renderer->GetName();
            rendererOwnerPtr_ = renderer;
            TextureRegistry::Instance().RegisterHandle(this, renderer, h);
            // Maintain legacy GL texture id for compatibility if this is OpenGL
            if (rendererOwner_ == std::string("opengl")) textureID_ = static_cast<unsigned int>(rendererHandle_.id);
            else textureID_ = 0; // clear any leftover legacy GL id
            return;
        }
        // If renderer did not create a handle and it is not OpenGL, just skip
        return;
    }

    // If renderer==nullptr, try to use the currently-installed renderer, or fall back to legacy GL path
    if (auto cur = RendererManager::GetRenderer()) {
        return UploadToRenderer(cur);
    }

    // Legacy path: create a GL texture if a GL context exists
    if (!SDL_GL_GetCurrentContext()) return;
    if (textureID_ != 0) return; // already uploaded

    ResolveGL((void**)&pglGenTextures, "glGenTextures");
    ResolveGL((void**)&pglBindTexture, "glBindTexture");
    ResolveGL((void**)&pglTexParameteri, "glTexParameteri");
    ResolveGL((void**)&pglTexParameterf, "glTexParameterf");
    ResolveGL((void**)&pglTexImage2D, "glTexImage2D");
    ResolveGL((void**)&pglGenerateMipmap, "glGenerateMipmap");
    ResolveGL((void**)&pglDeleteTextures, "glDeleteTextures");

    if (!pglGenTextures || !pglBindTexture || !pglTexParameteri || !pglTexImage2D || !pglDeleteTextures) {
        std::cerr << "Texture::UploadToRenderer -> GL texture functions not available" << std::endl;
        return;
    }

    unsigned int id = 0;
    pglGenTextures(1, &id);
    pglBindTexture(GL_TEXTURE_2D, id);
    int wrapMode = GL_REPEAT;
    if (settings_.wrap == TextureWrap::Clamp) wrapMode = GL_CLAMP_TO_EDGE;
    else if (settings_.wrap == TextureWrap::Mirror) wrapMode = GL_MIRRORED_REPEAT;
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapMode);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapMode);

    bool canMipmap = settings_.mipmaps && pglGenerateMipmap;
    int minFilter = GL_NEAREST;
    int magFilter = GL_NEAREST;
    if (settings_.filter == TextureFilter::Linear || settings_.filter == TextureFilter::Anisotropic) {
        magFilter = GL_LINEAR;
        minFilter = canMipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR;
    } else {
        magFilter = GL_NEAREST;
        minFilter = canMipmap ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST;
    }
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);

    int internalFormat = settings_.srgb ? GL_SRGB8_ALPHA8 : GL_RGBA;
    pglTexImage2D(GL_TEXTURE_2D, 0, internalFormat, (int)width_, (int)height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels_.data());

    if (canMipmap) {
        pglGenerateMipmap(GL_TEXTURE_2D);
    }

    if (settings_.filter == TextureFilter::Anisotropic && pglTexParameterf) {
        pglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 8.0f);
    }

    textureID_ = id;
    std::cout << "Texture::UploadToRenderer -> created GL texture " << textureID_ << " (" << width_ << "x" << height_ << ")" << std::endl;
}

void Texture::DestroyOnRenderer(IGraphicsAPI* renderer) {
    // If we have a renderer-managed handle, try to destroy it via the appropriate renderer
    if (rendererHandle_.IsValid()) {
        // If caller provided the renderer and it matches owner, use it
        if (renderer) {
            if (renderer == rendererOwnerPtr_ || renderer->GetName() == rendererOwner_) {
                renderer->DestroyTexture(rendererHandle_);
                TextureRegistry::Instance().UnregisterHandle(this, rendererOwnerPtr_, rendererHandle_);
                rendererHandle_ = {};
                rendererOwner_.clear();
                rendererOwnerPtr_ = nullptr;
                textureID_ = 0;
                return;
            }
        } else {
            // No renderer passed: if we have an owner pointer, call through it
            if (rendererOwnerPtr_) {
                rendererOwnerPtr_->DestroyTexture(rendererHandle_);
                TextureRegistry::Instance().UnregisterHandle(this, rendererOwnerPtr_, rendererHandle_);
                rendererHandle_ = {};
                rendererOwner_.clear();
                rendererOwnerPtr_ = nullptr;
                textureID_ = 0;
                return;
            }
            // Fallback by trying to match owner by name
            if (auto owner = RendererManager::GetRenderer()) {
                if (owner->GetName() == rendererOwner_) {
                    owner->DestroyTexture(rendererHandle_);
                    TextureRegistry::Instance().UnregisterHandle(this, owner, rendererHandle_);
                    rendererHandle_ = {};
                    rendererOwner_.clear();
                    rendererOwnerPtr_ = nullptr;
                    textureID_ = 0;
                    return;
                }
            }
        }

        // If we couldn't find the owner renderer, and the owner was OpenGL and a GL context exists, try legacy GL delete
        if (rendererOwner_ == std::string("opengl") && SDL_GL_GetCurrentContext()) {
            ResolveGL((void**)&pglDeleteTextures, "glDeleteTextures");
            if (pglDeleteTextures) {
                unsigned int id = static_cast<unsigned int>(rendererHandle_.id);
                pglDeleteTextures(1, &id);
                std::cout << "Texture::DestroyOnRenderer -> deleted GL texture " << id << " (fallback)" << std::endl;
            }
            TextureRegistry::Instance().UnregisterHandle(this, nullptr, rendererHandle_);
            rendererHandle_ = {};
            rendererOwner_.clear();
            rendererOwnerPtr_ = nullptr;
            textureID_ = 0;
            return;
        }
    }

    // Legacy: if we have a GL texture id, delete it
    if (textureID_) {
        ResolveGL((void**)&pglDeleteTextures, "glDeleteTextures");
        if (pglDeleteTextures) {
            pglDeleteTextures(1, &textureID_);
            std::cout << "Texture::DestroyOnRenderer -> deleted GL texture " << textureID_ << std::endl;
        }
        textureID_ = 0;
    }
}

} // namespace Genesis::Engine