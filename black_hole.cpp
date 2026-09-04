#define GL_SILENCE_DEPRECATION

#include <GL/glew.h>

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>   
#include <fstream>    
#include <sstream>    
#include <string>     
#include <vector>     
#include <cmath>      

static int   WIN_W = 1280, WIN_H = 720;
static float aspectRatio = float(WIN_W) / float(WIN_H);

static float camTheta  = 1.18f;
static float camPhi    = 0.35f;
static float camDist   = 14.0f;
static bool  mouseDown = false;     
static double lastX = 0, lastY = 0; 

static float bhMass     = 1.0f;
static float bhSpin     = 0.92f;
static float diskBright = 2.6f;
static float bloomStr   = 0.95f;
static float time_val   = 0.0f;

static std::string loadFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << "Cannot open file: " << path << "\n";
        return "";
    }
    std::ostringstream ss;
    ss << f.rdbuf(); 
    return ss.str(); 
}

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

static GLuint gridVAO, gridVBO, gridEBO;
static int    gridIndexCount = 0;
static void buildGrid(int N, float size) {
    std::vector<glm::vec3> verts;
    std::vector<unsigned>  idx;
    float step = size / (N - 1);

    for (int j = 0; j < N; j++)
    for (int i = 0; i < N; i++) {
        float x = -size/2 + i * step;
        float z = -size/2 + j * step;
        verts.push_back({x, 0.0f, z});
    }

    for (int j = 0; j < N; j++)
    for (int i = 0; i < N-1; i++) {
        idx.push_back(j*N + i);
        idx.push_back(j*N + i + 1);
    }

    for (int i = 0; i < N; i++)
    for (int j = 0; j < N-1; j++) {
        idx.push_back( j*N    + i);
        idx.push_back((j+1)*N + i);
    }

    gridIndexCount = static_cast<int>(idx.size());

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

struct FBO { GLuint fbo, tex; };
static FBO makeFBO(int w, int h) {
    FBO f;
    glGenFramebuffers(1, &f.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, f.fbo);

    glGenTextures(1, &f.tex);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0,
                 GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, f.tex, 0);
    auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Framebuffer incomplete (status " << status << ")\n";
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return f;
}

static void onScroll(GLFWwindow*, double, double dy) {
    camDist = glm::clamp(camDist - float(dy) * 0.5f, 3.0f, 40.0f);
}

static void onMouse(GLFWwindow* w, int btn, int action, int) {
    if (btn == GLFW_MOUSE_BUTTON_LEFT) {
        mouseDown = (action == GLFW_PRESS);
        if (mouseDown) glfwGetCursorPos(w, &lastX, &lastY);
    }
}

static void onCursor(GLFWwindow*, double x, double y) {
    if (!mouseDown) return;
    float dx = float(x - lastX) * 0.005f;
    float dy = float(y - lastY) * 0.005f;
    lastX = x; lastY = y;
    camPhi   += dx;
    camTheta  = glm::clamp(camTheta + dy, 0.1f, float(M_PI) - 0.1f);
}

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

static void onResize(GLFWwindow*, int w, int h) {
    if (h <= 0) h = 1;
    WIN_W = w; WIN_H = h;
    aspectRatio = float(w) / float(h);
    glViewport(0, 0, w, h);
}

