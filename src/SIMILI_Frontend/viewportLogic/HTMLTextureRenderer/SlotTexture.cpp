#include "SlotTexture.hpp"
#include "HtmlTextureRenderer.hpp"
#include "TextureRendererTest.hpp"
#include <iostream>
#include <windowsx.h>  // For GET_X_LPARAM, GET_Y_LPARAM

namespace {
    const wchar_t* kSlotTextureClassName = L"SIMILI_SlotTexture_Overlay";
    
    // ============================================================================
    // SHADERS FOR SLOT TEXTURE RENDERING
    // ============================================================================
    
    const char* vertexShaderSource = R"(
    #version 330 core
    layout(location = 0) in vec2 aPosition;
    layout(location = 1) in vec2 aTexCoord;
    
    out vec2 TexCoord;
    
    void main()
    {
        gl_Position = vec4(aPosition, 0.0, 1.0);
        TexCoord = aTexCoord;
    }
    )";
    
    const char* fragmentShaderSource = R"(
    #version 330 core
    in vec2 TexCoord;
    out vec4 FragColor;
    
    uniform vec4 uColor;
    uniform sampler2D uTexture;
    uniform bool uUseTexture;
    
    void main()
    {
        if (uUseTexture) {
            vec4 texColor = texture(uTexture, TexCoord);
            FragColor = texColor;
        } else {
            FragColor = uColor;
        }
    }
    )";
}

#ifndef WGL_CONTEXT_MAJOR_VERSION_ARB
#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#endif

typedef HGLRC (WINAPI * PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC hDC, HGLRC hShareContext, const int *attribList);

// ============================================================================
// LIFECYCLE
// ============================================================================

SlotTexture::SlotTexture()
    : hwnd_(nullptr)
    , parent_(nullptr)
    , hdc_(nullptr)
    , gl_context_(nullptr)
    , width_(100)
    , height_(100)
    , z_order_layer_(2)
    , vao_(0)
    , vbo_(0)
    , shader_program_(0)
    , color_r_(1.0f)   
    , color_g_(0.0f)
    , color_b_(0.0f)
    , color_a_(1.0f)    , html_renderer_(nullptr)
    , texture_renderer_(nullptr)
    , use_html_texture_(false)
    , html_browser_ready_(false)
    , rendering_enabled_(false)  // Rendering DISABLED by default
{
}

SlotTexture::~SlotTexture()
{
    being_destroyed_ = true;
    destroy();
}

// ============================================================================
// WINDOW CREATION
// ============================================================================

