#include "engine/Mesh.h"
#include "engine/MeshRegistry.h"
#include "engine/RendererManager.h"
#include <SDL.h>
#include <iostream>

// Minimal GL boolean fallback (used by vertex attrib setup) if GL headers are not included
#ifndef GL_FALSE
static const unsigned char GL_FALSE = 0;
#endif

namespace Genesis::Engine {

// Destructor placed after function pointer declarations for visibility

void Mesh::SetData(const std::vector<float>& vertices, const std::vector<float>& normals, const std::vector<float>& uvs, const std::vector<uint32_t>& indices) {
    vertices_ = vertices;
    normals_ = normals;
    uvs_ = uvs;
    indices_ = indices;

    // Register so we can recreate/destroy resources when renderer switches
    MeshRegistry::Instance().Register(this);
}

// Minimal dynamic GL function loader (resolve only what we need)
static bool ResolveGLFunction(void** fnPtr, const char* name) {
    if (*fnPtr) return true;
    auto addr = (void*)SDL_GL_GetProcAddress(name);
    if (!addr) return false;
    *fnPtr = addr;
    return true;
}

// Minimal typedefs without including GL headers
#ifdef _WIN32
#define APIENTRY __stdcall
#endif
using PFNGLGENVERTEXARRAYSPROC = void (APIENTRY*)(int, unsigned int*);
using PFNGLBINDVERTEXARRAYPROC = void (APIENTRY*)(unsigned int);
using PFNGLGENBUFFERSPROC = void (APIENTRY*)(int, unsigned int*);
using PFNGLBINDBUFFERPROC = void (APIENTRY*)(unsigned int, unsigned int);
using PFNGLBUFFERDATAPROC = void (APIENTRY*)(unsigned int, ptrdiff_t, const void*, unsigned int);
using PFNGLENABLEVERTEXATTRIBARRAYPROC = void (APIENTRY*)(unsigned int);
using PFNGLVERTEXATTRIBPOINTERPROC = void (APIENTRY*)(unsigned int, int, unsigned int, unsigned char, int, const void*);
using PFNGLDELETEVERTEXARRAYSPROC = void (APIENTRY*)(int, const unsigned int*);
using PFNGLDELETEBUFFERSPROC = void (APIENTRY*)(int, const unsigned int*);
using PFNGLDRAWELEMENTSPROC = void (APIENTRY*)(unsigned int, int, unsigned int, const void*);
using PFNGLENABLECLIENTSTATEPROC = void (APIENTRY*)(unsigned int);
using PFNGLDISABLECLIENTSTATEPROC = void (APIENTRY*)(unsigned int);
using PFNGLVERTEXPOINTERPROC = void (APIENTRY*)(int, unsigned int, int, const void*);
using PFNGLNORMALPOINTERPROC = void (APIENTRY*)(unsigned int, int, const void*);

static PFNGLGENVERTEXARRAYSPROC pglGenVertexArrays = nullptr;
static PFNGLBINDVERTEXARRAYPROC pglBindVertexArray = nullptr;
static PFNGLGENBUFFERSPROC pglGenBuffers = nullptr;
static PFNGLBINDBUFFERPROC pglBindBuffer = nullptr;
static PFNGLBUFFERDATAPROC pglBufferData = nullptr;
static PFNGLENABLEVERTEXATTRIBARRAYPROC pglEnableVertexAttribArray = nullptr;
static PFNGLVERTEXATTRIBPOINTERPROC pglVertexAttribPointer = nullptr;
static PFNGLDELETEVERTEXARRAYSPROC pglDeleteVertexArrays = nullptr;
static PFNGLDELETEBUFFERSPROC pglDeleteBuffers = nullptr;
static PFNGLDRAWELEMENTSPROC pglDrawElements = nullptr;
static PFNGLENABLECLIENTSTATEPROC pglEnableClientState = nullptr;
static PFNGLDISABLECLIENTSTATEPROC pglDisableClientState = nullptr;
static PFNGLVERTEXPOINTERPROC pglVertexPointer = nullptr;
static PFNGLNORMALPOINTERPROC pglNormalPointer = nullptr;

Mesh::~Mesh() {
    // Unregister from global registry
    MeshRegistry::Instance().Unregister(this);

    // Resolve delete functions lazily and delete GL resources if present
    if (!pglDeleteBuffers) ResolveGLFunction((void**)&pglDeleteBuffers, "glDeleteBuffers");
    if (!pglDeleteVertexArrays) ResolveGLFunction((void**)&pglDeleteVertexArrays, "glDeleteVertexArrays");
    if (ebo_ && pglDeleteBuffers) { pglDeleteBuffers(1, &ebo_); }
    if (vbo_ && pglDeleteBuffers) { pglDeleteBuffers(1, &vbo_); }
    if (vao_ && pglDeleteVertexArrays) { pglDeleteVertexArrays(1, &vao_); }

    // If this mesh had a renderer-side handle, attempt to destroy it on the current renderer
    IGraphicsAPI* cur = RendererManager::GetRenderer();
    if (uploadKind_ == UploadKind::Renderer && handle_.IsValid() && cur) {
        cur->DestroyMesh(handle_);
        handle_ = {};
        uploadKind_ = UploadKind::None;
        uploaded_ = false;
    }
}

// Move constructor
Mesh::Mesh(Mesh&& other) noexcept {
    vertices_ = std::move(other.vertices_);
    normals_ = std::move(other.normals_);
    uvs_ = std::move(other.uvs_);
    indices_ = std::move(other.indices_);

    vao_ = other.vao_;
    vbo_ = other.vbo_;
    ebo_ = other.ebo_;
    uploaded_ = other.uploaded_;
    indexType_ = other.indexType_;
    handle_ = other.handle_;
    uploadKind_ = other.uploadKind_;

    other.vao_ = 0;
    other.vbo_ = 0;
    other.ebo_ = 0;
    other.uploaded_ = false;
    other.indexType_ = 0;
    other.handle_ = {};
    other.uploadKind_ = UploadKind::None;
}

// Move assignment
Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this == &other) return *this;

    // release current resources
    if (!pglDeleteBuffers) ResolveGLFunction((void**)&pglDeleteBuffers, "glDeleteBuffers");
    if (!pglDeleteVertexArrays) ResolveGLFunction((void**)&pglDeleteVertexArrays, "glDeleteVertexArrays");
    if (ebo_ && pglDeleteBuffers) pglDeleteBuffers(1, &ebo_);
    if (vbo_ && pglDeleteBuffers) pglDeleteBuffers(1, &vbo_);
    if (vao_ && pglDeleteVertexArrays) pglDeleteVertexArrays(1, &vao_);

    // If this mesh had a renderer-side handle, attempt to destroy it on the current renderer
    IGraphicsAPI* cur = RendererManager::GetRenderer();
    if (uploadKind_ == UploadKind::Renderer && handle_.IsValid() && cur) {
        cur->DestroyMesh(handle_);
    }

    vertices_ = std::move(other.vertices_);
    normals_ = std::move(other.normals_);
    uvs_ = std::move(other.uvs_);
    indices_ = std::move(other.indices_);

    vao_ = other.vao_;
    vbo_ = other.vbo_;
    ebo_ = other.ebo_;
    uploaded_ = other.uploaded_;
    indexType_ = other.indexType_;
    handle_ = other.handle_;
    uploadKind_ = other.uploadKind_;

    other.vao_ = 0;
    other.vbo_ = 0;
    other.ebo_ = 0;
    other.uploaded_ = false;
    other.indexType_ = 0;
    other.handle_ = {};
    other.uploadKind_ = UploadKind::None;

    return *this;
}

