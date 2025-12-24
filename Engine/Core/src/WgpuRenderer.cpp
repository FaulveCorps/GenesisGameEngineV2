#include "engine/WgpuRenderer.h"
#include <iostream>
#include <SDL_syswm.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <cstring>
#include <fstream>

namespace Genesis::Engine {

// Device error callback for uncaptured device errors
static void OnDeviceError(const WGPUDevice* /*device*/, WGPUErrorType type, WGPUStringView message, void* userdata1, void* /*userdata2*/) {
    // If userdata1 is a WgpuRenderer*, forward to instance handler for proper state updates
    if (userdata1) {
        WgpuRenderer* self = reinterpret_cast<WgpuRenderer*>(userdata1);
        if (self) {
            self->HandleUncapturedDeviceError(type, message);
            return;
        }
    }

    char buf[1024];
    int n = snprintf(buf, sizeof(buf), "%.*s", (int)message.length, message.data);
    (void)n;
    std::cerr << "WgpuRenderer: device error: " << buf << std::endl;
}


void WgpuRenderer::HandleUncapturedDeviceError(WGPUErrorType type, WGPUStringView message) {
    std::string msg;
    msg.assign(message.data, message.length);
    std::cerr << "WgpuRenderer: uncaptured device error: " << msg << std::endl;

    if (type == WGPUErrorType_DeviceLost) {
        std::cerr << "WgpuRenderer: device lost detected; marking device as lost and scheduling reinit" << std::endl;
        m_deviceLost = true;
        m_initialized = false;
    }
}

bool WgpuRenderer::Init(SDL_Window* window, SDL_GLContext /*glContext*/) {
#ifdef HAVE_WGPU
    if (!window) return false;

    // Store SDL window and get native handle
    m_window = window;
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(window, &wmInfo)) {
        std::cerr << "WgpuRenderer: SDL_GetWindowWMInfo failed: " << SDL_GetError() << std::endl;
        return false;
    }
    HWND hwnd = wmInfo.info.win.window;

    try {
        // Create Dawn instance and enumerate adapters
        m_instance.SetBackendValidationLevel(dawn::native::BackendValidationLevel::Disabled);
        auto adapters = m_instance.EnumerateAdapters();
        if (adapters.empty()) {
            std::cerr << "WgpuRenderer: no adapters found" << std::endl;
            return false;
        }

        // Pick first adapter
        m_adapter = adapters[0];

        // Create a device (attach uncaptured error callback)
        WGPUDeviceDescriptor deviceDesc = WGPU_DEVICE_DESCRIPTOR_INIT;
        deviceDesc.uncapturedErrorCallbackInfo2.callback = &OnDeviceError;
        deviceDesc.uncapturedErrorCallbackInfo2.userdata1 = this;
        deviceDesc.uncapturedErrorCallbackInfo2.userdata2 = nullptr;

        WGPUDevice device = m_adapter.CreateDevice(&deviceDesc);
        if (!device) {
            std::cerr << "WgpuRenderer: adapter.CreateDevice returned null" << std::endl;
            return false;
        }
        m_device = device;

        // Get the device queue
        m_queue = wgpuDeviceGetQueue(m_device);

        // Attempt to load WGSL shaders from assets; otherwise fall back to embedded strings
        std::string vs_code;
        std::ifstream vs_file("assets/shaders/wgpu/triangle.vert.wgsl");
        if (vs_file) {
            vs_code.assign(std::istreambuf_iterator<char>(vs_file), std::istreambuf_iterator<char>());
        } else {
            vs_code = R"wgsl(
                @stage(vertex) fn vs_main(@builtin(vertex_index) VertexIndex : u32) -> @builtin(position) vec4<f32> {
                    var pos = array<vec2<f32>, 3>(vec2<f32>(0.0, 0.5), vec2<f32>(-0.5,-0.5), vec2<f32>(0.5,-0.5));
                    let p : vec2<f32> = pos[VertexIndex];
                    return vec4<f32>(p, 0.0, 1.0);
                }
            )wgsl";
        }

