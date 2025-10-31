#include "overlay_viewport.hpp"
#include <iostream>
#include <cmath>
#include <glad/glad.h>  // GLAD must be included before any OpenGL headers
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// WGL extension constants (not in standard headers)
#ifndef WGL_CONTEXT_MAJOR_VERSION_ARB
#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#endif

typedef HGLRC (WINAPI * PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC hDC, HGLRC hShareContext, const int *attribList);

namespace {
    const wchar_t* kOverlayClassName = L"SIMILI_OpenGL_Overlay";
}

OverlayViewport::OverlayViewport()
    : hwnd_(nullptr)
    , parent_(nullptr)
    , hdc_(nullptr)
    , gl_context_(nullptr)
    , width_(800)
    , height_(600)
    , vao_(0)
    , vbo_(0)
    , ebo_(0)
    , shader_program_(0)
    , bg_vao_(0)
    , bg_vbo_(0)
    , bg_shader_(0)
    , mouse_dragging_(false)
    , last_mouse_x_(0)
    , last_mouse_y_(0)
    , camera_rotation_x_(20.0f)   // Initial rotation for better view
    , camera_rotation_y_(30.0f)
    , camera_distance_(3.5f)      // Closer to fill more screen space
{
}

OverlayViewport::~OverlayViewport() {
    destroy();
}

