#pragma once
#include "Math.hpp"

class Camera3D {
public:
    Vec3 position;
    Vec3 target;
    Vec3 up;
    float fov;
    float aspect;
    float nearPlane;
    float farPlane;

    Camera3D();
    Camera3D(float aspect);

    Mat4 getViewMatrix() const;
    Mat4 getProjectionMatrix() const;
    void lookAt(const Vec3& t);
    void moveForward(float amount);
    void moveRight(float amount);
    void moveUp(float amount);
    void rotate(float yawDeg, float pitchDeg);
};
