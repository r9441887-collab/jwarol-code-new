#include "../include/Renderer3D.hpp"
#include "../include/GLFunctions.hpp"
#include <GL/gl.h>
#include <cstdio>

static unsigned int compileShader3D(const char* src, unsigned int type) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    int ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(shader, 512, NULL, log);
        fprintf(stderr, "Shader compile error: %s\n", log);
    }
    return shader;
}

static unsigned int linkProgram3D(unsigned int vs, unsigned int fs) {
    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    int ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, 512, NULL, log);
        fprintf(stderr, "Program link error: %s\n", log);
    }
    glDetachShader(prog, vs);
    glDetachShader(prog, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

static const char* litVS = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

uniform mat4 uMVP;
uniform mat4 uModel;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal = mat3(uModel) * aNormal;
    vUV = aUV;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* litFS = R"(
#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;

uniform vec4 uColor;
uniform vec3 uLightDir;
uniform float uLightIntensity;
uniform float uAmbient;

out vec4 FragColor;

void main() {
    vec3 n = normalize(vNormal);
    vec3 l = normalize(uLightDir);
    float diff = max(dot(n, l), 0.0);
    float light = uAmbient + diff * uLightIntensity;
    FragColor = vec4(uColor.rgb * light, uColor.a);
}
)";

static const char* depthVS = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* depthFS = R"(
#version 330 core
out vec4 FragColor;
void main() {
    FragColor = vec4(0.0, 0.0, 0.0, 1.0);
}
)";

static Vec3 g_lightDir = {0.5f, -1.0f, 0.3f};
static float g_ambient = 0.15f;

Renderer3D::Renderer3D(float width, float height, RenderBackend backend)
    : backend(backend), shaderProgram(0), depthProgram(0), screenW(width), screenH(height),
      viewProj(Mat4::identity()) {
    if (backend == RenderBackend::OPENGL) {
        initShaders();
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
    }
}

Renderer3D::~Renderer3D() {
    if (shaderProgram) glDeleteProgram(shaderProgram);
    if (depthProgram) glDeleteProgram(depthProgram);
}

void Renderer3D::initShaders() {
    unsigned int vs = compileShader3D(litVS, GL_VERTEX_SHADER);
    unsigned int fs = compileShader3D(litFS, GL_FRAGMENT_SHADER);
    shaderProgram = linkProgram3D(vs, fs);

    vs = compileShader3D(depthVS, GL_VERTEX_SHADER);
    fs = compileShader3D(depthFS, GL_FRAGMENT_SHADER);
    depthProgram = linkProgram3D(vs, fs);
}

void Renderer3D::beginFrame(Camera3D& camera) {
    if (backend == RenderBackend::VULKAN) {
        fprintf(stderr, "Vulkan backend not yet implemented\n");
        return;
    }
    if (backend == RenderBackend::DX11) {
        fprintf(stderr, "DX11 backend not available on this platform\n");
        return;
    }
    viewProj = camera.getProjectionMatrix() * camera.getViewMatrix();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer3D::endFrame() {
}

void Renderer3D::drawMesh(Mesh3D& mesh, const Mat4& model,
                          float r, float g, float b, float a) {
    drawMeshLit(mesh, model, r, g, b, {g_lightDir.x, g_lightDir.y, g_lightDir.z}, 1.0f, g_ambient);
}

void Renderer3D::drawMeshLit(Mesh3D& mesh, const Mat4& model,
                              float r, float g, float b,
                              const Vec3& lightDir, float lightIntensity,
                              float ambient) {
    if (backend != RenderBackend::OPENGL) return;

    glUseProgram(shaderProgram);

    Mat4 mvp = viewProj * model;
    int mvpLoc = glGetUniformLocation(shaderProgram, "uMVP");
    int modelLoc = glGetUniformLocation(shaderProgram, "uModel");
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, mvp.m);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model.m);

    int colorLoc = glGetUniformLocation(shaderProgram, "uColor");
    glUniform4f(colorLoc, r, g, b, 1.0f);

    Vec3 ld = lightDir.normalized();
    int lightDirLoc = glGetUniformLocation(shaderProgram, "uLightDir");
    glUniform3f(lightDirLoc, ld.x, ld.y, ld.z);
    glUniform1f(glGetUniformLocation(shaderProgram, "uLightIntensity"), lightIntensity);
    glUniform1f(glGetUniformLocation(shaderProgram, "uAmbient"), ambient);

    mesh.bind();
    glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void Renderer3D::setLightDirection(float x, float y, float z) {
    g_lightDir = {x, y, z};
}

void Renderer3D::setAmbient(float a) {
    g_ambient = a;
}

void Renderer3D::setCameraPosition(float x, float y, float z) {
    (void)x; (void)y; (void)z;
}

void Renderer3D::onResize(float w, float h) {
    screenW = w;
    screenH = h;
    glViewport(0, 0, (int)w, (int)h);
}
