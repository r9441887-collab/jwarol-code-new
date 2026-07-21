#pragma once
#include "Math.hpp"
#include "Mesh3D.hpp"
#include "Camera3D.hpp"

class AuraWindow;

struct VulkanRenderer3DInternal;

class VulkanRenderer3D {
private:
    VulkanRenderer3DInternal* impl;

public:
    VulkanRenderer3D(float width, float height, AuraWindow& win);
    ~VulkanRenderer3D();

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
    bool isInitialized() const;
    float getScreenWidth() const;
    float getScreenHeight() const;
    void onResize(float w, float h);
};
