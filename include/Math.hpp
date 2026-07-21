#pragma once
#include <cmath>
#include <cstring>

struct Vec2 {
    float x, y;
    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
};

struct Vec3 {
    float x, y, z;
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator-() const { return {-x, -y, -z}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3 cross(const Vec3& o) const {
        return {y*o.z - z*o.y, z*o.x - x*o.z, x*o.y - y*o.x};
    }
    float dot(const Vec3& o) const { return x*o.x + y*o.y + z*o.z; }
    float length() const { return std::sqrt(x*x + y*y + z*z); }
    Vec3 normalized() const {
        float l = length();
        if (l < 1e-8f) return {0, 0, 0};
        return {x/l, y/l, z/l};
    }
};

struct Mat4 {
    float m[16];

    static Mat4 identity() {
        Mat4 r{};
        r.m[0]=1; r.m[5]=1; r.m[10]=1; r.m[15]=1;
        return r;
    }

    Mat4 operator*(const Mat4& o) const {
        Mat4 r{};
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++) {
                float sum = 0;
                for (int k = 0; k < 4; k++)
                    sum += m[i + k*4] * o.m[k + j*4];
                r.m[i + j*4] = sum;
            }
        return r;
    }

    Vec3 transformPoint(const Vec3& p) const {
        float w = m[3]*p.x + m[7]*p.y + m[11]*p.z + m[15];
        return {
            (m[0]*p.x + m[4]*p.y + m[8]*p.z  + m[12]) / w,
            (m[1]*p.x + m[5]*p.y + m[9]*p.z  + m[13]) / w,
            (m[2]*p.x + m[6]*p.y + m[10]*p.z + m[14]) / w
        };
    }

    static Mat4 perspective(float fovDeg, float aspect, float near, float far) {
        float f = 1.0f / std::tan(fovDeg * 3.14159265f / 360.0f);
        Mat4 r{};
        r.m[0] = f / aspect;
        r.m[5] = f;
        r.m[10] = (far + near) / (near - far);
        r.m[11] = -1.0f;
        r.m[14] = (2.0f * far * near) / (near - far);
        return r;
    }

    static Mat4 ortho(float left, float right, float bottom, float top, float near, float far) {
        Mat4 r{};
        r.m[0]  =  2.0f / (right - left);
        r.m[5]  =  2.0f / (top - bottom);
        r.m[10] = -2.0f / (far - near);
        r.m[12] = -(right + left) / (right - left);
        r.m[13] = -(top + bottom) / (top - bottom);
        r.m[14] = -(far + near) / (far - near);
        r.m[15] =  1.0f;
        return r;
    }

    static Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
        Vec3 f = (target - eye).normalized();
        Vec3 s = f.cross(up).normalized();
        Vec3 u = s.cross(f);

        Mat4 r = identity();
        r.m[0] = s.x;  r.m[4] = s.y;  r.m[8]  = s.z;
        r.m[1] = u.x;  r.m[5] = u.y;  r.m[9]  = u.z;
        r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
        r.m[12] = -s.dot(eye);
        r.m[13] = -u.dot(eye);
        r.m[14] = f.dot(eye);
        return r;
    }

    static Mat4 translate(float x, float y, float z) {
        Mat4 r = identity();
        r.m[12] = x; r.m[13] = y; r.m[14] = z;
        return r;
    }

    static Mat4 scale(float x, float y, float z) {
        Mat4 r{};
        r.m[0] = x; r.m[5] = y; r.m[10] = z; r.m[15] = 1;
        return r;
    }

    static Mat4 rotateX(float deg) {
        float rad = deg * 3.14159265f / 180.0f;
        float c = std::cos(rad), s = std::sin(rad);
        Mat4 r = identity();
        r.m[5] = c;  r.m[6] = s;
        r.m[9] = -s; r.m[10] = c;
        return r;
    }

    static Mat4 rotateY(float deg) {
        float rad = deg * 3.14159265f / 180.0f;
        float c = std::cos(rad), s = std::sin(rad);
        Mat4 r = identity();
        r.m[0] = c;  r.m[2] = -s;
        r.m[8] = s;  r.m[10] = c;
        return r;
    }

    static Mat4 rotateZ(float deg) {
        float rad = deg * 3.14159265f / 180.0f;
        float c = std::cos(rad), s = std::sin(rad);
        Mat4 r = identity();
        r.m[0] = c;  r.m[1] = s;
        r.m[4] = -s; r.m[5] = c;
        return r;
    }

    const float* data() const { return m; }
};

struct Rect {
    float x, y, w, h;
    bool contains(Vec2 p) const {
        return p.x >= x && p.x <= x + w && p.y >= y && p.y <= y + h;
    }
    bool intersects(const Rect& o) const {
        return !(x + w < o.x || x > o.x + o.w || y + h < o.y || y > o.y + o.h);
    }
};