bool SlotTexture::create(HWND parent, int x, int y, int width, int height, int zOrderLayer)
{
    parent_ = parent;
    width_ = width;
    height_ = height;
    z_order_layer_ = zOrderLayer;
    
    std::cout << "[SlotTexture] Creating window - Parent: " << parent << 
                 ", Pos: (" << x << "," << y << "), Size: " << width << "x" << height << 
                 ", Layer: " << zOrderLayer << std::endl;
    
    // Validate parent window
    if (!parent || !IsWindow(parent)) {
        std::cerr << "[SlotTexture] ERROR: Invalid parent window!" << std::endl;
        return false;
    }
    
    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kSlotTextureClassName;
    
    static bool class_registered = false;
    if (!class_registered) {
        std::cout << "[SlotTexture] Registering window class..." << std::endl;
        if (!RegisterClassExW(&wc)) {
            DWORD err = GetLastError();
            std::cerr << "[SlotTexture] Failed to register window class, error: " << err << std::endl;
            return false;
        }
        class_registered = true;
        std::cout << "[SlotTexture] Window class registered successfully" << std::endl;
    }
    
    std::cout << "[SlotTexture] Creating window with CreateWindowExW..." << std::endl;
    
    // Try creating as popup window first, then set parent and layered style
    // Some Windows versions have issues with WS_EX_LAYERED + WS_CHILD combination
    hwnd_ = CreateWindowExW(
        0,  // No extended styles initially
        kSlotTextureClassName,
        L"SlotTexture Overlay",
        WS_POPUP,  // HIDDEN by default - visibility controlled by show() method
        x, y, width, height,
        nullptr,  // No parent initially
        nullptr,
        GetModuleHandle(nullptr),
        this
    );
    
    if (!hwnd_) {
        DWORD error = GetLastError();
        std::cerr << "[SlotTexture] Failed to create POPUP window, trying CHILD without LAYERED..." << std::endl;
        
        // Fallback: try simple child window without layered
        hwnd_ = CreateWindowExW(
            0,
            kSlotTextureClassName,
            L"SlotTexture Overlay",
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
            x, y, width, height,
            parent,
            nullptr,
            GetModuleHandle(nullptr),
            this
        );
        
        if (!hwnd_) {
            error = GetLastError();
            std::cerr << "[SlotTexture] Failed to create window, error: " << error << std::endl;
            
            // Additional debugging
            std::cerr << "[SlotTexture] CreateWindowExW parameters:" << std::endl;
            std::cerr << "  ExStyle: 0" << std::endl;
            std::cerr << "  ClassName: " << kSlotTextureClassName << std::endl;
            std::cerr << "  Style: WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS" << std::endl;
            std::cerr << "  Position: (" << x << ", " << y << ")" << std::endl;
            std::cerr << "  Size: " << width << " x " << height << std::endl;
            std::cerr << "  Parent HWND: " << parent << std::endl;
            
            return false;
        } else {
            std::cout << "[SlotTexture] Created as CHILD window (no layered transparency)" << std::endl;
        }
    } else {
        // Successfully created as popup, now set parent and layered style
        std::cout << "[SlotTexture] Created as POPUP, setting parent and layered style..." << std::endl;
        
        // Set parent
        SetParent(hwnd_, parent);
        
        // Add layered extended style
        DWORD exStyle = GetWindowLong(hwnd_, GWL_EXSTYLE);
        SetWindowLong(hwnd_, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
        
        // Set layered attributes
        SetLayeredWindowAttributes(hwnd_, RGB(255, 0, 255), 0, LWA_COLORKEY);
        
        std::cout << "[SlotTexture] Successfully configured as layered child window" << std::endl;
    }
    
    std::cout << "[SlotTexture] Window created successfully - HWND: " << hwnd_ << std::endl;
    
    // Get GLFW/shared context
    HGLRC sharedContext = wglGetCurrentContext();
    if (sharedContext) {
        std::cout << "[SlotTexture] Will share context: " << sharedContext << std::endl;
    }
    
    initializeOpenGL(sharedContext);
    ensureProperZOrder();
    
    return true;
}

void SlotTexture::destroy()
{
	if (!hwnd_) return;
	
	std::cout << "\n[SlotTexture] Destroying SlotTexture..." << std::endl;
	
	// CRITICAL: Set being_destroyed_ FIRST to prevent any new render calls
	being_destroyed_ = true;
	rendering_enabled_ = false;
	
	if (html_renderer_)
	{
		html_renderer_->detachTextureRenderer();
		
		if (html_renderer_->getBrowser())
		{
			html_renderer_->getBrowser()->GetHost()->CloseBrowser(true);					

		}
		
		html_renderer_ = nullptr;
	}
	
	HGLRC prevContext = wglGetCurrentContext();
	HDC prevDC = wglGetCurrentDC();
	
	if (gl_context_ && hdc_ && IsWindow(hwnd_))
	{
		BOOL makeCurrentResult = wglMakeCurrent(hdc_, gl_context_);
		if (makeCurrentResult)
		{
			if (texture_renderer_)
			{
				texture_renderer_->cleanup();
				delete texture_renderer_;
				texture_renderer_ = nullptr;
			}
			
			if (vao_) 
			{
				glDeleteVertexArrays(1, &vao_);
				vao_ = 0;
			}
			if (vbo_) 
			{
				glDeleteBuffers(1, &vbo_);
				vbo_ = 0;
			}
			if (shader_program_) 
			{
				glDeleteProgram(shader_program_);
				shader_program_ = 0;
			}
			
			wglMakeCurrent(nullptr, nullptr);
		}
		else
		{
			if (texture_renderer_)
			{
				delete texture_renderer_;
				texture_renderer_ = nullptr;
			}
		}
		
		wglDeleteContext(gl_context_);
		gl_context_ = nullptr;
	}
	else
	{
		if (texture_renderer_)
		{
			delete texture_renderer_;
			texture_renderer_ = nullptr;
		}
		
		if (gl_context_)
		{
			wglDeleteContext(gl_context_);
			gl_context_ = nullptr;
		}
	}
	
	if (prevContext && prevDC)
	{
		wglMakeCurrent(prevDC, prevContext);
	}
	
	if (hdc_ && hwnd_)
	{
		ReleaseDC(hwnd_, hdc_);
		hdc_ = nullptr;
	}
	
	if (hwnd_ && IsWindow(hwnd_))
	{
		DestroyWindow(hwnd_);
		hwnd_ = nullptr;
	}
	
	std::cout << "[SlotTexture] SlotTexture destroyed successfully" << std::endl;
}

// ============================================================================
// OPENGL INITIALIZATION
// ============================================================================

void SlotTexture::initializeOpenGL(HGLRC shareContext)
{
    hdc_ = GetDC(hwnd_);
    
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;
    
    int pixelFormat = ChoosePixelFormat(hdc_, &pfd);
    SetPixelFormat(hdc_, pixelFormat, &pfd);
    
    HGLRC tempContext = wglCreateContext(hdc_);
    wglMakeCurrent(hdc_, tempContext);
    
    PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = 
        (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
    
    if (wglCreateContextAttribsARB) {
        int attribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
            WGL_CONTEXT_MINOR_VERSION_ARB, 3,
            WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            0
        };
        
        gl_context_ = wglCreateContextAttribsARB(hdc_, shareContext, attribs);
        if (gl_context_) {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(tempContext);
            wglMakeCurrent(hdc_, gl_context_);
            std::cout << "[SlotTexture] OpenGL 3.3 context created (shared)" << std::endl;
        }
    }
    
    if (!gl_context_) {
        gl_context_ = tempContext;
    }
    
    glViewport(0, 0, width_, height_);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Set clear color to BLACK (opaque) to avoid magenta transparency
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    
    createQuad();
}

void SlotTexture::createQuad()
{
    // Full coverage quad to fill entire overlay (no black borders)
    float size = 1.0f;
    float vertices[] = {
        // Position     // TexCoord (Y inversé pour CEF)
        -size, -size,   0.0f, 1.0f,  // Bottom-left  -> Top-left texture
         size, -size,   1.0f, 1.0f,  // Bottom-right -> Top-right texture
         size,  size,   1.0f, 0.0f,  // Top-right    -> Bottom-right texture
        -size,  size,   0.0f, 0.0f   // Top-left     -> Bottom-left texture
    };
    
    unsigned int indices[] = {
        0, 1, 2,  // First triangle
        2, 3, 0   // Second triangle
    };
    
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    
    GLuint ebo;
    glGenBuffers(1, &ebo);
    
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    
    // Position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Texture coordinate attribute  
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindVertexArray(0);
    
    // Shaders for CEF texture rendering
    const char* vertexShaderSource = R"(
        #version 330 core
        layout (location = 0) in vec2 aPosition;
        layout (location = 1) in vec2 aTexCoord;
        
        out vec2 TexCoord;
        
        void main() {
            gl_Position = vec4(aPosition, 0.0, 1.0);
            TexCoord = aTexCoord;
        }
    )";
    
    const char* fragmentShaderSource = R"(
        #version 330 core
        in vec2 TexCoord;
        out vec4 FragColor;
        
        uniform vec4 uColor;
        uniform sampler2D uTexture;
        uniform bool uUseTexture;
        
        void main() {
            if (uUseTexture) {
                vec4 texColor = texture(uTexture, TexCoord);
                // Ensure we use the texture color directly without blending
                FragColor = vec4(texColor.rgb, 1.0);
            } else {
                FragColor = uColor;
            }
        }
    )";
    
    std::cout << "[SlotTexture] Full coverage quad created with corrected texture coordinates" << std::endl;
    
    // Compile shaders
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);
    
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);
    
    shader_program_ = glCreateProgram();
    glAttachShader(shader_program_, vertexShader);
    glAttachShader(shader_program_, fragmentShader);
    glLinkProgram(shader_program_);
    
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

