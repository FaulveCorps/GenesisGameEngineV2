#include "ENGINE/Texture.h"
#include "ENGINE/TextureRegistry.h"
#include "ENGINE/IGraphics.h"
#include "ENGINE/RendererManager.h"
#include <SDL.h>
#include <iostream>
#include <cstring>
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
using PFNGLTEXIMAGE2DPROC = void (APIENTRY*)(unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void*);
using PFNGLDELETETEXTURESPROC = void (APIENTRY*)(int, const unsigned int*);

static PFNGLGENTEXTURESPROC pglGenTextures = nullptr;
static PFNGLBINDTEXTUREPROC pglBindTexture = nullptr;
static PFNGLTEXPARAMETERIPROC pglTexParameteri = nullptr;
static PFNGLTEXIMAGE2DPROC pglTexImage2D = nullptr;
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
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800

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
#ifdef HAVE_STB_IMAGE
    int x = 0, y = 0, n = 0;
    unsigned char* data = stbi_load(path.c_str(), &x, &y, &n, 4);
    if (data) {
        size_t sz = static_cast<size_t>(x) * y * 4;
        std::vector<uint8_t> pixels(sz);
        std::memcpy(pixels.data(), data, sz);
        stbi_image_free(data);
        return CreateFromMemory(static_cast<uint32_t>(x), static_cast<uint32_t>(y), pixels);
    }
#endif

    SDL_Surface* surf = SDL_LoadBMP(path.c_str());
    if (!surf) {
        std::cerr << "Texture::CreateFromFile -> SDL_LoadBMP failed for '" << path << "': " << SDL_GetError() << std::endl;
        return nullptr;
    }
    SDL_Surface* conv = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surf);
    if (!conv) {
        std::cerr << "Texture::CreateFromFile -> SDL_ConvertSurfaceFormat failed for '" << path << "': " << SDL_GetError() << std::endl;
        return nullptr;
    }
    uint32_t w = static_cast<uint32_t>(conv->w);
    uint32_t h = static_cast<uint32_t>(conv->h);
    std::vector<uint8_t> pixels(static_cast<size_t>(w) * h * 4);
    std::memcpy(pixels.data(), conv->pixels, pixels.size());
    SDL_FreeSurface(conv);
    return CreateFromMemory(w, h, pixels);
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

        auto h = renderer->CreateTexture(width_, height_, pixels_.empty() ? nullptr : pixels_.data());
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
    ResolveGL((void**)&pglTexImage2D, "glTexImage2D");
    ResolveGL((void**)&pglDeleteTextures, "glDeleteTextures");

    if (!pglGenTextures || !pglBindTexture || !pglTexParameteri || !pglTexImage2D || !pglDeleteTextures) {
        std::cerr << "Texture::UploadToRenderer -> GL texture functions not available" << std::endl;
        return;
    }

    unsigned int id = 0;
    pglGenTextures(1, &id);
    pglBindTexture(GL_TEXTURE_2D, id);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    pglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (int)width_, (int)height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels_.data());

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