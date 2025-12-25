#include "ENGINE/Texture.h"
#include "ENGINE/TextureRegistry.h"
#include <SDL.h>
#include <iostream>

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
    auto t = std::make_shared<Texture>();
    t->width_ = width;
    t->height_ = height;
    t->pixels_ = pixels;
    TextureRegistry::Instance().Register(t.get());
    return t;
}

Texture::~Texture() {
    TextureRegistry::Instance().Unregister(this);
    DestroyOnRenderer(nullptr);
}

void Texture::UploadToRenderer(IGraphicsAPI* /*renderer*/) {
    if (textureID_ != 0) return; // already uploaded
    if (pixels_.empty() || width_ == 0 || height_ == 0) return;

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

void Texture::DestroyOnRenderer(IGraphicsAPI* /*renderer*/) {
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