#pragma once
#include "Math.hpp"
#include <vector>

struct Vertex {
    Vec3 position;
    Vec3 normal;
    Vec2 uv;
};

class Mesh3D {
public:
    unsigned int VAO = 0, VBO = 0, EBO = 0;
    int indexCount = 0;
    std::vector<Vertex> cpuVertices;
    std::vector<unsigned int> cpuIndices;

    Mesh3D();
    ~Mesh3D();

    void loadFromVertices(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
    void loadFromPositions(const std::vector<float>& positions, int vertexStride);
    void loadCube();
    void loadPlane(float size);
    void loadSphere(int rings, int sectors);
    void bind() const;
    void destroy();
};