        std::string fs_code;
        std::ifstream fs_file("assets/shaders/wgpu/triangle.frag.wgsl");
        if (fs_file) {
            fs_code.assign(std::istreambuf_iterator<char>(fs_file), std::istreambuf_iterator<char>());
        } else {
            fs_code = R"wgsl(
                @stage(fragment) fn fs_main() -> @location(0) vec4<f32> {
                    return vec4<f32>(1.0, 0.0, 0.0, 1.0);
                }
            )wgsl";
        }

        WGPUShaderSourceWGSL vs_wgsl = WGPU_SHADER_SOURCE_WGSL_INIT;
        vs_wgsl.code.data = vs_code.c_str();
        vs_wgsl.code.length = static_cast<size_t>(vs_code.size());

        WGPUShaderModuleDescriptor vs_desc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
        vs_desc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&vs_wgsl);
        m_vsModule = wgpuDeviceCreateShaderModule(m_device, &vs_desc);
        if (!m_vsModule) {
            std::cerr << "WgpuRenderer: vertex shader creation failed" << std::endl;
            return false;
        }

        WGPUShaderSourceWGSL fs_wgsl = WGPU_SHADER_SOURCE_WGSL_INIT;
        fs_wgsl.code.data = fs_code.c_str();
        fs_wgsl.code.length = static_cast<size_t>(fs_code.size());

        WGPUShaderModuleDescriptor fs_desc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
        fs_desc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&fs_wgsl);
        m_fsModule = wgpuDeviceCreateShaderModule(m_device, &fs_desc);
        if (!m_fsModule) {
            std::cerr << "WgpuRenderer: fragment shader creation failed" << std::endl;
            // cleanup vs module
            wgpuShaderModuleRelease(m_vsModule);
            m_vsModule = nullptr;
            return false;
        }

        // Create a basic pipeline layout
        WGPUPipelineLayoutDescriptor plDesc = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
        WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(m_device, &plDesc);

        // Vertex state
        WGPUVertexState vertexState = WGPU_VERTEX_STATE_INIT;
        vertexState.module = m_vsModule;
        WGPUStringView vsEntry = WGPU_STRING_VIEW_INIT;
        vsEntry.data = "vs_main";
        vsEntry.length = strlen(vsEntry.data);
        vertexState.entryPoint = vsEntry;

        // Fragment state
        WGPUColorTargetState colorTarget = WGPU_COLOR_TARGET_STATE_INIT;
        colorTarget.format = WGPUTextureFormat_BGRA8Unorm;
        colorTarget.writeMask = WGPUColorWriteMask_All;

        WGPUFragmentState fragmentState = WGPU_FRAGMENT_STATE_INIT;
        fragmentState.module = m_fsModule;
        WGPUStringView fsEntry = WGPU_STRING_VIEW_INIT;
        fsEntry.data = "fs_main";
        fsEntry.length = strlen(fsEntry.data);
        fragmentState.entryPoint = fsEntry;
        fragmentState.targetCount = 1;
        fragmentState.targets = &colorTarget;

        // Pipeline descriptor
        WGPURenderPipelineDescriptor rpDesc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
        rpDesc.layout = pipelineLayout;
        rpDesc.vertex = vertexState;
        rpDesc.fragment = &fragmentState;
        rpDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;

        m_pipeline = wgpuDeviceCreateRenderPipeline(m_device, &rpDesc);
        if (!m_pipeline) {
            std::cerr << "WgpuRenderer: pipeline creation failed" << std::endl;
            wgpuShaderModuleRelease(m_vsModule);
            wgpuShaderModuleRelease(m_fsModule);
            m_vsModule = m_fsModule = nullptr;
            if (pipelineLayout) wgpuPipelineLayoutRelease(pipelineLayout);
            return false;
        }

        // We no longer need the temporary pipeline layout handle
        if (pipelineLayout) wgpuPipelineLayoutRelease(pipelineLayout);

        // Create a Win32 surface for presenting to the SDL window (if available)
        WGPUSurfaceDescriptorFromWindowsHWND fromWindowsHWND;
        memset(&fromWindowsHWND, 0, sizeof(fromWindowsHWND));
        fromWindowsHWND.chain.next = nullptr;
        fromWindowsHWND.chain.sType = WGPUSType_SurfaceDescriptorFromWindowsHWND;
        fromWindowsHWND.hinstance = GetModuleHandle(NULL);
        fromWindowsHWND.hwnd = hwnd;

        WGPUSurfaceDescriptor surfaceDescriptor;
        memset(&surfaceDescriptor, 0, sizeof(surfaceDescriptor));
        surfaceDescriptor.nextInChain = &fromWindowsHWND.chain;
        surfaceDescriptor.label = nullptr;

        m_surface = wgpuInstanceCreateSurface(m_instance.Get(), &surfaceDescriptor);
        if (!m_surface) {
            std::cerr << "WgpuRenderer: warning: failed to create WGPU surface (presentation disabled)" << std::endl;
            m_surfaceConfigured = false;
        } else {
            m_surfaceConfigured = false;
            m_surfaceWidth = 0;
            m_surfaceHeight = 0;
            m_surfaceFormat = WGPUTextureFormat_BGRA8Unorm;
            std::cout << "WgpuRenderer: surface created" << std::endl;
        }

        m_initialized = true;
        std::cout << "WgpuRenderer: initialized (device & pipeline created)" << std::endl;
        return true;
    } catch (const std::exception& ex) {
        std::cerr << "WgpuRenderer: exception during Init: " << ex.what() << std::endl;
        return false;
    }
