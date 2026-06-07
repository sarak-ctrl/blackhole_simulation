// ============================================================
//  BLACK HOLE SIMULATION — Kerr Metric Ray Marcher
//  Language:  C++ with OpenGL 4.1
//  Physics:   Kerr geodesics, Doppler beaming, gravitational redshift
//  Visuals:   Volumetric accretion disk, HDR bloom, spacetime grid
// ============================================================

// Suppress macOS OpenGL deprecation warnings (Apple deprecated OpenGL in 2018
// but it still works perfectly — this just hides the warnings)
#define GL_SILENCE_DEPRECATION

// GLEW must ALWAYS be included before GLFW
// GLEW loads all modern OpenGL functions at runtime
#include <GL/glew.h>

// GLFW creates the window and handles input (mouse, keyboard)
#include <GLFW/glfw3.h>

// GLM is the math library — vectors, matrices, transformations
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Standard C++ libraries
#include <iostream>   // for printing errors to console
#include <fstream>    // for reading shader files from disk
#include <sstream>    // for converting file stream to string
#include <string>     // for std::string
#include <vector>     // for std::vector (dynamic arrays)
#include <cmath>      // for sin, cos, sqrt etc.

// ---------------------- Window size -------------------------
// These can change if the user resizes the window
static int   WIN_W = 1280, WIN_H = 720;
static float aspectRatio = float(WIN_W) / float(WIN_H);

// ----------------------- Camera state -----------------------
// camTheta = vertical angle (how high above the disk we are)
// camPhi   = horizontal angle (rotating around the black hole)
// camDist  = how far from the black hole
static float camTheta  = 1.18f;
static float camPhi    = 0.35f;
static float camDist   = 14.0f;
static bool  mouseDown = false;     // is left mouse button held?
static double lastX = 0, lastY = 0; // last mouse position

// ---------------- Physics parameters (changed by keyboard at runtime) -------------
// bhMass     = mass of the black hole (M in equations)
// bhSpin     = spin of the black hole (a, between 0 and 0.999)
//              0     = Schwarzschild (non-rotating)
//              0.999 = near-maximal Kerr (fast rotating)
// diskBright = how bright the accretion disk glows
// bloomStr   = strength of the HDR bloom glow effect
// time_val   = animation timer, increases every frame
static float bhMass     = 1.0f;
static float bhSpin     = 0.92f;
static float diskBright = 2.6f;
static float bloomStr   = 0.95f;
static float time_val   = 0.0f;

// -------------- Load a text file from disk into a string ---------------
// We use this to load the shader source code files (.vert/.frag)
static std::string loadFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << "Cannot open file: " << path << "\n";
        return "";
    }
    std::ostringstream ss;
    ss << f.rdbuf(); // read entire file into stream
    return ss.str(); // return as string
}

// ---------------- Compile a single shader (vertex or fragment) ----------------
// type = GL_VERTEX_SHADER or GL_FRAGMENT_SHADER
// src  = the GLSL source code as a string
// Returns the shader ID (a number OpenGL uses to refer to it)
static GLuint compileShader(GLenum type, const std::string& src) {
    if (src.empty()) {
        std::cerr << "Shader source is empty; check working directory and shader files.\n";
        return 0;
    }

    GLuint s = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(s, 1, &c, nullptr);
    glCompileShader(s);

    // Check if compilation succeeded
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, 2048, nullptr, log);
        std::cerr << "Shader compile error:\n" << log << "\n";
        glDeleteShader(s);
        return 0;
    }
    return s;
}