void Mesh::UploadToGPU() {
    if (uploaded_) {
        return;
    }
    if (vertices_.empty() || indices_.empty()) {
        return;
    }

    // If a non-GL renderer is active and supports CreateMesh, try to upload there first
    IGraphicsAPI* cur = RendererManager::GetRenderer();
    if (cur) {
        MeshDesc desc{ vertices_, normals_, uvs_, indices_ };
        MeshHandle h = cur->CreateMesh(desc);
        if (h.IsValid()) {
            handle_ = h;
            uploaded_ = true;
            uploadKind_ = UploadKind::Renderer;
            return;
        }
    }

    // Fallback: try to create GL buffers as before
    // Resolve required functions
    if (!ResolveGLFunction((void**)&pglGenVertexArrays, "glGenVertexArrays") ||
        !ResolveGLFunction((void**)&pglBindVertexArray, "glBindVertexArray") ||
        !ResolveGLFunction((void**)&pglGenBuffers, "glGenBuffers") ||
        !ResolveGLFunction((void**)&pglBindBuffer, "glBindBuffer") ||
        !ResolveGLFunction((void**)&pglBufferData, "glBufferData") ||
        !ResolveGLFunction((void**)&pglEnableVertexAttribArray, "glEnableVertexAttribArray") ||
        !ResolveGLFunction((void**)&pglVertexAttribPointer, "glVertexAttribPointer") ||
        !ResolveGLFunction((void**)&pglDeleteVertexArrays, "glDeleteVertexArrays") ||
        !ResolveGLFunction((void**)&pglDeleteBuffers, "glDeleteBuffers")) {
        // Missing modern GL functions — do nothing and we'll fall back to client arrays
        std::cerr << "Warning: modern GL buffer functions not available; falling back to client arrays" << std::endl;
        return;
    }

    // Create VAO
    pglGenVertexArrays(1, &vao_);
    pglBindVertexArray(vao_);

    // VBO
    pglGenBuffers(1, &vbo_);
    // Constants
    const unsigned int GL_ARRAY_BUFFER = 0x8892;
    const unsigned int GL_ELEMENT_ARRAY_BUFFER = 0x8893;
    const unsigned int GL_STATIC_DRAW = 0x88E4;
    const unsigned int GL_FLOAT = 0x1406;

    pglBindBuffer(GL_ARRAY_BUFFER, vbo_);
    pglBufferData(GL_ARRAY_BUFFER, (ptrdiff_t)(vertices_.size() * sizeof(float)), vertices_.data(), GL_STATIC_DRAW);

    // EBO: choose 16-bit indices when possible (some drivers have issues with 32-bit element draws)
    pglGenBuffers(1, &ebo_);
    pglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    // Determine if indices fit in unsigned short
    uint32_t maxIndex = 0;
    for (uint32_t i : indices_) if (i > maxIndex) maxIndex = i;
    const unsigned int GL_UNSIGNED_SHORT = 0x1403;
    const unsigned int GL_UNSIGNED_INT = 0x1405;
    if (maxIndex <= 0xFFFFu) {
        // Convert to 16-bit indices
        std::vector<uint16_t> indices16;
        indices16.reserve(indices_.size());
        for (uint32_t i : indices_) indices16.push_back((uint16_t)i);
        pglBufferData(GL_ELEMENT_ARRAY_BUFFER, (ptrdiff_t)(indices16.size() * sizeof(uint16_t)), indices16.data(), GL_STATIC_DRAW);
        indexType_ = GL_UNSIGNED_SHORT;
    } else {
        pglBufferData(GL_ELEMENT_ARRAY_BUFFER, (ptrdiff_t)(indices_.size() * sizeof(uint32_t)), indices_.data(), GL_STATIC_DRAW);
        indexType_ = GL_UNSIGNED_INT;
    }

    // Vertex attribute 0 = position (3 floats)
    const unsigned char GL_FALSE = 0;
    pglEnableVertexAttribArray(0);
    pglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (const void*)0);

    // If normals exist, create a normals VBO interleaving is not implemented yet; upload as separate VBO
    if (!normals_.empty()) {
        unsigned int normalsVBO = 0;
        pglGenBuffers(1, &normalsVBO);
        pglBindBuffer(GL_ARRAY_BUFFER, normalsVBO);
        pglBufferData(GL_ARRAY_BUFFER, (ptrdiff_t)(normals_.size() * sizeof(float)), normals_.data(), GL_STATIC_DRAW);
        pglEnableVertexAttribArray(1);
        pglVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (const void*)0);
    }

    // If UVs exist, create a UV VBO
    if (!uvs_.empty()) {
        unsigned int uvsVBO = 0;
        pglGenBuffers(1, &uvsVBO);
        pglBindBuffer(GL_ARRAY_BUFFER, uvsVBO);
        pglBufferData(GL_ARRAY_BUFFER, (ptrdiff_t)(uvs_.size() * sizeof(float)), uvs_.data(), GL_STATIC_DRAW);
        pglEnableVertexAttribArray(2);
        pglVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, (const void*)0);
    }

    // NOTE: keep VAO bound for diagnosis (some drivers have surprising VAO semantics)
    // pglBindVertexArray(0);
    uploaded_ = true;
}

