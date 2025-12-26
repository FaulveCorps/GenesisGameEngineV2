#include "engine/Window.h"
#include <iostream>

namespace Genesis::Engine {

Window::Window() = default;
Window::~Window() { Shutdown(); }

bool Window::Init(const std::string& title, int width, int height) {
    std::cout << "Initializing SDL with VIDEO subsystem..." << std::endl;
    
    // Get SDL2 version info
    SDL_version ver{};
    SDL_GetVersion(&ver);
    std::cout << "SDL version: " << (int)ver.major << "." << (int)ver.minor << "." << (int)ver.patch << std::endl;
    
    // Enable SDL logging for diagnostics (SDL2)
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);
    
    // First try initializing just the events subsystem to verify SDL works
    std::cout << "Testing basic SDL initialization (events only)..." << std::endl;
    SDL_ClearError(); // Clear any previous errors
    int events_result = SDL_Init(SDL_INIT_EVENTS);
    const char* events_error = SDL_GetError();
    
    std::cout << "SDL_Init(EVENTS) returned: " << events_result << std::endl;
    std::cout << "SDL Error after EVENTS init: '" << (events_error ? events_error : "(none)") << "'" << std::endl;
    
    if (events_result != 0) {
        std::cerr << "\n=== CRITICAL SDL FAILURE ===" << std::endl;
        std::cerr << "SDL cannot initialize even basic subsystems" << std::endl;
        std::cerr << "This indicates a corrupted SDL2.dll or system incompatibility" << std::endl;
        std::cerr << "\nPossible solutions:" << std::endl;
        std::cerr << "1. Rebuild vcpkg SDL2: vcpkg remove sdl2 && vcpkg install sdl2" << std::endl;
        std::cerr << "2. Check Windows Event Viewer for application errors" << std::endl;
        std::cerr << "3. Run as Administrator to test permissions" << std::endl;
        std::cerr << "4. This environment may not support SDL (WSL/container/restricted)" << std::endl;
        return false;
    }
    std::cout << "Events subsystem initialized successfully!" << std::endl;
    SDL_Quit();
    
    // Now try VIDEO subsystem
    std::cout << "\nInitializing VIDEO subsystem..." << std::endl;
    SDL_ClearError();
    int init_result = SDL_Init(SDL_INIT_VIDEO);
    const char* video_error = SDL_GetError();
    
    std::cout << "SDL_Init(VIDEO) returned: " << init_result << std::endl;
    std::cout << "SDL Error after VIDEO init: '" << (video_error ? video_error : "(none)") << "'" << std::endl;
    
    if (init_result != 0) {
        std::cerr << "\nVIDEO subsystem failed (EVENTS worked, VIDEO failed)" << std::endl;
        std::cerr << "This indicates missing graphics drivers or headless environment" << std::endl;
        return false;
    }
    
    std::cout << "SDL initialized successfully" << std::endl;

    // Request OpenGL 3.3 Core Profile for better compatibility
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    std::cout << "Creating window: " << title << " (" << width << "x" << height << ")" << std::endl;

    // SDL2 requires position arguments
    m_window = SDL_CreateWindow(title.c_str(),
                                SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED,
                                width,
                                height,
                                SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!m_window) {
        const char* error = SDL_GetError();
        std::cerr << "SDL_CreateWindow failed: " << (error ? error : "(no error message)") << std::endl;
        std::cerr << "Note: Ensure graphics drivers are up to date" << std::endl;
        SDL_Quit();
        return false;
    }

    std::cout << "Window created successfully" << std::endl;

    // Create a GL context (used by future renderer)
    std::cout << "Creating OpenGL context..." << std::endl;
    m_glContext = SDL_GL_CreateContext(m_window);
    if (!m_glContext) {
        const char* error = SDL_GetError();
        std::cerr << "SDL_GL_CreateContext failed: " << (error ? error : "(no error message)") << std::endl;
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
        SDL_Quit();
        return false;
    }

    std::cout << "OpenGL context created successfully" << std::endl;
    std::cout << "Window initialized: " << width << "x" << height << std::endl;
    return true;
}

void Window::Shutdown() {
    if (m_glContext) {
        SDL_GL_DeleteContext(m_glContext);
        m_glContext = nullptr;
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    SDL_Quit();
}

bool Window::PollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT)
            return false;
        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE)
            return false;
    }
    return true;
}

} // namespace Genesis::Engine