// ============================================================================
// RENDERING
// ============================================================================

void SlotTexture::render()
{
    if (being_destroyed_) return;
    if (!gl_context_ || !hwnd_) return;
    if (!rendering_enabled_) return;  
    if (!IsWindowVisible(hwnd_)) return;
    
    // Save current OpenGL context and state to restore later
    HGLRC prevContext = wglGetCurrentContext();
    HDC prevDC = wglGetCurrentDC();
    GLint prevFBO = 0;
    GLint prevViewport[4] = {0, 0, 0, 0};
    
    if (prevContext) 
	{
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
        glGetIntegerv(GL_VIEWPORT, prevViewport);
    }
    
    wglMakeCurrent(hdc_, gl_context_);
    
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    renderQuad();
    
    SwapBuffers(hdc_);
    
    // Restore the previous OpenGL context and state to avoid conflicts with overlay
    if (prevContext && prevDC) {
        wglMakeCurrent(prevDC, prevContext);
        glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
        glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
    }
}

void SlotTexture::renderQuad()
{
    glUseProgram(shader_program_);
    
    // Set color uniform
    GLint colorLocation = glGetUniformLocation(shader_program_, "uColor");
    glUniform4f(colorLocation, color_r_, color_g_, color_b_, color_a_);
    
    // Set texture usage - use HTML texture if available and browser is ready
    GLint useTextureLocation = glGetUniformLocation(shader_program_, "uUseTexture");
    bool shouldUseTexture = use_html_texture_ && html_browser_ready_ && texture_renderer_;
    glUniform1i(useTextureLocation, shouldUseTexture ? 1 : 0);
    
    if (shouldUseTexture) {
        // Bind HTML texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture_renderer_->getTextureId());
        GLint textureLocation = glGetUniformLocation(shader_program_, "uTexture");
        glUniform1i(textureLocation, 0);
    }
    
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void SlotTexture::enableRendering(bool enable)
{
    if (being_destroyed_) return;
    rendering_enabled_ = enable;
    
    // Hide/show the window to prevent black bar when rendering is disabled
    if (hwnd_) {
        ShowWindow(hwnd_, enable ? SW_SHOW : SW_HIDE);
    }
}

// ============================================================================
// WINDOW MANAGEMENT
// ============================================================================

void SlotTexture::setPosition(int x, int y, int width, int height)
{
    if (being_destroyed_) return;
    if (!hwnd_) return;
    
    SetWindowPos(hwnd_, HWND_TOP, x, y, width, height, 
                 SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_NOZORDER);
    
    // Only update if dimensions actually changed
    if (width_ != width || height_ != height) {
        width_ = width;
        height_ = height;
        
        if (gl_context_) {
            wglMakeCurrent(hdc_, gl_context_);
            glViewport(0, 0, width, height);
        }
        
        // Synchronize HTML texture size when SlotTexture is resized
        updateHTMLTextureSize(width, height);
    }
    
    ensureProperZOrder();
}

void SlotTexture::show(bool visible)
{
    if (being_destroyed_) return;
    if (!hwnd_) return;
    
    if (!visible)
    {
        // Clear OpenGL content before hiding to prevent artifacts
        if (gl_context_ && hdc_)
        {
            HGLRC prevContext = wglGetCurrentContext();
            HDC prevDC = wglGetCurrentDC();
            
            wglMakeCurrent(hdc_, gl_context_);
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            SwapBuffers(hdc_);
            
            if (prevContext && prevDC)
            {
                wglMakeCurrent(prevDC, prevContext);
            }
        }
        
        // Hide window properly
        SetWindowPos(hwnd_, NULL, 0, 0, 0, 0,
                     SWP_HIDEWINDOW | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    else
    {
        ShowWindow(hwnd_, SW_SHOW);
        ensureProperZOrder();
    }
}

bool SlotTexture::isVisible() const
{
    if (being_destroyed_) return false;
    return hwnd_ && IsWindowVisible(hwnd_);
}

// ============================================================================
// Z-ORDER MANAGEMENT
// ============================================================================

void SlotTexture::setZOrderLayer(int layer)
{
    z_order_layer_ = layer;
    ensureProperZOrder();
}

void SlotTexture::ensureProperZOrder()
{
    if (being_destroyed_) return;
    if (!hwnd_ || !parent_) return;
    
    // Layer-based Z-order:
    // Layer 0: CEF (bottom)
    // Layer 1: OpenGL viewport
    // Layer 2+: SlotTexture and other overlays (top)
    
    if (z_order_layer_ == 0) {
        SetWindowPos(hwnd_, HWND_BOTTOM, 0, 0, 0, 0, 
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    } else if (z_order_layer_ == 1) {
        // Layer 1: Position after CEF but before layer 2+
        SetWindowPos(hwnd_, parent_, 0, 0, 0, 0, 
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    } else {
        // Layer 2+: Always on top for SlotTexture overlays
        SetWindowPos(hwnd_, HWND_TOP, 0, 0, 0, 0, 
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
    
    // Ensure TOPMOST flag is never set
    DWORD exStyle = GetWindowLongW(hwnd_, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOPMOST) {
        SetWindowLongW(hwnd_, GWL_EXSTYLE, exStyle & ~WS_EX_TOPMOST);
        SetWindowPos(hwnd_, HWND_NOTOPMOST, 0, 0, 0, 0, 
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        std::cout << "[SlotTexture] Removed TOPMOST flag (layer=" << z_order_layer_ << ")" << std::endl;
    }
    
    // CRITICAL: Ensure window is always visible and can receive messages
    if (!IsWindowVisible(hwnd_)) {
        ShowWindow(hwnd_, SW_SHOW);
        std::cout << "[SlotTexture] Forced window visible during Z-order update" << std::endl;
    }
    
    // Force window to be enabled for input messages
    EnableWindow(hwnd_, TRUE);
}

// ============================================================================
// WINDOW PROCEDURE
// ============================================================================

LRESULT CALLBACK SlotTexture::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    SlotTexture* slot = nullptr;
    
    if (msg == WM_CREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        slot = static_cast<SlotTexture*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(slot));
    } else {
        slot = reinterpret_cast<SlotTexture*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    
    if (slot && slot->being_destroyed_) {
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    
    if (slot) {
        switch (msg) {
            case WM_PAINT:
            {
                PAINTSTRUCT ps;
                BeginPaint(hwnd, &ps);
                slot->render();
                EndPaint(hwnd, &ps);
                return 0;
            }
            
            case WM_ERASEBKGND:
                return 1;
                
            case WM_WINDOWPOSCHANGING:
            {
                WINDOWPOS* wp = reinterpret_cast<WINDOWPOS*>(lParam);
                if (wp->hwndInsertAfter == HWND_TOPMOST) {
                    wp->hwndInsertAfter = HWND_TOP;
                }
                break;
            }
            
            case WM_WINDOWPOSCHANGED:
            {
                // After window position changes (like during resize), ensure proper Z-order and message routing
                WINDOWPOS* wp = reinterpret_cast<WINDOWPOS*>(lParam);
                if (wp->flags & (SWP_FRAMECHANGED | SWP_SHOWWINDOW)) {
                    std::cout << "[SlotTexture] Window position changed - ensuring message routing" << std::endl;
                    slot->ensureProperZOrder();
                    
                    // CRITICAL: Force window to stay active for message reception
                    SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, 
                                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
                    
                    // Ensure the window can receive mouse messages
                    DWORD style = GetWindowLong(hwnd, GWL_STYLE);
                    if (!(style & WS_VISIBLE)) {
                        ShowWindow(hwnd, SW_SHOW);
                        std::cout << "[SlotTexture] Forced window visible after position change" << std::endl;
                    }
                    
                    std::cout << "[SlotTexture] Position changed - message routing restored" << std::endl;
                }
                break;
            }
            
            case WM_NCHITTEST:
            {

                return HTTRANSPARENT;
            }
            
            case WM_SIZE:
            {
                // Handle SlotTexture resize events - ensure we maintain proper message routing
                int newWidth = LOWORD(lParam);
                int newHeight = HIWORD(lParam);
                if (newWidth > 0 && newHeight > 0) {
                    std::cout << "[SlotTexture] Resized to: " << newWidth << "x" << newHeight 
                              << " - ensuring message routing integrity" << std::endl;
                    
                    slot->width_ = newWidth;
                    slot->height_ = newHeight;
                    
                    if (slot->gl_context_) {
                        HGLRC prevContext = wglGetCurrentContext();
                        HDC prevDC = wglGetCurrentDC();
                        wglMakeCurrent(slot->hdc_, slot->gl_context_);
                        glViewport(0, 0, newWidth, newHeight);
                        if (prevContext && prevDC) {
                            wglMakeCurrent(prevDC, prevContext);
                        }
                    }
                    
                    // CRITICAL: After resize, force window to regain proper message routing
                    SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, 
                                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
                    
                    // Force refresh the window to ensure it's properly visible and receiving messages
                    InvalidateRect(hwnd, NULL, TRUE);
                    UpdateWindow(hwnd);
                    
                    std::cout << "[SlotTexture] Resize complete - forced window refresh and Z-order update" << std::endl;
                }
                break;
            }
            
            case WM_SETCURSOR:
                return DefWindowProc(slot->parent_, msg, wParam, lParam);
        }
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ============================================================================
// CEF HTML RENDERING
// ============================================================================

void SlotTexture::loadHTML(const std::string& url)
{
    if (!texture_renderer_) 
    {
        texture_renderer_ = new TextureRendererTest();
        // Use actual SlotTexture dimensions for proper scaling
        texture_renderer_->initialize(width_, height_);
        std::cout << "[SlotTexture] TextureRenderer created for HTML rendering (" << width_ << "x" << height_ << ")" << std::endl;
    }
    
    if (!html_renderer_) {
        html_renderer_ = new HtmlTextureRenderer(texture_renderer_);
        html_renderer_->setViewportWindow(hwnd_);
        
        // Set callback to know when browser is ready
        html_renderer_->setOnBrowserCreatedCallback([this](CefRefPtr<CefBrowser> browser) {
            html_browser_ready_ = true;
            std::cout << "[SlotTexture] CEF browser ready - HTML texture active" << std::endl;
        });
        
        std::cout << "[SlotTexture] HtmlTextureRenderer created" << std::endl;
    }
    
    // Use SlotTexture dimensions for browser to ensure proper scaling
    html_renderer_->createBrowser(url, width_, height_);
    std::cout << "[SlotTexture] Loading HTML: " << url << " (viewport: " << width_ << "x" << height_ << ")" << std::endl;
}

void SlotTexture::setUseHTMLTexture(bool useTexture)
{
    use_html_texture_ = useTexture;
    std::cout << "[SlotTexture] HTML texture usage: " << (useTexture ? "enabled" : "disabled") << std::endl;
}

void SlotTexture::updateHTMLTextureSize(int width, int height)
{
	width_ = width;
	height_ = height;
	
	// Update texture renderer size
	if (texture_renderer_) {
		texture_renderer_->initialize(width, height);
	}
	
	// Update HTML renderer size
	if (html_renderer_) {
		html_renderer_->updateSize(width, height);
	}
	
	std::cout << "[SlotTexture] HTML texture size updated to: " << width << "x" << height << std::endl;
}

void SlotTexture::onBrowserCreated(CefBrowser* browser)
{
    html_browser_ready_ = true;
    std::cout << "[SlotTexture] Browser created and ready" << std::endl;
}
