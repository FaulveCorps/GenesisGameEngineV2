#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <iostream>
#include <vector>

// Minimal dynamic GL function loader and typedefs (copied style from Mesh.cpp)
static bool ResolveGLFunction(void** fnPtr, const char* name) {
    if (*fnPtr) return true;
    auto addr = (void*)SDL_GL_GetProcAddress(name);
    if (!addr) return false;
    *fnPtr = addr;
    return true;
}

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
using PFNGLDRAWELEMENTSPROC = void (APIENTRY*)(unsigned int, int, unsigned int, const void*);
using PFNGLISVERTEXARRAYPROC = unsigned char (APIENTRY*)(unsigned int);
using PFNGLGETINTEGERVPROC = void (APIENTRY*)(unsigned int, int*);
using PFNGLGETVERTEXATTRIBIVPROC = void (APIENTRY*)(unsigned int, unsigned int, int*);
using PFNGLGETERRORPROC = unsigned int (APIENTRY*)();

static PFNGLGENVERTEXARRAYSPROC pglGenVertexArrays = nullptr;
static PFNGLBINDVERTEXARRAYPROC pglBindVertexArray = nullptr;
static PFNGLGENBUFFERSPROC pglGenBuffers = nullptr;
static PFNGLBINDBUFFERPROC pglBindBuffer = nullptr;
static PFNGLBUFFERDATAPROC pglBufferData = nullptr;
static PFNGLENABLEVERTEXATTRIBARRAYPROC pglEnableVertexAttribArray = nullptr;
static PFNGLVERTEXATTRIBPOINTERPROC pglVertexAttribPointer = nullptr;
static PFNGLDRAWELEMENTSPROC pglDrawElements = nullptr;
static PFNGLISVERTEXARRAYPROC pglIsVertexArray = nullptr;
static PFNGLGETINTEGERVPROC pglGetIntegerv = nullptr;
static PFNGLGETVERTEXATTRIBIVPROC pglGetVertexAttribiv = nullptr;
static PFNGLGETERRORPROC pglGetErrorPtr = nullptr;

