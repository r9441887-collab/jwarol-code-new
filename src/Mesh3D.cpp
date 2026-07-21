#include "../include/Mesh3D.hpp"
#include "../include/GLFunctions.hpp"
#include <GL/gl.h>
#include <cstddef>
#include <cmath>

Mesh3D::Mesh3D() {}

Mesh3D::~Mesh3D() {
    destroy();
}

void Mesh3D::destroy() {
    if (EBO) { glDeleteBuffers(1, &EBO); EBO = 0; }
    if (VBO) { glDeleteBuffers(1, &VBO); VBO = 0; }
    if (VAO) { glDeleteVertexArrays(1, &VAO); VAO = 0; }
    indexCount = 0;
}

void Mesh3D::loadFromVertices(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices) {
    destroy();
    indexCount = (int)indices.size();
    cpuVertices = vertices;
    cpuIndices = indices;

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, false, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, false, sizeof(Vertex), (void*)offsetof(Vertex, uv));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

void Mesh3D::loadFromPositions(const std::vector<float>& positions, int stride) {
    destroy();
    indexCount = (int)(positions.size() / stride);

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(float), positions.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, false, stride * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void Mesh3D::loadCube() {
    std::vector<Vertex> vertices = {
        // Front face
        {{-0.5f,-0.5f, 0.5f}, { 0, 0, 1}, {0, 0}},
        {{ 0.5f,-0.5f, 0.5f}, { 0, 0, 1}, {1, 0}},
        {{ 0.5f, 0.5f, 0.5f}, { 0, 0, 1}, {1, 1}},
        {{-0.5f, 0.5f, 0.5f}, { 0, 0, 1}, {0, 1}},
        // Back face
        {{ 0.5f,-0.5f,-0.5f}, { 0, 0,-1}, {0, 0}},
        {{-0.5f,-0.5f,-0.5f}, { 0, 0,-1}, {1, 0}},
        {{-0.5f, 0.5f,-0.5f}, { 0, 0,-1}, {1, 1}},
        {{ 0.5f, 0.5f,-0.5f}, { 0, 0,-1}, {0, 1}},
        // Top face
        {{-0.5f, 0.5f, 0.5f}, { 0, 1, 0}, {0, 0}},
        {{ 0.5f, 0.5f, 0.5f}, { 0, 1, 0}, {1, 0}},
        {{ 0.5f, 0.5f,-0.5f}, { 0, 1, 0}, {1, 1}},
        {{-0.5f, 0.5f,-0.5f}, { 0, 1, 0}, {0, 1}},
        // Bottom face
        {{-0.5f,-0.5f,-0.5f}, { 0,-1, 0}, {0, 0}},
        {{ 0.5f,-0.5f,-0.5f}, { 0,-1, 0}, {1, 0}},
        {{ 0.5f,-0.5f, 0.5f}, { 0,-1, 0}, {1, 1}},
        {{-0.5f,-0.5f, 0.5f}, { 0,-1, 0}, {0, 1}},
        // Right face
        {{ 0.5f,-0.5f, 0.5f}, { 1, 0, 0}, {0, 0}},
        {{ 0.5f,-0.5f,-0.5f}, { 1, 0, 0}, {1, 0}},
        {{ 0.5f, 0.5f,-0.5f}, { 1, 0, 0}, {1, 1}},
        {{ 0.5f, 0.5f, 0.5f}, { 1, 0, 0}, {0, 1}},
        // Left face
        {{-0.5f,-0.5f,-0.5f}, {-1, 0, 0}, {0, 0}},
        {{-0.5f,-0.5f, 0.5f}, {-1, 0, 0}, {1, 0}},
        {{-0.5f, 0.5f, 0.5f}, {-1, 0, 0}, {1, 1}},
        {{-0.5f, 0.5f,-0.5f}, {-1, 0, 0}, {0, 1}},
    };

    std::vector<unsigned int> indices;
    for (int i = 0; i < 6; i++) {
        unsigned int base = i * 4;
        indices.push_back(base); indices.push_back(base+1); indices.push_back(base+2);
        indices.push_back(base); indices.push_back(base+2); indices.push_back(base+3);
    }

    loadFromVertices(vertices, indices);
}

void Mesh3D::loadPlane(float size) {
    float h = size * 0.5f;
    std::vector<Vertex> vertices = {
        {{-h, 0, -h}, {0, 1, 0}, {0, 0}},
        {{ h, 0, -h}, {0, 1, 0}, {1, 0}},
        {{ h, 0,  h}, {0, 1, 0}, {1, 1}},
        {{-h, 0,  h}, {0, 1, 0}, {0, 1}},
    };
    std::vector<unsigned int> indices = {0, 1, 2, 0, 2, 3};
    loadFromVertices(vertices, indices);
}

void Mesh3D::loadSphere(int rings, int sectors) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    const float PI = 3.14159265f;
    for (int r = 0; r <= rings; r++) {
        float phi = PI * r / rings;
        float sinPhi = std::sin(phi), cosPhi = std::cos(phi);
        for (int s = 0; s <= sectors; s++) {
            float theta = 2.0f * PI * s / sectors;
            float x = cosPhi * std::cos(theta);
            float y = sinPhi;
            float z = cosPhi * std::sin(theta);
            float u = (float)s / sectors;
            float v = (float)r / rings;
            vertices.push_back({{x*0.5f, y*0.5f, z*0.5f}, {x, y, z}, {u, v}});
        }
    }

    for (int r = 0; r < rings; r++) {
        for (int s = 0; s < sectors; s++) {
            int i0 = r * (sectors + 1) + s;
            int i1 = i0 + sectors + 1;
            indices.push_back(i0); indices.push_back(i1); indices.push_back(i0 + 1);
            indices.push_back(i0 + 1); indices.push_back(i1); indices.push_back(i1 + 1);
        }
    }

    loadFromVertices(vertices, indices);
}

void Mesh3D::bind() const {
    glBindVertexArray(VAO);
}