// ----------------- Link vertex + fragment shader into a program ------------------
// A "program" is the complete pipeline that runs on the GPU
// vert = compiled vertex shader ID
// frag = compiled fragment shader ID
static GLuint linkProgram(GLuint vert, GLuint frag) {
    if (vert == 0 || frag == 0) {
        std::cerr << "Program link skipped due to missing shader stage.\n";
        return 0;
    }

    GLuint p = glCreateProgram();
    glAttachShader(p, vert);
    glAttachShader(p, frag);
    glLinkProgram(p);

    // Check if linking succeeded
    GLint ok;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(p, 2048, nullptr, log);
        std::cerr << "Program link error:\n" << log << "\n";
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

// --------------------------- Fullscreen quad --------------------------------
// A quad is just 2 triangles that fill the entire screen.
// We use it to run the ray-marching shader on every pixel.
// Think of it as a canvas that the GPU paints the black hole on.
static GLuint quadVAO, quadVBO;
static void initQuad() {
    // 6 vertices (2 triangles) covering NDC space (-1 to 1)
    float verts[] = { -1,-1,  1,-1,  -1,1,
                       1,-1,  1, 1,  -1,1 };
    glGenVertexArrays(1, &quadVAO);
    glBindVertexArray(quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
}
static void drawQuad() {
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

// ------------------------- Spacetime grid geometry -------------------------
// Builds an N×N grid of lines on the XZ plane.
// The grid vertices get warped downward in the vertex shader
// to show the curvature of spacetime around the black hole.
// N    = number of grid lines in each direction
// size = total width/length of the grid in world units
static GLuint gridVAO, gridVBO, gridEBO;
static int    gridIndexCount = 0;
static void buildGrid(int N, float size) {
    std::vector<glm::vec3> verts;
    std::vector<unsigned>  idx;
    float step = size / (N - 1);

    // Create N×N vertex positions on flat XZ plane
    for (int j = 0; j < N; j++)
    for (int i = 0; i < N; i++) {
        float x = -size/2 + i * step;
        float z = -size/2 + j * step;
        verts.push_back({x, 0.0f, z});
    }

    // Connect vertices with lines along X direction
    for (int j = 0; j < N; j++)
    for (int i = 0; i < N-1; i++) {
        idx.push_back(j*N + i);
        idx.push_back(j*N + i + 1);
    }

    // Connect vertices with lines along Z direction
    for (int i = 0; i < N; i++)
    for (int j = 0; j < N-1; j++) {
        idx.push_back( j*N    + i);
        idx.push_back((j+1)*N + i);
    }

    gridIndexCount = static_cast<int>(idx.size());

    // Upload geometry to the GPU
    glGenVertexArrays(1, &gridVAO);
    glBindVertexArray(gridVAO);

    glGenBuffers(1, &gridVBO);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER,
        verts.size() * sizeof(glm::vec3),
        verts.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &gridEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gridEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        idx.size() * sizeof(unsigned),
        idx.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
}

// --------------------------- Framebuffer Object (FBO) -----------------------------------
// An FBO lets us render to a texture instead of the screen.
// We use this for multi-pass rendering:
//   Pass 1: render black hole → texture
//   Pass 2: extract bright areas → texture
//   Pass 3: blur bright areas → texture (bloom effect)
//   Pass 4: combine everything → screen
struct FBO { GLuint fbo, tex; };
static FBO makeFBO(int w, int h) {
    FBO f;
    // Create framebuffer
    glGenFramebuffers(1, &f.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, f.fbo);

    // Create HDR texture (GL_RGBA16F allows values > 1.0 for HDR)
    glGenTextures(1, &f.tex);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0,
                 GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Attach texture to framebuffer
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, f.tex, 0);
    auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Framebuffer incomplete (status " << status << ")\n";
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return f;
}

// --------------------------- Input callbacks --------------------------
// These functions are called automatically by GLFW when the
// user interacts with the window.

// Mouse scroll wheel → zoom in/out
static void onScroll(GLFWwindow*, double, double dy) {
    camDist = glm::clamp(camDist - float(dy) * 0.5f, 3.0f, 40.0f);
}

// Mouse button → start/stop orbiting
static void onMouse(GLFWwindow* w, int btn, int action, int) {
    if (btn == GLFW_MOUSE_BUTTON_LEFT) {
        mouseDown = (action == GLFW_PRESS);
        if (mouseDown) glfwGetCursorPos(w, &lastX, &lastY);
    }
}

// Mouse move → orbit camera around black hole
static void onCursor(GLFWwindow*, double x, double y) {
    if (!mouseDown) return;
    float dx = float(x - lastX) * 0.005f;
    float dy = float(y - lastY) * 0.005f;
    lastX = x; lastY = y;
    camPhi   += dx;
    // Clamp vertical angle so camera doesn't flip upside down
    camTheta  = glm::clamp(camTheta + dy, 0.1f, float(M_PI) - 0.1f);
}

// Keyboard → tweak physics parameters in real time
// Q/A = spin up/down      (how fast the black hole rotates)
// W/S = mass up/down      (how heavy the black hole is)
// E/D = disk brightness   (how bright the accretion disk glows)
// R/F = bloom strength    (how strong the glow effect is)
static void onKey(GLFWwindow*, int key, int, int action, int) {
    if (action == GLFW_RELEASE) return;
    float spd = 0.05f;
    if (key == GLFW_KEY_Q) bhSpin     = glm::clamp(bhSpin     + spd,       0.0f,  0.999f);
    if (key == GLFW_KEY_A) bhSpin     = glm::clamp(bhSpin     - spd,       0.0f,  0.999f);
    if (key == GLFW_KEY_W) bhMass     = glm::clamp(bhMass     + spd*0.5f,  0.5f,  3.0f);
    if (key == GLFW_KEY_S) bhMass     = glm::clamp(bhMass     - spd*0.5f,  0.5f,  3.0f);
    if (key == GLFW_KEY_E) diskBright = glm::clamp(diskBright + spd,       0.2f,  5.0f);
    if (key == GLFW_KEY_D) diskBright = glm::clamp(diskBright - spd,       0.2f,  5.0f);
    if (key == GLFW_KEY_R) bloomStr   = glm::clamp(bloomStr   + spd,       0.0f,  4.0f);
    if (key == GLFW_KEY_F) bloomStr   = glm::clamp(bloomStr   - spd,       0.0f,  4.0f);
}

// Window resize → update viewport and aspect ratio
static void onResize(GLFWwindow*, int w, int h) {
    if (h <= 0) h = 1;
    WIN_W = w; WIN_H = h;
    aspectRatio = float(w) / float(h);
    glViewport(0, 0, w, h);
}

// ═════════════════════════════════════════════════════════════
//  MAIN — entry point of the program
// ═════════════════════════════════════════════════════════════
int main() {

    // ---------------------- Initialise GLFW -------------------------
    // GLFW manages the OS window and OpenGL context
    if (!glfwInit()) {
        std::cerr << "GLFW init failed\n";
        return 1;
    }

    // Request OpenGL 4.1 Core Profile (highest macOS supports)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // required on macOS
    glfwWindowHint(GLFW_SAMPLES, 4); // 4x anti-aliasing

    // Create the window
    GLFWwindow* win = glfwCreateWindow(
        WIN_W, WIN_H, "Black Hole Simulation V.1.1", nullptr, nullptr);
    if (!win) {
        std::cerr << "Window creation failed\n";
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(win); // make this window's context active
    glfwSwapInterval(1);         // enable VSync (cap to monitor refresh rate)

    // Register input callbacks
    glfwSetScrollCallback(win,          onScroll);
    glfwSetMouseButtonCallback(win,     onMouse);
    glfwSetCursorPosCallback(win,       onCursor);
    glfwSetKeyCallback(win,             onKey);
    glfwSetFramebufferSizeCallback(win, onResize);

    // ------------------------ Initialise GLEW --------------------------
    // GLEW loads pointers to all modern OpenGL functions
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "GLEW init failed\n";
        glfwDestroyWindow(win);
        glfwTerminate();
        return 1;
    }

    // Print OpenGL version so we know what we got
    std::cout << "OpenGL: " << glGetString(GL_VERSION) << "\n";
    std::cout << "Controls:\n"
              << "  Mouse drag — orbit camera\n"
              << "  Scroll     — zoom\n"
              << "  Q/A        — spin up/down\n"
              << "  W/S        — mass up/down\n"
              << "  E/D        — disk brightness\n"
              << "  R/F        — bloom strength\n";

    // ---------------------- Load and compile all shaders ----------------------
    // bh.vert / bh.frag     = black hole ray marcher
    // grid.vert / grid.frag = spacetime curvature grid
    // blur.frag             = bright extract + gaussian blur
    // composite.frag        = HDR tone map + bloom combine
    auto vs_bh  = compileShader(GL_VERTEX_SHADER,   loadFile("bh.vert"));
    auto fs_bh  = compileShader(GL_FRAGMENT_SHADER, loadFile("bh.frag"));
    auto vs_grd = compileShader(GL_VERTEX_SHADER,   loadFile("grid.vert"));
    auto fs_grd = compileShader(GL_FRAGMENT_SHADER, loadFile("grid.frag"));
    auto vs_blr = compileShader(GL_VERTEX_SHADER,   loadFile("bh.vert"));
    auto fs_blr = compileShader(GL_FRAGMENT_SHADER, loadFile("blur.frag"));
    auto vs_cmp = compileShader(GL_VERTEX_SHADER,   loadFile("bh.vert"));
    auto fs_cmp = compileShader(GL_FRAGMENT_SHADER, loadFile("composite.frag"));

    // Link shaders into GPU programs
    GLuint progBH        = linkProgram(vs_bh,  fs_bh);
    GLuint progGrid      = linkProgram(vs_grd, fs_grd);
    GLuint progBlur      = linkProgram(vs_blr, fs_blr);
    GLuint progComposite = linkProgram(vs_cmp, fs_cmp);
    if (progBH == 0 || progGrid == 0 || progBlur == 0 || progComposite == 0) {
        std::cerr << "One or more shader programs failed to build. Exiting.\n";
        glfwDestroyWindow(win);
        glfwTerminate();
        return 1;
    }

    // ----------------------- Create geometry -----------------------
    initQuad();              // fullscreen quad for ray marching
    buildGrid(80, 30.0f);    // 80×80 spacetime grid, 30 units wide

    // ------------------ Create framebuffers for multi-pass rendering -----------------
    FBO fboScene  = makeFBO(WIN_W,   WIN_H);    // full scene HDR
    FBO fboBright = makeFBO(WIN_W,   WIN_H);    // bright regions only
    FBO fboBlurA  = makeFBO(WIN_W/2, WIN_H/2);  // blur ping
    FBO fboBlurB  = makeFBO(WIN_W/2, WIN_H/2);  // blur pong

    // ------------------- OpenGL state -----------------------
    glEnable(GL_DEPTH_TEST); // closer objects hide farther ones
    glEnable(GL_BLEND);      // allow transparency
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    double prevTime = glfwGetTime();

    // ════════════════════════════════════════════════════════
    //  MAIN RENDER LOOP — runs every frame until window closes
    // ════════════════════════════════════════════════════════
    while (!glfwWindowShouldClose(win)) {

        // Calculate delta time (seconds since last frame)
        double now = glfwGetTime();
        float  dt  = float(now - prevTime);
        prevTime   = now;
        time_val  += dt; // advance animation timer

        // ---------------------- Camera position in 3D world space ---------------------
        // Convert spherical coordinates to Cartesian (x,y,z)
        // This lets the camera orbit smoothly around the origin
        glm::vec3 camPos = glm::vec3(
            camDist * sin(camTheta) * cos(camPhi),  // x
            camDist * cos(camTheta),                 // y
            camDist * sin(camTheta) * sin(camPhi)   // z
        );

        // Build view and projection matrices for the grid
        glm::mat4 view = glm::lookAt(camPos, glm::vec3(0), glm::vec3(0,1,0));
        glm::mat4 proj = glm::perspective(
            glm::radians(50.0f), aspectRatio, 0.1f, 200.0f);
        glm::mat4 vp = proj * view; // combined ViewProjection matrix

        // ════════════════════════════════════════════════════
        //  PASS 1: Ray-march black hole + accretion disk
        //  Output: HDR colour for every pixel → fboScene
        // ════════════════════════════════════════════════════
        glBindFramebuffer(GL_FRAMEBUFFER, fboScene.fbo);
        glViewport(0, 0, WIN_W, WIN_H);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glDisable(GL_DEPTH_TEST);
        glUseProgram(progBH);
        // Send all physics/camera parameters to the GPU shader
        glUniform1f(glGetUniformLocation(progBH, "uTime"),       time_val);
        glUniform1f(glGetUniformLocation(progBH, "uMass"),       bhMass);
        glUniform1f(glGetUniformLocation(progBH, "uSpin"),       bhSpin);
        glUniform1f(glGetUniformLocation(progBH, "uDiskBright"), diskBright);
        glUniform1f(glGetUniformLocation(progBH, "uAspect"),     aspectRatio);
        glUniform3fv(glGetUniformLocation(progBH, "uCamPos"), 1, glm::value_ptr(camPos));
        glUniform1f(glGetUniformLocation(progBH, "uCamTheta"),   camTheta);
        glUniform1f(glGetUniformLocation(progBH, "uCamPhi"),     camPhi);
        glUniform1f(glGetUniformLocation(progBH, "uCamDist"),    camDist);
        drawQuad(); // run the ray marcher on every pixel

        // ════════════════════════════════════════════════════
        //  PASS 2: Draw spacetime curvature grid on top
        //  The grid vertex shader warps the grid downward
        //  to visualise Flamm's paraboloid (the gravity well)
        // ════════════════════════════════════════════════════
        glEnable(GL_DEPTH_TEST);
        glUseProgram(progGrid);
        glUniformMatrix4fv(glGetUniformLocation(progGrid, "uVP"),
                           1, GL_FALSE, glm::value_ptr(vp));
        glUniform1f(glGetUniformLocation(progGrid, "uTime"), time_val);
        glUniform1f(glGetUniformLocation(progGrid, "uMass"), bhMass);
        glUniform1f(glGetUniformLocation(progGrid, "uSpin"), bhSpin);
        glBindVertexArray(gridVAO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gridEBO);
        glDrawElements(GL_LINES, gridIndexCount, GL_UNSIGNED_INT, nullptr);

        // ════════════════════════════════════════════════════
        //  PASS 3: Extract bright regions for bloom
        //  Only pixels brighter than uThresh contribute to bloom
        //  This prevents dim areas from glowing
        // ════════════════════════════════════════════════════
        glBindFramebuffer(GL_FRAMEBUFFER, fboBright.fbo);
        glViewport(0, 0, WIN_W, WIN_H);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
        glUseProgram(progBlur);
        glUniform1i(glGetUniformLocation(progBlur, "uTex"),    0);
        glUniform1i(glGetUniformLocation(progBlur, "uMode"),   0);      // bright extract mode
        glUniform1f(glGetUniformLocation(progBlur, "uThresh"), 1.05f);   // brightness threshold
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fboScene.tex);
        drawQuad();

        // ════════════════════════════════════════════════════
        //  PASS 4: Gaussian blur (ping-pong, 6 iterations)
        //  We alternate between two FBOs blurring horizontally
        //  then vertically — this gives a smooth circular glow
        // ════════════════════════════════════════════════════
        bool   horizontal = true;
        GLuint blurSrc    = fboBright.tex;
        for (int i = 0; i < 6; i++) {
            FBO& dst = horizontal ? fboBlurA : fboBlurB;
            glBindFramebuffer(GL_FRAMEBUFFER, dst.fbo);
            glViewport(0, 0, WIN_W/2, WIN_H/2);
            glClear(GL_COLOR_BUFFER_BIT);
            glUseProgram(progBlur);
            glUniform1i(glGetUniformLocation(progBlur, "uMode"),       1); // blur mode
            glUniform1i(glGetUniformLocation(progBlur, "uHorizontal"), horizontal ? 1 : 0);
            glUniform1i(glGetUniformLocation(progBlur, "uTex"),        0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, blurSrc);
            drawQuad();
            blurSrc    = dst.tex;
            horizontal = !horizontal;
        }

        // ════════════════════════════════════════════════════
        //  PASS 5: Composite — combine scene + bloom → screen
        //  Also applies ACES filmic tone mapping and gamma
        //  correction so HDR values look right on the monitor
        // ════════════════════════════════════════════════════
        glBindFramebuffer(GL_FRAMEBUFFER, 0); // render to screen
        glViewport(0, 0, WIN_W, WIN_H);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
        glUseProgram(progComposite);
        glUniform1i(glGetUniformLocation(progComposite, "uScene"),    0);
        glUniform1i(glGetUniformLocation(progComposite, "uBloom"),    1);
        glUniform1f(glGetUniformLocation(progComposite, "uBloomStr"), bloomStr);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fboScene.tex); // original scene
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, blurSrc);      // blurred bloom
        drawQuad();

        // Print current parameters to console every 2 seconds
        static int frame = 0;
        if (++frame % 120 == 0)
            printf("\rMass=%.2f  Spin=%.3f  Disk=%.2f  Bloom=%.2f   ",
                   bhMass, bhSpin, diskBright, bloomStr);

        glfwSwapBuffers(win); // show rendered frame
        glfwPollEvents();     // process mouse/keyboard events
    }

    // ------------------------- Cleanup ------------------------------
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}