int main(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window* win = SDL_CreateWindow("GL VAO Repro", 640, 480, SDL_WINDOW_OPENGL);
    if (!win) { std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl; return 1; }
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    if (!ctx) { std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << std::endl; return 1; }

    // Resolve functions
    ResolveGLFunction((void**)&pglGenVertexArrays, "glGenVertexArrays");
    ResolveGLFunction((void**)&pglBindVertexArray, "glBindVertexArray");
    ResolveGLFunction((void**)&pglGenBuffers, "glGenBuffers");
    ResolveGLFunction((void**)&pglBindBuffer, "glBindBuffer");
    ResolveGLFunction((void**)&pglBufferData, "glBufferData");
    ResolveGLFunction((void**)&pglEnableVertexAttribArray, "glEnableVertexAttribArray");
    ResolveGLFunction((void**)&pglVertexAttribPointer, "glVertexAttribPointer");
    ResolveGLFunction((void**)&pglDrawElements, "glDrawElements");
    ResolveGLFunction((void**)&pglIsVertexArray, "glIsVertexArray");
    ResolveGLFunction((void**)&pglGetIntegerv, "glGetIntegerv");
    ResolveGLFunction((void**)&pglGetVertexAttribiv, "glGetVertexAttribiv");
    ResolveGLFunction((void**)&pglGetErrorPtr, "glGetError");

    std::cout << "GL function ptrs: genVAO=" << (void*)pglGenVertexArrays << " bindVAO=" << (void*)pglBindVertexArray << " genBuf=" << (void*)pglGenBuffers << " bindBuf=" << (void*)pglBindBuffer << " drawE=" << (void*)pglDrawElements << std::endl;

    // Create VAO/VBO/EBO
    unsigned int vao=0, vbo=0, ebo=0;
    pglGenVertexArrays(1, &vao);
    pglBindVertexArray(vao);

    pglGenBuffers(1, &vbo);
    pglBindBuffer(0x8892 /*GL_ARRAY_BUFFER*/, vbo);
    float verts[] = { 0.0f,  0.5f, 0.0f,
                     -0.5f, -0.5f, 0.0f,
                      0.5f, -0.5f, 0.0f };
    pglBufferData(0x8892 /*GL_ARRAY_BUFFER*/, sizeof(verts), verts, 0x88E4 /*GL_STATIC_DRAW*/);

    pglEnableVertexAttribArray(0);
    pglVertexAttribPointer(0, 3, 0x1406 /*GL_FLOAT*/, 0, 3*sizeof(float), (const void*)0);

    pglGenBuffers(1, &ebo);
    pglBindBuffer(0x8893 /*GL_ELEMENT_ARRAY_BUFFER*/, ebo);
    unsigned int indices_uint[] = {0,1,2};
    unsigned short indices_ushort[] = {0,1,2};
    pglBufferData(0x8893 /*GL_ELEMENT_ARRAY_BUFFER*/, sizeof(indices_uint), indices_uint, 0x88E4 /*GL_STATIC_DRAW*/);

    // Query after upload
    std::cout << "After upload: glIsVertexArray(" << vao << ")=" << (int)pglIsVertexArray(vao) << std::endl;
    int elemBinding = 0; pglGetIntegerv(0x8895 /*GL_ELEMENT_ARRAY_BUFFER_BINDING*/, &elemBinding);
    int vaoBinding = 0; pglGetIntegerv(0x85B5 /*GL_VERTEX_ARRAY_BINDING*/, &vaoBinding);
    int attrib0_enabled_i = 0; pglGetVertexAttribiv(0, 0x8622 /*GL_VERTEX_ATTRIB_ARRAY_ENABLED*/, &attrib0_enabled_i);
    unsigned char attrib0_enabled = (unsigned char)attrib0_enabled_i;
    std::cout << "GL_ELEMENT_ARRAY_BUFFER_BINDING=" << elemBinding << " GL_VERTEX_ARRAY_BINDING=" << vaoBinding << " attrib0 enabled=" << (int)attrib0_enabled << std::endl;

    // Unbind VAO simulating other engine ops
    pglBindVertexArray(0);
    pglBindBuffer(0x8892 /*GL_ARRAY_BUFFER*/, 0);

    // Print function pointers again
    std::cout << "Func ptrs at draw: genVAO=" << (void*)pglGenVertexArrays << " bindVAO=" << (void*)pglBindVertexArray << " drawE=" << (void*)pglDrawElements << std::endl;

    // Rebind VAO and query state
    pglBindVertexArray(vao);
    std::cout << "At draw - glIsVertexArray(" << vao << ")=" << (int)pglIsVertexArray(vao) << std::endl;
    pglGetIntegerv(0x8895 /*GL_ELEMENT_ARRAY_BUFFER_BINDING*/, &elemBinding);
    pglGetIntegerv(0x85B5 /*GL_VERTEX_ARRAY_BINDING*/, &vaoBinding);
    int attrib0_enabled_i2 = 0; pglGetVertexAttribiv(0, 0x8622 /*GL_VERTEX_ATTRIB_ARRAY_ENABLED*/, &attrib0_enabled_i2);
    attrib0_enabled = (unsigned char)attrib0_enabled_i2;
    std::cout << "At draw - GL_ELEMENT_ARRAY_BUFFER_BINDING=" << elemBinding << " GL_VERTEX_ARRAY_BINDING=" << vaoBinding << " attrib0 enabled=" << (int)attrib0_enabled << std::endl;

    unsigned int err = pglGetErrorPtr();
    std::cout << "Before draw glGetError()=" << std::hex << err << std::dec << std::endl;

    std::cout << "Attempting glDrawElements with GL_UNSIGNED_INT..." << std::endl;
    pglDrawElements(0x0004 /*GL_TRIANGLES*/, 3, 0x1405 /*GL_UNSIGNED_INT*/, nullptr);
    err = pglGetErrorPtr();
    std::cout << "After UINT draw glGetError()=" << std::hex << err << std::dec << std::endl;

    std::cout << "Attempting glDrawElements with GL_UNSIGNED_SHORT..." << std::endl;
    // re-upload USHORT indices to EBO
    pglBindBuffer(0x8893 /*GL_ELEMENT_ARRAY_BUFFER*/, ebo);
    pglBufferData(0x8893 /*GL_ELEMENT_ARRAY_BUFFER*/, sizeof(indices_ushort), indices_ushort, 0x88E4 /*GL_STATIC_DRAW*/);
    pglDrawElements(0x0004 /*GL_TRIANGLES*/, 3, 0x1403 /*GL_UNSIGNED_SHORT*/, nullptr);
    err = pglGetErrorPtr();
    std::cout << "After USHORT draw glGetError()=" << std::hex << err << std::dec << std::endl;

    SDL_Delay(2000);

    SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();

    return 0;
}
