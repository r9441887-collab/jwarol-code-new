#pragma once

#ifdef _WIN32
#include "Math.hpp"
#include "Mesh3D.hpp"
#include "Camera3D.hpp"

class AuraWindow;

class D3D11Renderer3D {
private:
    float screenW, screenH;
    bool initialized;
    AuraWindow* window;

public:
    D3D11Renderer3D(float width, float height, AuraWindow& win);
    ~D3D11Renderer3D();

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
    bool isInitialized() const { return initialized; }
    void onResize(float w, float h);
};

#endif
