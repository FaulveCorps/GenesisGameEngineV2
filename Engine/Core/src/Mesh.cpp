#include "engine/Mesh.h"
#include <SDL.h>
#include <iostream>

// Minimal GL boolean fallback (used by vertex attrib setup) if GL headers are not included
#ifndef GL_FALSE
static const unsigned char GL_FALSE = 0;
#endif

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

    std::cout << "Mesh::UploadToGPU -> funcs: genVAO=" << (pglGenVertexArrays?1:0) << " bindVAO=" << (pglBindVertexArray?1:0) << " genBuf=" << (pglGenBuffers?1:0) << " bindBuf=" << (pglBindBuffer?1:0) << " bufferData=" << (pglBufferData?1:0) << " enableAttr=" << (pglEnableVertexAttribArray?1:0) << " attribPtr=" << (pglVertexAttribPointer?1:0) << std::endl;

    // Sanity: print current GL context and function pointer addresses
    std::cout << "Mesh::UploadToGPU -> SDL_GL_GetCurrentContext=" << (void*)SDL_GL_GetCurrentContext() << " pglBindBuffer=" << (void*)pglBindBuffer << " pglBindVertexArray=" << (void*)pglBindVertexArray << " pglEnableVertexAttribArray=" << (void*)pglEnableVertexAttribArray << std::endl;

    // Create VAO
    pglGenVertexArrays(1, &vao_);
    pglBindVertexArray(vao_);

    // Verify VAO is bound
    {
        using PFNGLGETINTEGERVPROC = void (APIENTRY*)(unsigned int, int*);
        PFNGLGETINTEGERVPROC pglGetIntegerv = nullptr;
        auto addr = (void*)SDL_GL_GetProcAddress("glGetIntegerv");
        if (addr) pglGetIntegerv = (PFNGLGETINTEGERVPROC)addr;
        if (pglGetIntegerv) {
            int vaoBind = 0; const unsigned int GL_VERTEX_ARRAY_BINDING = 0x85B5;
            pglGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vaoBind);
            std::cout << "Mesh::UploadToGPU -> after bind VAO GL_VERTEX_ARRAY_BINDING=" << vaoBind << std::endl;
        }
    }

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
        std::cout << "Mesh::UploadToGPU -> uploaded indices as GL_UNSIGNED_SHORT (maxIndex=" << maxIndex << ")" << std::endl;
    } else {
        pglBufferData(GL_ELEMENT_ARRAY_BUFFER, (ptrdiff_t)(indices_.size() * sizeof(uint32_t)), indices_.data(), GL_STATIC_DRAW);
        indexType_ = GL_UNSIGNED_INT;
        std::cout << "Mesh::UploadToGPU -> uploaded indices as GL_UNSIGNED_INT (maxIndex=" << maxIndex << ")" << std::endl;
    }

    // Debug: verify EBO binding on VAO immediately after upload
    {
        using PFNGLGETINTEGERVPROC = void (APIENTRY*)(unsigned int, int*);
        PFNGLGETINTEGERVPROC pglGetIntegerv = nullptr;
        auto addrGetIntegerv = (void*)SDL_GL_GetProcAddress("glGetIntegerv");
        if (addrGetIntegerv) pglGetIntegerv = (PFNGLGETINTEGERVPROC)addrGetIntegerv;
        if (pglGetIntegerv) {
            int boundElem = 0;
            const unsigned int GL_ELEMENT_ARRAY_BUFFER_BINDING = 0x8895;
            pglGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &boundElem);
            std::cout << "Mesh::UploadToGPU -> ebo=" << ebo_ << " GL_ELEMENT_ARRAY_BUFFER_BINDING=" << boundElem << std::endl;
        } else {
            std::cerr << "Mesh::UploadToGPU -> glGetIntegerv not available" << std::endl;
        }
    }

    // Vertex attribute 0 = position (3 floats)
    const unsigned char GL_FALSE = 0;
    pglEnableVertexAttribArray(0);
    pglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (const void*)0);

    // Debug: query attrib state immediately after setup
    {
        auto addrGetVertexAttrib = (void*)SDL_GL_GetProcAddress("glGetVertexAttribiv");
        if (addrGetVertexAttrib) {
            using PFNGLGETVERTEXATTRIBIVPROC = void (APIENTRY*)(unsigned int, unsigned int, int*);
            PFNGLGETVERTEXATTRIBIVPROC pglGetVertexAttribiv = (PFNGLGETVERTEXATTRIBIVPROC)addrGetVertexAttrib;
            int enabled0 = 0, bufbind0 = -1; const unsigned int GL_VERTEX_ATTRIB_ARRAY_ENABLED = 0x8622; const unsigned int GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING = 0x889F;
            pglGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled0);
            pglGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &bufbind0);
            std::cout << "Mesh::UploadToGPU -> after setup attrib0 enabled=" << enabled0 << " buffer_binding=" << bufbind0 << std::endl;
        }
        auto addrGetError = (void*)SDL_GL_GetProcAddress("glGetError");
        if (addrGetError) {
            using PFNGLGETERRORPROC = unsigned int (APIENTRY*)();
            PFNGLGETERRORPROC pglGetError = (PFNGLGETERRORPROC)addrGetError;
            unsigned int err = pglGetError();
            if (err != 0) std::cerr << "Mesh::UploadToGPU -> GL error after attrib setup: 0x" << std::hex << err << std::dec << std::endl;
        }
    }

    // If normals exist, create a normals VBO interleaving is not implemented yet; upload as separate VBO
    if (!normals_.empty()) {
        unsigned int normalsVBO = 0;
        pglGenBuffers(1, &normalsVBO);
        pglBindBuffer(GL_ARRAY_BUFFER, normalsVBO);
        pglBufferData(GL_ARRAY_BUFFER, (ptrdiff_t)(normals_.size() * sizeof(float)), normals_.data(), GL_STATIC_DRAW);
        pglEnableVertexAttribArray(1);
        pglVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (const void*)0);
    }

    // NOTE: keep VAO bound for diagnosis (some drivers have surprising VAO semantics)
    // pglBindVertexArray(0);
    uploaded_ = true;
    std::cout << "Mesh::UploadToGPU -> SDL_GL_GetCurrentContext=" << (void*)SDL_GL_GetCurrentContext() << " vao=" << vao_ << " vbo=" << vbo_ << " ebo=" << ebo_ << "" << std::endl;
}

