#include "../include/Camera3D.hpp"
#include <cmath>

Camera3D::Camera3D()
    : position{0, 2, 5}, target{0, 0, 0}, up{0, 1, 0},
      fov(60.0f), aspect(4.0f/3.0f), nearPlane(0.1f), farPlane(100.0f) {}

Camera3D::Camera3D(float aspect)
    : position{0, 2, 5}, target{0, 0, 0}, up{0, 1, 0},
      fov(60.0f), aspect(aspect), nearPlane(0.1f), farPlane(100.0f) {}

Mat4 Camera3D::getViewMatrix() const {
    return Mat4::lookAt(position, target, up);
}

Mat4 Camera3D::getProjectionMatrix() const {
    return Mat4::perspective(fov, aspect, nearPlane, farPlane);
}

void Camera3D::lookAt(const Vec3& t) {
    target = t;
}

void Camera3D::moveForward(float amount) {
    Vec3 dir = (target - position).normalized();
    Vec3 move = dir * amount;
    position = position + move;
    target = target + move;
}

void Camera3D::moveRight(float amount) {
    Vec3 dir = (target - position).normalized();
    Vec3 right = dir.cross(up).normalized();
    Vec3 move = right * amount;
    position = position + move;
    target = target + move;
}

void Camera3D::moveUp(float amount) {
    Vec3 move = up * amount;
    position = position + move;
    target = target + move;
}

void Camera3D::rotate(float yawDeg, float pitchDeg) {
    Vec3 dir = (target - position).normalized();
    Vec3 right = dir.cross(up).normalized();

    float yawRad = yawDeg * 3.14159265f / 180.0f;
    float pitchRad = pitchDeg * 3.14159265f / 180.0f;

    // Yaw
    float cosY = std::cos(yawRad), sinY = std::sin(yawRad);
    dir = {
        cosY * dir.x + sinY * dir.z,
        dir.y,
        -sinY * dir.x + cosY * dir.z
    };

    // Pitch
    float cosP = std::cos(pitchRad), sinP = std::sin(pitchRad);
    Vec3 newDir = dir * cosP + up * sinP;
    up = up * cosP - dir * sinP;
    dir = newDir;

    target = position + dir;
}