int main() {

    if (!glfwInit()) {
        std::cerr << "GLFW init failed\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); 
    glfwWindowHint(GLFW_SAMPLES, 4); 

    GLFWwindow* win = glfwCreateWindow(
        WIN_W, WIN_H, "Black Hole Simulation V.1.1", nullptr, nullptr);
    if (!win) {
        std::cerr << "Window creation failed\n";
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(win); 
    glfwSwapInterval(1);         

    glfwSetScrollCallback(win,          onScroll);
    glfwSetMouseButtonCallback(win,     onMouse);
    glfwSetCursorPosCallback(win,       onCursor);
    glfwSetKeyCallback(win,             onKey);
    glfwSetFramebufferSizeCallback(win, onResize);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "GLEW init failed\n";
        glfwDestroyWindow(win);
        glfwTerminate();
        return 1;
    }

    std::cout << "OpenGL: " << glGetString(GL_VERSION) << "\n";
    std::cout << "Controls:\n"
              << "  Mouse drag — orbit camera\n"
              << "  Scroll     — zoom\n"
              << "  Q/A        — spin up/down\n"
              << "  W/S        — mass up/down\n"
              << "  E/D        — disk brightness\n"
              << "  R/F        — bloom strength\n";

    auto vs_bh  = compileShader(GL_VERTEX_SHADER,   loadFile("bh.vert"));
    auto fs_bh  = compileShader(GL_FRAGMENT_SHADER, loadFile("bh.frag"));
    auto vs_grd = compileShader(GL_VERTEX_SHADER,   loadFile("grid.vert"));
    auto fs_grd = compileShader(GL_FRAGMENT_SHADER, loadFile("grid.frag"));
    auto vs_blr = compileShader(GL_VERTEX_SHADER,   loadFile("bh.vert"));
    auto fs_blr = compileShader(GL_FRAGMENT_SHADER, loadFile("blur.frag"));
    auto vs_cmp = compileShader(GL_VERTEX_SHADER,   loadFile("bh.vert"));
    auto fs_cmp = compileShader(GL_FRAGMENT_SHADER, loadFile("composite.frag"));

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

    initQuad();              
    buildGrid(80, 30.0f);    

    FBO fboScene  = makeFBO(WIN_W,   WIN_H);    
    FBO fboBright = makeFBO(WIN_W,   WIN_H);    
    FBO fboBlurA  = makeFBO(WIN_W/2, WIN_H/2);  
    FBO fboBlurB  = makeFBO(WIN_W/2, WIN_H/2);  

    glEnable(GL_DEPTH_TEST); 
    glEnable(GL_BLEND);      
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    double prevTime = glfwGetTime();

    while (!glfwWindowShouldClose(win)) {

        double now = glfwGetTime();
        float  dt  = float(now - prevTime);
        prevTime   = now;
        time_val  += dt; // advance animation timer

        glm::vec3 camPos = glm::vec3(
            camDist * sin(camTheta) * cos(camPhi),  
            camDist * cos(camTheta),                 
            camDist * sin(camTheta) * sin(camPhi)   
        );

        glm::mat4 view = glm::lookAt(camPos, glm::vec3(0), glm::vec3(0,1,0));
        glm::mat4 proj = glm::perspective(
            glm::radians(50.0f), aspectRatio, 0.1f, 200.0f);
        glm::mat4 vp = proj * view; 

        glBindFramebuffer(GL_FRAMEBUFFER, fboScene.fbo);
        glViewport(0, 0, WIN_W, WIN_H);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glDisable(GL_DEPTH_TEST);
        glUseProgram(progBH);
        glUniform1f(glGetUniformLocation(progBH, "uTime"),       time_val);
        glUniform1f(glGetUniformLocation(progBH, "uMass"),       bhMass);
        glUniform1f(glGetUniformLocation(progBH, "uSpin"),       bhSpin);
        glUniform1f(glGetUniformLocation(progBH, "uDiskBright"), diskBright);
        glUniform1f(glGetUniformLocation(progBH, "uAspect"),     aspectRatio);
        glUniform3fv(glGetUniformLocation(progBH, "uCamPos"), 1, glm::value_ptr(camPos));
        glUniform1f(glGetUniformLocation(progBH, "uCamTheta"),   camTheta);
        glUniform1f(glGetUniformLocation(progBH, "uCamPhi"),     camPhi);
        glUniform1f(glGetUniformLocation(progBH, "uCamDist"),    camDist);
        drawQuad(); 

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

        glBindFramebuffer(GL_FRAMEBUFFER, fboBright.fbo);
        glViewport(0, 0, WIN_W, WIN_H);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
        glUseProgram(progBlur);
        glUniform1i(glGetUniformLocation(progBlur, "uTex"),    0);
        glUniform1i(glGetUniformLocation(progBlur, "uMode"),   0);      
        glUniform1f(glGetUniformLocation(progBlur, "uThresh"), 1.05f);  
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fboScene.tex);
        drawQuad();

        bool   horizontal = true;
        GLuint blurSrc    = fboBright.tex;
        for (int i = 0; i < 6; i++) {
            FBO& dst = horizontal ? fboBlurA : fboBlurB;
            glBindFramebuffer(GL_FRAMEBUFFER, dst.fbo);
            glViewport(0, 0, WIN_W/2, WIN_H/2);
            glClear(GL_COLOR_BUFFER_BIT);
            glUseProgram(progBlur);
            glUniform1i(glGetUniformLocation(progBlur, "uMode"),       1); 
            glUniform1i(glGetUniformLocation(progBlur, "uHorizontal"), horizontal ? 1 : 0);
            glUniform1i(glGetUniformLocation(progBlur, "uTex"),        0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, blurSrc);
            drawQuad();
            blurSrc    = dst.tex;
            horizontal = !horizontal;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, WIN_W, WIN_H);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
        glUseProgram(progComposite);
        glUniform1i(glGetUniformLocation(progComposite, "uScene"),    0);
        glUniform1i(glGetUniformLocation(progComposite, "uBloom"),    1);
        glUniform1f(glGetUniformLocation(progComposite, "uBloomStr"), bloomStr);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fboScene.tex); 
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, blurSrc);     
        drawQuad();

        static int frame = 0;
        if (++frame % 120 == 0)
            printf("\rMass=%.2f  Spin=%.3f  Disk=%.2f  Bloom=%.2f   ",
                   bhMass, bhSpin, diskBright, bloomStr);

        glfwSwapBuffers(win); 
        glfwPollEvents();    
    }

    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