void Mesh::DestroyOnRenderer(IGraphicsAPI* renderer) {
    // If this mesh was uploaded to a renderer-side handle, destroy it there
    if (uploadKind_ == UploadKind::Renderer && handle_.IsValid()) {
        if (renderer) {
            renderer->DestroyMesh(handle_);
        }
        handle_ = {};
        uploadKind_ = UploadKind::None;
        uploaded_ = false;
        return;
    }

    // If this mesh has GL resources, delete them now
    if (vao_ || vbo_ || ebo_) {
        if (!SDL_GL_GetCurrentContext()) {
            return;
        }
        if (!pglDeleteBuffers) ResolveGLFunction((void**)&pglDeleteBuffers, "glDeleteBuffers");
        if (!pglDeleteVertexArrays) ResolveGLFunction((void**)&pglDeleteVertexArrays, "glDeleteVertexArrays");
        if (ebo_ && pglDeleteBuffers) { pglDeleteBuffers(1, &ebo_); ebo_ = 0; }
        if (vbo_ && pglDeleteBuffers) { pglDeleteBuffers(1, &vbo_); vbo_ = 0; }
        if (vao_ && pglDeleteVertexArrays) { pglDeleteVertexArrays(1, &vao_); vao_ = 0; }
        uploadKind_ = UploadKind::None;
        uploaded_ = false;
        return;
    }
}

