#pragma once
#include "Math.hpp"
#include "Mesh3D.hpp"
#include "Camera3D.hpp"

enum class RenderBackend {
    OPENGL,
    VULKAN,
    DX11
};

class Texture;

class Renderer3D {
private:
    RenderBackend backend;
    unsigned int shaderProgram;
    unsigned int depthProgram;
    float screenW, screenH;
    Mat4 viewProj;

    void initShaders();

public:
    Renderer3D(float width, float height, RenderBackend backend = RenderBackend::OPENGL);
    ~Renderer3D();

    void beginFrame(Camera3D& camera);
    void endFrame();

    void drawMesh(Mesh3D& mesh, const Mat4& model,
                  float r, float g, float b, float a = 1.0f);
    void drawMeshLit(Mesh3D& mesh, const Mat4& model,
                     float r, float g, float b,
                     const Vec3& lightDir, float lightIntensity = 1.0f,
                     float ambient = 0.15f);

    void setLightDirection(float x, float y, float z);
    void setAmbient(float a);
    void setCameraPosition(float x, float y, float z);

    RenderBackend getBackend() const { return backend; }
    float getScreenWidth() const { return screenW; }
    float getScreenHeight() const { return screenH; }
    void onResize(float w, float h);
};
