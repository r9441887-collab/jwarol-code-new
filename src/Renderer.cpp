#include "../include/Renderer.hpp"
#include "../include/Texture.hpp"
#include "../include/GLFunctions.hpp"
#include <GL/gl.h>

static unsigned int compileShader(const char* src, unsigned int type) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    return shader;
}

static unsigned int linkProgram(unsigned int vShader, unsigned int fShader) {
    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vShader);
    glAttachShader(prog, fShader);
    glLinkProgram(prog);
    glDetachShader(prog, vShader);
    glDetachShader(prog, fShader);
    glDeleteShader(vShader);
    glDeleteShader(fShader);
    return prog;
}

static void orthoMatrix(float* out, float l, float r, float b, float t) {
    out[0]  =  2.0f / (r - l);
    out[1]  =  0.0f;
    out[2]  =  0.0f;
    out[3]  =  0.0f;
    out[4]  =  0.0f;
    out[5]  =  2.0f / (t - b);
    out[6]  =  0.0f;
    out[7]  =  0.0f;
    out[8]  =  0.0f;
    out[9]  =  0.0f;
    out[10] = -1.0f;
    out[11] =  0.0f;
    out[12] = -(r + l) / (r - l);
    out[13] = -(t + b) / (t - b);
    out[14] =  0.0f;
    out[15] =  1.0f;
}

Renderer::Renderer(float width, float height) : screenW(width), screenH(height) {
    glGenVertexArrays(1, &rectVAO);
    glGenBuffers(1, &rectVBO);

    glGenVertexArrays(1, &texVAO);
    glGenBuffers(1, &texVBO);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const char* texVS = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTex;
uniform mat4 uProj;
out vec2 vTex;
void main() {
    gl_Position = uProj * vec4(aPos, 0.0, 1.0);
    vTex = aTex;
}
)";
    const char* texFS = R"(
#version 330 core
in vec2 vTex;
out vec4 FragColor;
uniform sampler2D uTex;
uniform float uAlpha;
void main() {
    FragColor = texture(uTex, vTex) * uAlpha;
}
)";
    unsigned int vs = compileShader(texVS, GL_VERTEX_SHADER);
    unsigned int fs = compileShader(texFS, GL_FRAGMENT_SHADER);
    texProgram = linkProgram(vs, fs);

    const char* rectVS = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uProj;
void main() {
    gl_Position = uProj * vec4(aPos, 1.0);
}
)";
    const char* rectFS = R"(
#version 330 core
uniform vec4 uColor;
out vec4 FragColor;
void main() {
    FragColor = uColor;
}
)";
    vs = compileShader(rectVS, GL_VERTEX_SHADER);
    fs = compileShader(rectFS, GL_FRAGMENT_SHADER);
    rectProgram = linkProgram(vs, fs);
}

Renderer::~Renderer() {
    glDeleteProgram(texProgram);
    glDeleteProgram(rectProgram);
    glDeleteBuffers(1, &rectVBO);
    glDeleteVertexArrays(1, &rectVAO);
    glDeleteBuffers(1, &texVBO);
    glDeleteVertexArrays(1, &texVAO);
}

void Renderer::draw_rect(float x, float y, float w, float h, float r, float g, float b) {
    float vertices[] = {
        x,   y,   0.0f,
        x+w, y,   0.0f,
        x+w, y+h, 0.0f,
        x,   y+h, 0.0f
    };

    glUseProgram(rectProgram);

    float proj[16];
    orthoMatrix(proj, 0, screenW, screenH, 0);
    glUniformMatrix4fv(glGetUniformLocation(rectProgram, "uProj"), 1, GL_FALSE, proj);

    int colorLoc = glGetUniformLocation(rectProgram, "uColor");
    glUniform4f(colorLoc, r, g, b, 1.0f);

    glBindVertexArray(rectVAO);
    glBindBuffer(GL_ARRAY_BUFFER, rectVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, false, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void Renderer::draw_texture(Texture& tex, float x, float y, float w, float h, float alpha) {
    float vertices[] = {
        x,   y,   0.0f, 0.0f,
        x+w, y,   1.0f, 0.0f,
        x+w, y+h, 1.0f, 1.0f,
        x,   y+h, 0.0f, 1.0f
    };

    glUseProgram(texProgram);

    float proj[16];
    orthoMatrix(proj, 0, screenW, screenH, 0);
    glUniformMatrix4fv(glGetUniformLocation(texProgram, "uProj"), 1, GL_FALSE, proj);

    glUniform1f(glGetUniformLocation(texProgram, "uAlpha"), alpha);

    tex.bind(0);
    int texLoc = glGetUniformLocation(texProgram, "uTex");
    glUniform1i(texLoc, 0);

    glBindVertexArray(texVAO);
    glBindBuffer(GL_ARRAY_BUFFER, texVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, false, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, false, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void Renderer::draw_texture_ex(Texture& tex, float x, float y, float w, float h,
                                float sx, float sy, float sw, float sh, float alpha) {
    float tw = (float)tex.getWidth();
    float th = (float)tex.getHeight();
    float u0 = sx / tw, v0 = sy / th;
    float u1 = (sx + sw) / tw, v1 = (sy + sh) / th;

    float vertices[] = {
        x,   y,   u0, v0,
        x+w, y,   u1, v0,
        x+w, y+h, u1, v1,
        x,   y+h, u0, v1
    };

    glUseProgram(texProgram);

    float proj[16];
    orthoMatrix(proj, 0, screenW, screenH, 0);
    glUniformMatrix4fv(glGetUniformLocation(texProgram, "uProj"), 1, GL_FALSE, proj);

    glUniform1f(glGetUniformLocation(texProgram, "uAlpha"), alpha);

    tex.bind(0);
    glUniform1i(glGetUniformLocation(texProgram, "uTex"), 0);

    glBindVertexArray(texVAO);
    glBindBuffer(GL_ARRAY_BUFFER, texVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, false, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, false, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}