void Mesh::UploadToRenderer(IGraphicsAPI* renderer) {
    if (uploaded_) return;
    if (vertices_.empty() || indices_.empty()) return;

    if (!renderer) return; // nothing to do

    MeshDesc desc{ vertices_, normals_, uvs_, indices_ };
    MeshHandle h = renderer->CreateMesh(desc);
    if (h.IsValid()) {
        handle_ = h;
        uploaded_ = true;
        uploadKind_ = UploadKind::Renderer;
    }
}

void Mesh::Draw(Material* material, const float* transform) const {
    if (vertices_.empty() || indices_.empty()) return;

    // If this mesh belongs to a non-GL renderer, draw via the renderer API
    if (uploadKind_ == UploadKind::Renderer && handle_.IsValid()) {
        IGraphicsAPI* cur = RendererManager::GetRenderer();
        if (cur) {
            cur->DrawMesh(handle_, material, transform);
            return;
        }
    }
    
    // Fallback to standard Draw() if no renderer handle or renderer doesn't support material/transform override
    // Note: Standard Draw() doesn't support material/transform, so this is a best-effort fallback
    // Ideally, we should set uniforms here if we are in GL mode but not using the renderer API directly
    // For now, just call Draw() which will use whatever shader is active
    Draw();
}

void Mesh::Draw() const {
    if (vertices_.empty() || indices_.empty()) return;

    // If this mesh belongs to a non-GL renderer, draw via the renderer API
    if (uploadKind_ == UploadKind::Renderer && handle_.IsValid()) {
        IGraphicsAPI* cur = RendererManager::GetRenderer();
        if (cur) {
            cur->DrawMesh(handle_);
            return;
        }
    }

    const unsigned int GL_TRIANGLES = 0x0004;
    const unsigned int GL_UNSIGNED_INT = 0x1405;
    const unsigned int GL_FLOAT = 0x1406;

    if (uploaded_ && vao_) {
        // Use VAO path
        pglBindVertexArray(vao_);

        // If pglBindVertexArray pointer is missing, attempt to resolve it
        if (!pglBindVertexArray) ResolveGLFunction((void**)&pglBindVertexArray, "glBindVertexArray");

        // Ensure EBO is bound (some drivers require re-binding per VAO or context)
        if (ebo_) {
            if (!pglBindBuffer) ResolveGLFunction((void**)&pglBindBuffer, "glBindBuffer");
            if (pglBindBuffer) {
                pglBindBuffer(0x8893 /*GL_ELEMENT_ARRAY_BUFFER*/, ebo_);
            }
        }

        if (!pglDrawElements) {
            if (!ResolveGLFunction((void**)&pglDrawElements, "glDrawElements")) {
                pglBindVertexArray(0);
                return;
            }
        }

        // Use the index type chosen at upload time (GL_UNSIGNED_SHORT or GL_UNSIGNED_INT)
        unsigned int drawIndexType = indexType_ ? indexType_ : 0x1405 /*GL_UNSIGNED_INT*/;
        pglDrawElements(GL_TRIANGLES, (int)indices_.size(), drawIndexType, nullptr);

        pglBindVertexArray(0);
    } else {
        // Fallback to client arrays
        if (!pglEnableClientState) ResolveGLFunction((void**)&pglEnableClientState, "glEnableClientState");
        if (!pglVertexPointer) ResolveGLFunction((void**)&pglVertexPointer, "glVertexPointer");
        if (!pglNormalPointer) ResolveGLFunction((void**)&pglNormalPointer, "glNormalPointer");
        if (!pglDisableClientState) ResolveGLFunction((void**)&pglDisableClientState, "glDisableClientState");

        const unsigned int GL_VERTEX_ARRAY = 0x8074;
        const unsigned int GL_NORMAL_ARRAY = 0x8075;
        if (pglEnableClientState) pglEnableClientState(GL_VERTEX_ARRAY);
        if (pglVertexPointer) pglVertexPointer(3, GL_FLOAT, 0, vertices_.data());

        if (!normals_.empty() && pglEnableClientState && pglNormalPointer) {
            pglEnableClientState(GL_NORMAL_ARRAY);
            pglNormalPointer(GL_FLOAT, 0, normals_.data());
        } else if (pglDisableClientState) {
            pglDisableClientState(GL_NORMAL_ARRAY);
        }

        if (!pglDrawElements) ResolveGLFunction((void**)&pglDrawElements, "glDrawElements");
        if (pglDrawElements) pglDrawElements(GL_TRIANGLES, (int)indices_.size(), GL_UNSIGNED_INT, indices_.data());

        if (pglDisableClientState) pglDisableClientState(GL_VERTEX_ARRAY);
        if (!normals_.empty() && pglDisableClientState) pglDisableClientState(GL_NORMAL_ARRAY);
    }
}

} // namespace Genesis::Engine
