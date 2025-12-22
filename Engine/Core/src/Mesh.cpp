#include "engine/Mesh.h"
#include <GL/gl.h>
#include <SDL3/SDL.h>
#include <iostream>

namespace Genesis::Engine {

Mesh::~Mesh() {
    if (ebo_) glDeleteBuffers(1, &ebo_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
}

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

using PFNGLGENVERTEXARRAYSPROC = void(*)(GLsizei, unsigned int*);
using PFNGLBINDVERTEXARRAYPROC = void(*)(unsigned int);
using PFNGLGENBUFFERSPROC = void(*)(GLsizei, unsigned int*);
using PFNGLBINDBUFFERPROC = void(*)(GLenum, unsigned int);
using PFNGLBUFFERDATAPROC = void(*)(GLenum, ptrdiff_t, const void*, GLenum);
using PFNGLENABLEVERTEXATTRIBARRAYPROC = void(*)(unsigned int);
using PFNGLVERTEXATTRIBPOINTERPROC = void(*)(unsigned int, int, GLenum, unsigned char, int, const void*);
using PFNGLDELETEVERTEXARRAYSPROC = void(*)(GLsizei, const unsigned int*);
using PFNGLDELETEBUFFERSPROC = void(*)(GLsizei, const unsigned int*);

static PFNGLGENVERTEXARRAYSPROC pglGenVertexArrays = nullptr;
static PFNGLBINDVERTEXARRAYPROC pglBindVertexArray = nullptr;
static PFNGLGENBUFFERSPROC pglGenBuffers = nullptr;
static PFNGLBINDBUFFERPROC pglBindBuffer = nullptr;
static PFNGLBUFFERDATAPROC pglBufferData = nullptr;
static PFNGLENABLEVERTEXATTRIBARRAYPROC pglEnableVertexAttribArray = nullptr;
static PFNGLVERTEXATTRIBPOINTERPROC pglVertexAttribPointer = nullptr;
static PFNGLDELETEVERTEXARRAYSPROC pglDeleteVertexArrays = nullptr;
static PFNGLDELETEBUFFERSPROC pglDeleteBuffers = nullptr;

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
    pglBindBuffer(GL_ARRAY_BUFFER, vbo_);
    pglBufferData(GL_ARRAY_BUFFER, (ptrdiff_t)(vertices_.size() * sizeof(float)), vertices_.data(), GL_STATIC_DRAW);

    // EBO
    pglGenBuffers(1, &ebo_);
    pglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    pglBufferData(GL_ELEMENT_ARRAY_BUFFER, (ptrdiff_t)(indices_.size() * sizeof(uint32_t)), indices_.data(), GL_STATIC_DRAW);

    // Vertex attribute 0 = position (3 floats)
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

    if (uploaded_ && vao_) {
        // Use VAO path
        pglBindVertexArray(vao_);
        glDrawElements(GL_TRIANGLES, (GLsizei)indices_.size(), GL_UNSIGNED_INT, nullptr);
        pglBindVertexArray(0);
    } else {
        // Fallback to client arrays
        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, vertices_.data());

        if (!normals_.empty()) {
            glEnableClientState(GL_NORMAL_ARRAY);
            glNormalPointer(GL_FLOAT, 0, normals_.data());
        } else {
            glDisableClientState(GL_NORMAL_ARRAY);
        }

        glDrawElements(GL_TRIANGLES, (GLsizei)indices_.size(), GL_UNSIGNED_INT, indices_.data());

        glDisableClientState(GL_VERTEX_ARRAY);
        if (!normals_.empty()) glDisableClientState(GL_NORMAL_ARRAY);
    }
}

} // namespace Genesis::Engine
