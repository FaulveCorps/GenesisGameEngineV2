#include "engine/Mesh.h"
#include <SDL.h>
#include <iostream>

namespace Genesis::Engine {

// Destructor placed after function pointer declarations for visibility

void Mesh::SetData(const std::vector<float>& vertices, const std::vector<float>& normals, const std::vector<uint32_t>& indices) {
    vertices_ = vertices;
    normals_ = normals;
    indices_ = indices;
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
    // Resolve delete functions lazily
    if (!pglDeleteBuffers) ResolveGLFunction((void**)&pglDeleteBuffers, "glDeleteBuffers");
    if (!pglDeleteVertexArrays) ResolveGLFunction((void**)&pglDeleteVertexArrays, "glDeleteVertexArrays");
    if (ebo_ && pglDeleteBuffers) pglDeleteBuffers(1, &ebo_);
    if (vbo_ && pglDeleteBuffers) pglDeleteBuffers(1, &vbo_);
    if (vao_ && pglDeleteVertexArrays) pglDeleteVertexArrays(1, &vao_);
}

void Mesh::UploadToGPU() {
    if (uploaded_) return;
    if (vertices_.empty() || indices_.empty()) return;

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

    // EBO
    pglGenBuffers(1, &ebo_);
    pglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    pglBufferData(GL_ELEMENT_ARRAY_BUFFER, (ptrdiff_t)(indices_.size() * sizeof(uint32_t)), indices_.data(), GL_STATIC_DRAW);

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

    // Unbind VAO
    pglBindVertexArray(0);
    uploaded_ = true;
}

void Mesh::Draw() const {
    if (vertices_.empty() || indices_.empty()) return;

    const unsigned int GL_TRIANGLES = 0x0004;
    const unsigned int GL_UNSIGNED_INT = 0x1405;
    const unsigned int GL_FLOAT = 0x1406;

    if (uploaded_ && vao_) {
        // Use VAO path
        pglBindVertexArray(vao_);
        if (!pglDrawElements) ResolveGLFunction((void**)&pglDrawElements, "glDrawElements");
        pglDrawElements(GL_TRIANGLES, (int)indices_.size(), GL_UNSIGNED_INT, nullptr);
        pglBindVertexArray(0);
    } else {
        // Fallback to client arrays
        if (!pglEnableClientState) ResolveGLFunction((void**)&pglEnableClientState, "glEnableClientState");
        if (!pglVertexPointer) ResolveGLFunction((void**)&pglVertexPointer, "glVertexPointer");
        if (!pglNormalPointer) ResolveGLFunction((void**)&pglNormalPointer, "glNormalPointer");
        if (!pglDisableClientState) ResolveGLFunction((void**)&pglDisableClientState, "glDisableClientState");

        const unsigned int GL_VERTEX_ARRAY = 0x8074;
        const unsigned int GL_NORMAL_ARRAY = 0x8075;
        pglEnableClientState(GL_VERTEX_ARRAY);
        pglVertexPointer(3, GL_FLOAT, 0, vertices_.data());

        if (!normals_.empty()) {
            pglEnableClientState(GL_NORMAL_ARRAY);
            pglNormalPointer(GL_FLOAT, 0, normals_.data());
        } else {
            pglDisableClientState(GL_NORMAL_ARRAY);
        }

        if (!pglDrawElements) ResolveGLFunction((void**)&pglDrawElements, "glDrawElements");
        pglDrawElements(GL_TRIANGLES, (int)indices_.size(), GL_UNSIGNED_INT, indices_.data());

        pglDisableClientState(GL_VERTEX_ARRAY);
        if (!normals_.empty()) pglDisableClientState(GL_NORMAL_ARRAY);
    }
}

} // namespace Genesis::Engine