#else
    std::cerr << "WgpuRenderer: wgpu not available (HAVE_WGPU not defined)" << std::endl;
    return false;
#endif
}

void WgpuRenderer::BeginFrame() {
#ifdef HAVE_WGPU
    if (!m_initialized) return;

    // If the device was reported lost, try to reinitialize on the next frame
    if (m_deviceLost) {
        std::cerr << "WgpuRenderer: device lost; attempting reinitialize" << std::endl;
        // Try a graceful shutdown and reinit sequence
        Shutdown();
        if (!Init(m_window, nullptr)) {
            std::cerr << "WgpuRenderer: reinitialize failed; will retry on next frame" << std::endl;
            return;
        }
        // Clear the device-lost flag on successful reinit
        m_deviceLost = false;
    }

    // Window size
    int w = 0, h = 0;
    if (m_window) {
        SDL_GetWindowSize(m_window, &w, &h);
    }
    if (w == 0 || h == 0) {
        w = 640; h = 480;
    }

    // If we have a real surface, render directly to it and present
    if (m_surface) {
        // Process any pending instance events (necessary on some backends)
        dawn::native::InstanceProcessEvents(m_instance.Get());

        // Reconfigure surface if size changed
        if (!m_surfaceConfigured || w != m_surfaceWidth || h != m_surfaceHeight) {
            if (m_surfaceConfigured) {
                wgpuSurfaceUnconfigure(m_surface);
                m_surfaceConfigured = false;
            }

            if (w != 0 && h != 0) {
                WGPUSurfaceConfiguration config;
                memset(&config, 0, sizeof(config));
                config.device = m_device;
                config.format = m_surfaceFormat;
                config.usage = WGPUTextureUsage_RenderAttachment;
                config.width = static_cast<uint32_t>(w);
                config.height = static_cast<uint32_t>(h);
                config.presentMode = WGPUPresentMode_Fifo;

                wgpuSurfaceConfigure(m_surface, &config);
                m_surfaceConfigured = true;
                m_surfaceWidth = w;
                m_surfaceHeight = h;
            } else {
                // Nothing to render
                return;
            }
        }

        // Acquire current swapchain texture
        WGPUSurfaceTexture surfaceTex;
        wgpuSurfaceGetCurrentTexture(m_surface, &surfaceTex);
        if (surfaceTex.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
            std::cerr << "WgpuRenderer: cannot acquire next swap chain texture" << std::endl;
            return;
        }

        // Create a view for the surface texture
        WGPUTextureView surfaceView = wgpuTextureCreateView(surfaceTex.texture, nullptr);
        if (!surfaceView) {
            std::cerr << "WgpuRenderer: failed to create surface texture view" << std::endl;
            if (surfaceTex.texture) { wgpuTextureRelease(surfaceTex.texture); }
            return;
        }

        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(m_device, nullptr);
        if (!encoder) {
            std::cerr << "WgpuRenderer: failed to create command encoder" << std::endl;
            wgpuTextureViewRelease(surfaceView);
            if (surfaceTex.texture) { wgpuTextureRelease(surfaceTex.texture); }
            return;
        }

        WGPURenderPassColorAttachment colorAttachment = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
        colorAttachment.view = surfaceView;
        colorAttachment.loadOp = WGPULoadOp_Clear;
        colorAttachment.storeOp = WGPUStoreOp_Store;
        colorAttachment.clearValue = WGPU_COLOR_INIT;
        colorAttachment.clearValue.r = 0.1;
        colorAttachment.clearValue.g = 0.2;
        colorAttachment.clearValue.b = 0.3;
        colorAttachment.clearValue.a = 1.0;

        WGPURenderPassDescriptor rpDesc = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
        rpDesc.colorAttachmentCount = 1;
        rpDesc.colorAttachments = &colorAttachment;

        WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &rpDesc);
        if (!pass) {
            std::cerr << "WgpuRenderer: failed to begin render pass" << std::endl;
            wgpuCommandEncoderFinish(encoder, nullptr);
            wgpuTextureViewRelease(surfaceView);
            if (surfaceTex.texture) { wgpuTextureRelease(surfaceTex.texture); }
            return;
        }

        wgpuRenderPassEncoderSetViewport(pass, 0.f, 0.f, static_cast<float>(w), static_cast<float>(h), 0.f, 1.f);
        wgpuRenderPassEncoderSetPipeline(pass, m_pipeline);
        wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
        wgpuRenderPassEncoderEnd(pass);

        WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, nullptr);
        if (cmd) {
            wgpuQueueSubmit(m_queue, 1, &cmd);
            wgpuCommandBufferRelease(cmd);
        }
        wgpuCommandEncoderRelease(encoder);
        wgpuTextureViewRelease(surfaceView);

        // Present
        wgpuSurfacePresent(m_surface);

        // Release the acquired swapchain texture to avoid leaking references
        if (surfaceTex.texture) {
            wgpuTextureRelease(surfaceTex.texture);
        }

    } else {
        // Fallback: create an offscreen render target sized to the SDL window
        WGPUTextureDescriptor texDesc = WGPU_TEXTURE_DESCRIPTOR_INIT;
        texDesc.dimension = WGPUTextureDimension_2D;
        texDesc.size.width = static_cast<uint32_t>(w);
        texDesc.size.height = static_cast<uint32_t>(h);
        texDesc.size.depthOrArrayLayers = 1;
        texDesc.mipLevelCount = 1;
        texDesc.sampleCount = 1;
        texDesc.format = WGPUTextureFormat_BGRA8Unorm;
        texDesc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;

        WGPUTexture target = wgpuDeviceCreateTexture(m_device, &texDesc);
        if (!target) {
            std::cerr << "WgpuRenderer: failed to create target texture" << std::endl;
            return;
        }

        WGPUTextureView view = wgpuTextureCreateView(target, nullptr);
        if (!view) {
            std::cerr << "WgpuRenderer: failed to create texture view" << std::endl;
            wgpuTextureDestroy(target);
            return;
        }

        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(m_device, nullptr);
        if (!encoder) {
            std::cerr << "WgpuRenderer: failed to create command encoder" << std::endl;
            wgpuTextureViewRelease(view);
            wgpuTextureDestroy(target);
            return;
        }

        WGPURenderPassColorAttachment colorAttachment = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
        colorAttachment.view = view;
        colorAttachment.loadOp = WGPULoadOp_Clear;
        colorAttachment.storeOp = WGPUStoreOp_Store;
        colorAttachment.clearValue = WGPU_COLOR_INIT;
        colorAttachment.clearValue.r = 0.1;
        colorAttachment.clearValue.g = 0.2;
        colorAttachment.clearValue.b = 0.3;
        colorAttachment.clearValue.a = 1.0;

        WGPURenderPassDescriptor rpDesc = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
        rpDesc.colorAttachmentCount = 1;
        rpDesc.colorAttachments = &colorAttachment;

        WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &rpDesc);
        if (!pass) {
            std::cerr << "WgpuRenderer: failed to begin render pass" << std::endl;
            wgpuCommandEncoderFinish(encoder, nullptr);
            wgpuTextureViewRelease(view);
            wgpuTextureDestroy(target);
            return;
        }

        wgpuRenderPassEncoderSetPipeline(pass, m_pipeline);
        wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
        wgpuRenderPassEncoderEnd(pass);

        WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, nullptr);
        if (cmd) {
            wgpuQueueSubmit(m_queue, 1, &cmd);
            wgpuCommandBufferRelease(cmd);
        }

        // Cleanup (destroy resources; in real usage keep until GPU work finishes)
        wgpuTextureViewRelease(view);
        wgpuTextureDestroy(target);
    }

