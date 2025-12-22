#include "engine/OpenGLRenderer.h"
#include <SDL3/SDL.h>
#include <iostream>

namespace Genesis::Engine {

bool OpenGLRenderer::Init(SDL_Window* window, SDL_GLContext glContext) {
    if (!window || !glContext) {
        std::cerr << "OpenGLRenderer: invalid window or GL context" << std::endl;
        return false;
    }

    m_window = window;
    m_context = glContext;

    // Make sure the GL context is current on this thread
    if (SDL_GL_MakeCurrent(m_window, m_context) != 0) {
        std::cerr << "SDL_GL_MakeCurrent failed: " << SDL_GetError() << std::endl;
        return false;
    }

    // Basic GL init
    glViewport(0, 0, 1280, 720);
    glClearColor(0.1f, 0.12f, 0.15f, 1.0f);
    glEnable(GL_DEPTH_TEST);

    // Build a simple default shader (fallback embedded sources)
    const std::string defaultVert = R"(
        #version 120
        attribute vec3 aPos;
        attribute vec3 aNormal;
        varying vec3 vNormal;
        void main() {
            vNormal = aNormal;
            gl_Position = gl_ModelViewProjectionMatrix * vec4(aPos, 1.0);
        }
    )";

    const std::string defaultFrag = R"(
        #version 120
        varying vec3 vNormal;
        void main() {
            vec3 n = normalize(vNormal);
            float light = max(dot(n, vec3(0.0,0.0,1.0)), 0.0);
            gl_FragColor = vec4(vec3(0.6,0.7,1.0) * light, 1.0);
        }
    )";

    auto s = Shader::FromSource(defaultVert, defaultFrag);
    if (!s) {
        std::cerr << "Warning: default shader failed to compile; falling back to fixed-function pipeline" << std::endl;
    } else {
        m_defaultShader = std::make_shared<Shader>(std::move(*s));
    }

    return true;
}

void OpenGLRenderer::BeginFrame() {
    if (SDL_GL_MakeCurrent(m_window, m_context) != 0) {
        std::cerr << "SDL_GL_MakeCurrent failed in BeginFrame: " << SDL_GetError() << std::endl;
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Ensure the default shader is active where available
    if (m_defaultShader) m_defaultShader->Use();
}

void OpenGLRenderer::EndFrame() {
    SDL_GL_SwapWindow(m_window);
}

void OpenGLRenderer::Shutdown() {
    // Nothing platform-specific here; Window owns the GL context
    m_window = nullptr;
    m_context = nullptr;
}

} // namespace Genesis::Engine