void Mesh::Draw() const {
    if (vertices_.empty() || indices_.empty()) return;

    const unsigned int GL_TRIANGLES = 0x0004;
    const unsigned int GL_UNSIGNED_INT = 0x1405;
    const unsigned int GL_FLOAT = 0x1406;

    if (uploaded_ && vao_) {
        // Use VAO path
        std::cout << "Mesh::Draw -> using VAO path (vao=" << vao_ << ")" << std::endl;
        std::cout << "Mesh::Draw -> vao=" << vao_ << " vbo=" << vbo_ << " ebo=" << ebo_ << std::endl;
        pglBindVertexArray(vao_);

        // Debug: print current GL context, VAO binding, and function pointer addresses
        std::cout << "Mesh::Draw -> SDL_GL_GetCurrentContext=" << (void*)SDL_GL_GetCurrentContext() << " pglBindVertexArray=" << (void*)pglBindVertexArray << " pglBindBuffer=" << (void*)pglBindBuffer << " pglDrawElements=" << (void*)pglDrawElements << std::endl;
        // If pglBindVertexArray pointer is missing, attempt to resolve it
        if (!pglBindVertexArray) ResolveGLFunction((void**)&pglBindVertexArray, "glBindVertexArray");

        // Quick sanity: is this name a valid VAO?
        {
            auto addrIsVAO = (void*)SDL_GL_GetProcAddress("glIsVertexArray");
            if (addrIsVAO) {
                using PFNGLISVERTEXARRAYPROC = unsigned char(APIENTRY*)(unsigned int);
                PFNGLISVERTEXARRAYPROC pglIsVertexArray = (PFNGLISVERTEXARRAYPROC)addrIsVAO;
                unsigned char isVAO = pglIsVertexArray(vao_);
                std::cout << "Mesh::Draw -> glIsVertexArray(" << vao_ << ")=" << (int)isVAO << std::endl;
            }
        }

        // Note: resolve glGetVertexAttribiv locally where it is used below
        {
            using PFNGLGETINTEGERVPROC = void (APIENTRY*)(unsigned int, int*);
            PFNGLGETINTEGERVPROC pglGetIntegerv = nullptr;
            auto addrGetIntegerv = (void*)SDL_GL_GetProcAddress("glGetIntegerv");
            if (addrGetIntegerv) pglGetIntegerv = (PFNGLGETINTEGERVPROC)addrGetIntegerv;
            if (pglGetIntegerv) {
                int vaoBind = 0; const unsigned int GL_VERTEX_ARRAY_BINDING = 0x85B5;
                pglGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vaoBind);
                std::cout << "Mesh::Draw -> after bind VAO GL_VERTEX_ARRAY_BINDING=" << vaoBind << std::endl;
            }
        }
        // Ensure EBO is bound (some drivers require re-binding per VAO or context)
        if (ebo_) {
            if (!pglBindBuffer) ResolveGLFunction((void**)&pglBindBuffer, "glBindBuffer");
            if (pglBindBuffer) {
                pglBindBuffer(0x8893 /*GL_ELEMENT_ARRAY_BUFFER*/, ebo_);
            } else {
                std::cerr << "Mesh::Draw -> pglBindBuffer not available" << std::endl;
            }

            // Query binding after rebind
            using PFNGLGETINTEGERVPROC = void (APIENTRY*)(unsigned int, int*);
            PFNGLGETINTEGERVPROC pglGetIntegerv = nullptr;
            auto addrGetIntegerv = (void*)SDL_GL_GetProcAddress("glGetIntegerv");
            if (addrGetIntegerv) pglGetIntegerv = (PFNGLGETINTEGERVPROC)addrGetIntegerv;
            if (pglGetIntegerv) {
                int boundElem = 0;
                const unsigned int GL_ELEMENT_ARRAY_BUFFER_BINDING = 0x8895;
                pglGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &boundElem);
                std::cout << "Mesh::Draw -> after rebind GL_ELEMENT_ARRAY_BUFFER_BINDING=" << boundElem << std::endl;
            } else {
                std::cerr << "Mesh::Draw -> glGetIntegerv not available" << std::endl;
            }
        }

        if (!pglDrawElements) {
            if (!ResolveGLFunction((void**)&pglDrawElements, "glDrawElements")) {
                std::cerr << "Mesh::Draw -> glDrawElements not available" << std::endl;
                pglBindVertexArray(0);
                return;
            }
        }
        // Debug: print indices count and first values
        std::cout << "Mesh::Draw -> indices.count=" << indices_.size();
        if (indices_.size() > 0) {
            std::cout << " sample[0..min3]=" << indices_[0];
            if (indices_.size() > 1) std::cout << "," << indices_[1];
            if (indices_.size() > 2) std::cout << "," << indices_[2];
        }
        std::cout << " indexType=" << indexType_ << std::endl;

        // Debug: query current buffer bindings and vertex attrib state
        {
            using PFNGLGETINTEGERVPROC = void (APIENTRY*)(unsigned int, int*);
            PFNGLGETINTEGERVPROC pglGetIntegerv = nullptr;
            auto addrGetIntegerv = (void*)SDL_GL_GetProcAddress("glGetIntegerv");
            if (addrGetIntegerv) pglGetIntegerv = (PFNGLGETINTEGERVPROC)addrGetIntegerv;
            if (pglGetIntegerv) {
                int boundArray = 0;
                int boundElem = 0;
                const unsigned int GL_ARRAY_BUFFER_BINDING = 0x8894;
                const unsigned int GL_ELEMENT_ARRAY_BUFFER_BINDING = 0x8895;
                pglGetIntegerv(GL_ARRAY_BUFFER_BINDING, &boundArray);
                pglGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &boundElem);
                std::cout << "Mesh::Draw -> GL_ARRAY_BUFFER_BINDING=" << boundArray << " GL_ELEMENT_ARRAY_BUFFER_BINDING=" << boundElem << std::endl;

                // Vertex attrib state
                using PFNGLGETVERTEXATTRIBIVPROC = void (APIENTRY*)(unsigned int, unsigned int, int*);
                PFNGLGETVERTEXATTRIBIVPROC pglGetVertexAttribiv = nullptr;
                auto addrGetVertexAttrib = (void*)SDL_GL_GetProcAddress("glGetVertexAttribiv");
                if (addrGetVertexAttrib) pglGetVertexAttribiv = (PFNGLGETVERTEXATTRIBIVPROC)addrGetVertexAttrib;
                if (pglGetVertexAttribiv) {
                    int enabled0 = 0, bufBind0 = -1;
                    const unsigned int GL_VERTEX_ATTRIB_ARRAY_ENABLED = 0x8622;
                    const unsigned int GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING = 0x889F;
                    pglGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled0);
                    pglGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &bufBind0);
                    std::cout << "Mesh::Draw -> attrib0 enabled=" << enabled0 << " buffer_binding=" << bufBind0 << std::endl;
                    int enabled1 = 0, bufBind1 = -1;
                    pglGetVertexAttribiv(1, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled1);
                    pglGetVertexAttribiv(1, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &bufBind1);
                    std::cout << "Mesh::Draw -> attrib1 enabled=" << enabled1 << " buffer_binding=" << bufBind1 << std::endl;

                    // If attributes aren't set on VAO, try to set them explicitly as a workaround
                    if (enabled0 == 0) {
                        std::cout << "Mesh::Draw -> attrib0 not enabled on VAO; explicitly binding VBO and setting attrib pointer" << std::endl;
                        if (!pglBindBuffer) ResolveGLFunction((void**)&pglBindBuffer, "glBindBuffer");
                        if (!pglEnableVertexAttribArray) ResolveGLFunction((void**)&pglEnableVertexAttribArray, "glEnableVertexAttribArray");
                        if (!pglVertexAttribPointer) ResolveGLFunction((void**)&pglVertexAttribPointer, "glVertexAttribPointer");
                        const unsigned int GL_ARRAY_BUFFER = 0x8892;
                        if (pglBindBuffer) pglBindBuffer(GL_ARRAY_BUFFER, vbo_);
                        if (pglEnableVertexAttribArray) pglEnableVertexAttribArray(0);
                        if (pglVertexAttribPointer) pglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (const void*)0);
                        // Try binding EBO directly
                        const unsigned int GL_ELEMENT_ARRAY_BUFFER = 0x8893;
                        if (pglBindBuffer && ebo_) pglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);

                        // Re-query state after manual setup to see if it took effect
                        if (pglGetVertexAttribiv) {
                            int enabledAfter=0, bindAfter=-1;
                            const unsigned int GL_VERTEX_ATTRIB_ARRAY_ENABLED = 0x8622; const unsigned int GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING = 0x889F;
                            pglGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabledAfter);
                            pglGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &bindAfter);
                            std::cout << "Mesh::Draw -> after manual setup attrib0 enabled=" << enabledAfter << " buffer_binding=" << bindAfter << std::endl;
                        }
                        if (pglGetIntegerv) {
                            int boundElemAfter=0; const unsigned int GL_ELEMENT_ARRAY_BUFFER_BINDING = 0x8895; pglGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &boundElemAfter);
                            std::cout << "Mesh::Draw -> after manual setup GL_ELEMENT_ARRAY_BUFFER_BINDING=" << boundElemAfter << std::endl;
                        }
                    }
                } else {
                    std::cerr << "Mesh::Draw -> glGetVertexAttribiv not available" << std::endl;
                }

            } else {
                std::cerr << "Mesh::Draw -> glGetIntegerv not available" << std::endl;
            }
        }

        // Use the index type chosen at upload time (GL_UNSIGNED_SHORT or GL_UNSIGNED_INT)
        unsigned int drawIndexType = indexType_ ? indexType_ : 0x1405 /*GL_UNSIGNED_INT*/;
        pglDrawElements(GL_TRIANGLES, (int)indices_.size(), drawIndexType, nullptr);

        // Check for GL errors after drawing

        // Check for GL errors after drawing
        {
            using PFNGLGETERRORPROC = unsigned int (APIENTRY*)();
            PFNGLGETERRORPROC pglGetError = nullptr;
            auto addr = (void*)SDL_GL_GetProcAddress("glGetError");
            if (addr) pglGetError = (PFNGLGETERRORPROC)addr;
            if (pglGetError) {
                unsigned int err = pglGetError();
                if (err != 0) {
                    std::cerr << "GL error after pglDrawElements: 0x" << std::hex << err << std::dec << std::endl;
                    if (err == 0x501) {
                        // GL_INVALID_VALUE: try explicit client-memory glDrawElements (indices pointer)
                        std::cout << "Mesh::Draw -> glDrawElements reported GL_INVALID_VALUE; trying client-memory indices fallback" << std::endl;
                        if (pglDrawElements) {
                            // First try native (unsigned int) client indices
                            pglDrawElements(GL_TRIANGLES, (int)indices_.size(), 0x1405 /*GL_UNSIGNED_INT*/, indices_.data());
                            unsigned int err2 = pglGetError();
                            if (err2 == 0) {
                                std::cout << "Mesh::Draw -> client-memory GL_UNSIGNED_INT draw succeeded" << std::endl;
                            } else {
                                std::cerr << "GL error after client-memory GL_UNSIGNED_INT fallback: 0x" << std::hex << err2 << std::dec << std::endl;
                                // Try converting to 16-bit indices as many drivers accept USHORT
                                std::vector<uint16_t> indices16;
                                indices16.reserve(indices_.size());
                                for (uint32_t i : indices_) indices16.push_back((uint16_t)i);
                                pglDrawElements(GL_TRIANGLES, (int)indices16.size(), 0x1403 /*GL_UNSIGNED_SHORT*/, indices16.data());
                                unsigned int err3 = pglGetError();
                                if (err3 == 0) {
                                    std::cout << "Mesh::Draw -> client-memory GL_UNSIGNED_SHORT draw succeeded" << std::endl;
                                } else {
                                    std::cerr << "GL error after client-memory GL_UNSIGNED_SHORT fallback: 0x" << std::hex << err3 << std::dec << std::endl;

                                    // Last resort: create a temporary VAO/EBO with 16-bit indices and try again (mimic diag test)
                                    std::cout << "Mesh::Draw -> creating temporary VAO/EBO with USHORT indices as last resort" << std::endl;
                                    // Resolve needed functions locally
                                    auto addrGenVAO = (void*)SDL_GL_GetProcAddress("glGenVertexArrays");
                                    auto addrBindVAO = (void*)SDL_GL_GetProcAddress("glBindVertexArray");
                                    auto addrGenBuf = (void*)SDL_GL_GetProcAddress("glGenBuffers");
                                    auto addrBindBuf = (void*)SDL_GL_GetProcAddress("glBindBuffer");
                                    auto addrBufData = (void*)SDL_GL_GetProcAddress("glBufferData");
                                    auto addrEnableAttr = (void*)SDL_GL_GetProcAddress("glEnableVertexAttribArray");
                                    auto addrAttribPtr = (void*)SDL_GL_GetProcAddress("glVertexAttribPointer");
                                    auto addrDrawEls = (void*)SDL_GL_GetProcAddress("glDrawElements");
                                    auto addrDeleteBuf = (void*)SDL_GL_GetProcAddress("glDeleteBuffers");
                                    auto addrDeleteVAO = (void*)SDL_GL_GetProcAddress("glDeleteVertexArrays");
                                    if (addrGenVAO && addrBindVAO && addrGenBuf && addrBindBuf && addrBufData && addrEnableAttr && addrAttribPtr && addrDrawEls) {
                                        using PFNGLGENVERTEXARRAYSPROC = void (APIENTRY*)(int, unsigned int*);
                                        using PFNGLBINDVERTEXARRAYPROC = void (APIENTRY*)(unsigned int);
                                        using PFNGLGENBUFFERSPROC = void (APIENTRY*)(int, unsigned int*);
                                        using PFNGLBINDBUFFERPROC = void (APIENTRY*)(unsigned int, unsigned int);
                                        using PFNGLBUFFERDATAPROC = void (APIENTRY*)(unsigned int, ptrdiff_t, const void*, unsigned int);
                                        using PFNGLENABLEVERTEXATTRIBARRAYPROC = void (APIENTRY*)(unsigned int);
                                        using PFNGLVERTEXATTRIBPOINTERPROC = void (APIENTRY*)(unsigned int, int, unsigned int, unsigned char, int, const void*);
                                        using PFNGLDRAWELEMENTSPROC = void (APIENTRY*)(unsigned int, int, unsigned int, const void*);
                                        using PFNGLDELETEBUFFERSPROC = void (APIENTRY*)(int, const unsigned int*);
                                        using PFNGLDELETEVERTEXARRAYSPROC = void (APIENTRY*)(int, const unsigned int*);

                                        auto pglGenVAO2 = (PFNGLGENVERTEXARRAYSPROC)addrGenVAO;
                                        auto pglBindVAO2 = (PFNGLBINDVERTEXARRAYPROC)addrBindVAO;
                                        auto pglGenBuf2 = (PFNGLGENBUFFERSPROC)addrGenBuf;
                                        auto pglBindBuf2 = (PFNGLBINDBUFFERPROC)addrBindBuf;
                                        auto pglBufData2 = (PFNGLBUFFERDATAPROC)addrBufData;
                                        auto pglEnableAttr2 = (PFNGLENABLEVERTEXATTRIBARRAYPROC)addrEnableAttr;
                                        auto pglAttribPtr2 = (PFNGLVERTEXATTRIBPOINTERPROC)addrAttribPtr;
                                        auto pglDrawEls2 = (PFNGLDRAWELEMENTSPROC)addrDrawEls;
                                        auto pglDeleteBuf2 = (PFNGLDELETEBUFFERSPROC)addrDeleteBuf;
                                        auto pglDeleteVAO2 = (PFNGLDELETEVERTEXARRAYSPROC)addrDeleteVAO;

                                        unsigned int tmpVAO=0, tmpVBO=0, tmpEBO=0;
                                        pglGenVAO2(1,&tmpVAO);
                                        pglBindVAO2(tmpVAO);
                                        pglGenBuf2(1,&tmpVBO);
                                        const unsigned int GL_ARRAY_BUFFER = 0x8892; const unsigned int GL_ELEMENT_ARRAY_BUFFER = 0x8893; const unsigned int GL_STATIC_DRAW = 0x88E4; const unsigned int GL_FLOAT = 0x1406;
                                        pglBindBuf2(GL_ARRAY_BUFFER, tmpVBO);
                                        pglBufData2(GL_ARRAY_BUFFER, (ptrdiff_t)(vertices_.size() * sizeof(float)), vertices_.data(), GL_STATIC_DRAW);
                                        pglGenBuf2(1,&tmpEBO);
                                        pglBindBuf2(GL_ELEMENT_ARRAY_BUFFER, tmpEBO);
                                        pglBufData2(GL_ELEMENT_ARRAY_BUFFER, (ptrdiff_t)(indices16.size() * sizeof(uint16_t)), indices16.data(), GL_STATIC_DRAW);
                                        pglEnableAttr2(0);
                                        pglAttribPtr2(0,3,GL_FLOAT,0,0,(const void*)0);

                                        pglDrawEls2(0x0004, (int)indices16.size(), 0x1403 /*GL_UNSIGNED_SHORT*/, nullptr);
                                        // check error
                                        auto addrGetErr = (void*)SDL_GL_GetProcAddress("glGetError");
                                        if (addrGetErr) {
                                            using PFNGLGETERRORPROC = unsigned int (APIENTRY*)(); PFNGLGETERRORPROC pglGetError=(PFNGLGETERRORPROC)addrGetErr; unsigned int e=pglGetError(); if (e!=0) std::cerr<<"Mesh::Draw -> tmp VAO draw error: 0x"<<std::hex<<e<<std::dec<<std::endl; else std::cout<<"Mesh::Draw -> tmp VAO draw (USHORT) succeeded"<<std::endl; }

                                        // cleanup
                                        pglBindVAO2(0);
                                        if (tmpVBO && pglDeleteBuf2) { pglDeleteBuf2(1,&tmpVBO); }
                                        if (tmpEBO && pglDeleteBuf2) { pglDeleteBuf2(1,&tmpEBO); }
                                        if (tmpVAO && pglDeleteVAO2) { pglDeleteVAO2(1,&tmpVAO); }
                                    } else {
                                        std::cerr << "Mesh::Draw -> unable to resolve functions for tmp VAO fallback" << std::endl;
                                    }
                                }
                            }
                        } else {
                            std::cerr << "Mesh::Draw -> glDrawElements not available for fallback" << std::endl;
                        }
                    }
                            }
                        } else {
                            std::cerr << "Mesh::Draw -> glDrawElements not available for fallback" << std::endl;
                        }
                    }
                }
            }
        }
        pglBindVertexArray(0);
    } else {
        // Fallback to client arrays
        std::cout << "Mesh::Draw -> using client arrays fallback" << std::endl;
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
        if (pglDrawElements) pglDrawElements(GL_TRIANGLES, (int)indices_.size(), GL_UNSIGNED_INT, indices_.data()); else std::cerr << "Mesh::Draw -> no glDrawElements available in fallback" << std::endl;

        if (pglDisableClientState) pglDisableClientState(GL_VERTEX_ARRAY);
        if (!normals_.empty() && pglDisableClientState) pglDisableClientState(GL_NORMAL_ARRAY);
    }
}

} // namespace Genesis::Engine