#else
    (void)0;
#endif
}

void WgpuRenderer::EndFrame() {
#ifdef HAVE_WGPU
    if (!m_initialized) return;
    // tick device to allow any background work and processing
    dawn::native::DeviceTick(m_device);
    // (No swapchain present for now)
#else
    (void)0;
#endif
}

void WgpuRenderer::Shutdown() {
#ifdef HAVE_WGPU
    if (!m_initialized) return;

    if (m_pipeline) {
        wgpuRenderPipelineRelease(m_pipeline);
        m_pipeline = nullptr;
    }
    if (m_vsModule) {
        wgpuShaderModuleRelease(m_vsModule);
        m_vsModule = nullptr;
    }
    if (m_fsModule) {
        wgpuShaderModuleRelease(m_fsModule);
        m_fsModule = nullptr;
    }

    // Unconfigure and release surface if present
    if (m_surface) {
        // Unconfigure will destroy any created swapchain textures
        wgpuSurfaceUnconfigure(m_surface);
        // Release the surface reference
        wgpuSurfaceRelease(m_surface);
        m_surface = nullptr;
        m_surfaceConfigured = false;
    }

    if (m_device) {
        wgpuDeviceRelease(m_device);
        m_device = nullptr;
    }

    m_initialized = false;
    std::cout << "WgpuRenderer: shutdown" << std::endl;
#else
    std::cerr << "WgpuRenderer: not initialized or not available" << std::endl;
#endif
}

} // namespace Genesis::Engine