bool OverlayViewport::create(HWND parent, int x, int y, int width, int height) {
    parent_ = parent;
    width_ = width;
    height_ = height;
    
    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = kOverlayClassName;
    
    static bool class_registered = false;
    if (!class_registered) {
        if (!RegisterClassExW(&wc)) {
            DWORD err = GetLastError();
            std::cerr << "[OverlayViewport] Failed to register window class. Error: " << err << std::endl;
            return false;
        }
        class_registered = true;
        std::cout << "[OverlayViewport] Window class registered" << std::endl;
    }
    
    // Create as child window
    std::cout << "[OverlayViewport] Creating child window with parent: " << parent 
              << " at (" << x << "," << y << ") size " << width << "x" << height << std::endl;
    
    hwnd_ = CreateWindowExW(
        0, 
        kOverlayClassName,
        L"OpenGL Overlay",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        x, y, width, height,
        parent,
        nullptr,
        GetModuleHandle(nullptr),
        this
    );
    
    if (!hwnd_) {
        DWORD error = GetLastError();
        std::cerr << "[OverlayViewport] Failed to create overlay window. Error: " << error << std::endl;
        return false;
    }
    
    std::cout << "[OverlayViewport] Window created successfully: " << hwnd_ << std::endl;

    SetWindowPos(hwnd_, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    
    RECT window_rect;
    GetWindowRect(hwnd_, &window_rect);
    POINT top_left = {window_rect.left, window_rect.top};
    ScreenToClient(parent, &top_left);
    std::cout << "[OverlayViewport] Actual client position: (" << top_left.x << ", " << top_left.y << ")" << std::endl;
        
    initializeOpenGL();
    
    std::cout << "[OverlayViewport] Created at (" << x << "," << y << ") size " << width << "x" << height << std::endl;
    
    return true;
}

void OverlayViewport::destroy() {
    if (gl_context_) {
        wglMakeCurrent(hdc_, gl_context_);
        
        // Clean up modern OpenGL resources
        if (vao_) glDeleteVertexArrays(1, &vao_);
        if (vbo_) glDeleteBuffers(1, &vbo_);
        if (ebo_) glDeleteBuffers(1, &ebo_);
        if (shader_program_) glDeleteProgram(shader_program_);
        
        // Clean up background resources
        if (bg_vao_) glDeleteVertexArrays(1, &bg_vao_);
        if (bg_vbo_) glDeleteBuffers(1, &bg_vbo_);
        if (bg_shader_) glDeleteProgram(bg_shader_);
        
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(gl_context_);
        gl_context_ = nullptr;
    }
    
    if (hdc_) {
        ReleaseDC(hwnd_, hdc_);
        hdc_ = nullptr;
    }
    
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

void OverlayViewport::initializeOpenGL() 
{
    hdc_ = GetDC(hwnd_);
    
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.cAlphaBits = 8;
    
    int pixel_format = ChoosePixelFormat(hdc_, &pfd);
    SetPixelFormat(hdc_, pixel_format, &pfd);
    
    HGLRC temp_context = wglCreateContext(hdc_);
    wglMakeCurrent(hdc_, temp_context);
    
    if (!gladLoadGL()) 
    {
        std::cerr << "[OverlayViewport] Failed to load GLAD" << std::endl;
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(temp_context);
        return;
    }
    
    // Create OpenGL 4.6 core profile context
    int attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
        WGL_CONTEXT_MINOR_VERSION_ARB, 6,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0
    };
    
    PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = 
        (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
    
    if (wglCreateContextAttribsARB) 
    {
        gl_context_ = wglCreateContextAttribsARB(hdc_, 0, attribs);
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(temp_context);
        wglMakeCurrent(hdc_, gl_context_);
        
        std::cout << "[OverlayViewport] OpenGL " << glGetString(GL_VERSION) 
                  << " - " << glGetString(GL_RENDERER)
                  << " (Core Profile 4.6)" << std::endl;
    }
    else 
    {
        gl_context_ = temp_context;
        std::cout << "[OverlayViewport] OpenGL " << glGetString(GL_VERSION) 
                  << " - " << glGetString(GL_RENDERER)
                  << " (Legacy - fallback)" << std::endl;
    }
    
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    initializeModernRendering();
}

void OverlayViewport::setPosition(int x, int y, int width, int height) {
    if (hwnd_) {
        SetWindowPos(hwnd_, HWND_TOP, x, y, width, height, 0);
        width_ = width;
        height_ = height;
        
        if (gl_context_ && hdc_) {
            wglMakeCurrent(hdc_, gl_context_);
            glViewport(0, 0, width, height);
            wglMakeCurrent(nullptr, nullptr);
        }
    }
}

void OverlayViewport::show(bool visible) {
    if (hwnd_) {
        ShowWindow(hwnd_, visible ? SW_SHOW : SW_HIDE);
    }
}

void OverlayViewport::render() {
    if (!hwnd_ || !gl_context_) {
        return;
    }
    
    wglMakeCurrent(hdc_, gl_context_);
    
    renderScene();
    
    SwapBuffers(hdc_);
}

void OverlayViewport::renderScene() {
    // Clear
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glViewport(0, 0, width_, height_);
    
    // Draw background gradient first (disable depth test)
    glDisable(GL_DEPTH_TEST);
    glUseProgram(bg_shader_);
    glBindVertexArray(bg_vao_);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
    
    // Draw 3D cube
    glUseProgram(shader_program_);
    
    // Setup matrices using GLM
    float aspect = static_cast<float>(width_) / static_cast<float>(height_);
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    
    glm::mat4 view = glm::mat4(1.0f);
    view = glm::translate(view, glm::vec3(0.0f, 0.0f, -camera_distance_));
    view = glm::rotate(view, glm::radians(camera_rotation_x_), glm::vec3(1.0f, 0.0f, 0.0f));
    view = glm::rotate(view, glm::radians(camera_rotation_y_), glm::vec3(0.0f, 1.0f, 0.0f));
    
    glm::mat4 model = glm::mat4(1.0f);
    
    // Send matrices to shader
    unsigned int modelLoc = glGetUniformLocation(shader_program_, "model");
    unsigned int viewLoc = glGetUniformLocation(shader_program_, "view");
    unsigned int projectionLoc = glGetUniformLocation(shader_program_, "projection");
    
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
    
    // Draw cube using VAO
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void OverlayViewport::handleMouseInput(UINT msg, WPARAM wParam, LPARAM lParam) {
    int x = LOWORD(lParam);
    int y = HIWORD(lParam);
    
    switch (msg) {
        case WM_LBUTTONDOWN:
            mouse_dragging_ = true;
            last_mouse_x_ = x;
            last_mouse_y_ = y;
            SetCapture(hwnd_);
            break;
            
        case WM_LBUTTONUP:
            mouse_dragging_ = false;
            ReleaseCapture();
            break;
            
        case WM_MOUSEMOVE:
            if (mouse_dragging_) {
                int dx = x - last_mouse_x_;
                int dy = y - last_mouse_y_;
                
                camera_rotation_y_ += dx * 0.5f;
                camera_rotation_x_ += dy * 0.5f;
                
                // Clamp vertical rotation
                if (camera_rotation_x_ > 89.0f) camera_rotation_x_ = 89.0f;
                if (camera_rotation_x_ < -89.0f) camera_rotation_x_ = -89.0f;
                
                last_mouse_x_ = x;
                last_mouse_y_ = y;
                
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            break;
            
        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            camera_distance_ -= delta * 0.01f;
            
            // Clamp distance (allow closer for better view)
            if (camera_distance_ < 1.5f) camera_distance_ = 1.5f;
            if (camera_distance_ > 20.0f) camera_distance_ = 20.0f;
            
            InvalidateRect(hwnd_, nullptr, FALSE);
            break;
        }
    }
}

LRESULT CALLBACK OverlayViewport::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    OverlayViewport* overlay = nullptr;
    
    if (msg == WM_CREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        overlay = static_cast<OverlayViewport*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(overlay));
    } else {
        overlay = reinterpret_cast<OverlayViewport*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    
    if (overlay) {
        switch (msg) {
            case WM_PAINT: {
                PAINTSTRUCT ps;
                BeginPaint(hwnd, &ps);
                overlay->render();
                EndPaint(hwnd, &ps);
                return 0;
            }
            
            case WM_ERASEBKGND:
                return 1;
            
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_MOUSEMOVE:
            case WM_MOUSEWHEEL:
                overlay->handleMouseInput(msg, wParam, lParam);
                return 0;
        }
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

unsigned int OverlayViewport::compileShader(unsigned int type, const char* source) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::cerr << "[OverlayViewport] Shader compilation failed: " << log << std::endl;
    }
    
    return shader;
}

unsigned int OverlayViewport::createShaderProgram(const char* vertex_src, const char* fragment_src) {
    unsigned int vertex_shader = compileShader(GL_VERTEX_SHADER, vertex_src);
    unsigned int fragment_shader = compileShader(GL_FRAGMENT_SHADER, fragment_src);
    
    unsigned int program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    
    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(program, 512, nullptr, log);
        std::cerr << "[OverlayViewport] Shader linking failed: " << log << std::endl;
    }
    
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    
    return program;
}

void OverlayViewport::initializeModernRendering() 
{

    const char* vertex_shader_src = R"(
        #version 460 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec3 aColor;
        
        out vec3 vertexColor;
        
        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;
        
        void main() {
            gl_Position = projection * view * model * vec4(aPos, 1.0);
            vertexColor = aColor;
        }
    )";
    
    const char* fragment_shader_src = R"(
        #version 460 core
        in vec3 vertexColor;
        out vec4 FragColor;
        
        void main() {
            FragColor = vec4(vertexColor, 1.0);
        }
    )";
    
    shader_program_ = createShaderProgram(vertex_shader_src, fragment_shader_src);
    
    // Cube vertices with colors (position + color)
    float vertices[] = 
    {

        -1.0f, -1.0f,  1.0f,  1.0f, 0.0f, 0.0f,
         1.0f, -1.0f,  1.0f,  1.0f, 0.0f, 0.0f,
         1.0f,  1.0f,  1.0f,  1.0f, 0.0f, 0.0f,
        -1.0f,  1.0f,  1.0f,  1.0f, 0.0f, 0.0f,
        
        // Back face (green)
        -1.0f, -1.0f, -1.0f,  0.0f, 1.0f, 0.0f,
         1.0f, -1.0f, -1.0f,  0.0f, 1.0f, 0.0f,
         1.0f,  1.0f, -1.0f,  0.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, -1.0f,  0.0f, 1.0f, 0.0f,
        
        // Top face (blue)
        -1.0f,  1.0f, -1.0f,  0.0f, 0.0f, 1.0f,
         1.0f,  1.0f, -1.0f,  0.0f, 0.0f, 1.0f,
         1.0f,  1.0f,  1.0f,  0.0f, 0.0f, 1.0f,
        -1.0f,  1.0f,  1.0f,  0.0f, 0.0f, 1.0f,
        
        // Bottom face (yellow)
        -1.0f, -1.0f, -1.0f,  1.0f, 1.0f, 0.0f,
         1.0f, -1.0f, -1.0f,  1.0f, 1.0f, 0.0f,
         1.0f, -1.0f,  1.0f,  1.0f, 1.0f, 0.0f,
        -1.0f, -1.0f,  1.0f,  1.0f, 1.0f, 0.0f,
        
        // Right face (magenta)
         1.0f, -1.0f, -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f,  1.0f,  1.0f, 0.0f, 1.0f,
         1.0f, -1.0f,  1.0f,  1.0f, 0.0f, 1.0f,
        
        // Left face (cyan)
        -1.0f, -1.0f, -1.0f,  0.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, -1.0f,  0.0f, 1.0f, 1.0f,
        -1.0f,  1.0f,  1.0f,  0.0f, 1.0f, 1.0f,
        -1.0f, -1.0f,  1.0f,  0.0f, 1.0f, 1.0f
    };
    
    unsigned int indices[] = {
        0,  1,  2,   2,  3,  0,   // Front
        4,  5,  6,   6,  7,  4,   // Back
        8,  9,  10,  10, 11, 8,   // Top
        12, 13, 14,  14, 15, 12,  // Bottom
        16, 17, 18,  18, 19, 16,  // Right
        20, 21, 22,  22, 23, 20  
    };
    
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);
    
    glBindVertexArray(vao_);
    
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    
    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Color attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindVertexArray(0);
    
    // Create background gradient quad
    const char* bg_vertex_src = R"(
        #version 460 core
        layout (location = 0) in vec2 aPos;
        out vec2 vPos;
        
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
            vPos = aPos;
        }
    )";
    
    const char* bg_fragment_src = R"(
        #version 460 core
        in vec2 vPos;
        out vec4 FragColor;
        
        void main() {
            // Gradient from gray (top) to white (bottom)
            float t = (vPos.y + 1.0) * 0.5;  // Map from [-1,1] to [0,1]
            vec3 topColor = vec3(0.7, 0.7, 0.7);     // Gray
            vec3 bottomColor = vec3(0.95, 0.95, 0.95);  // Almost white
            vec3 color = mix(bottomColor, topColor, t);
            FragColor = vec4(color, 1.0);
        }
    )";
    
    bg_shader_ = createShaderProgram(bg_vertex_src, bg_fragment_src);
    
    // Fullscreen quad vertices (position only, no color needed)
    float bg_vertices[] = {
        -1.0f, -1.0f,  // Bottom-left
         1.0f, -1.0f,  // Bottom-right
         1.0f,  1.0f,  // Top-right
        -1.0f,  1.0f   // Top-left
    };
    
    glGenVertexArrays(1, &bg_vao_);
    glGenBuffers(1, &bg_vbo_);
    
    glBindVertexArray(bg_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, bg_vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(bg_vertices), bg_vertices, GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glBindVertexArray(0);
    
    std::cout << "[OverlayViewport] Modern rendering initialized (VAO/VBO/Shaders)" << std::endl;
}

