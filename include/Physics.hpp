#pragma once
#include "Math.hpp"
#include <vector>

enum class BodyType : int {
    DYNAMIC = 0,
    STATIC = 1,
    KINEMATIC = 2
};

struct PhysicsBody {
    Vec2 pos;
    Vec2 size;
    Vec2 velocity;
    BodyType type = BodyType::DYNAMIC;
    bool enabled = true;

    void applyForce(const Vec2& force) { velocity = velocity + force; }
};

struct PhysicsPoint {
    Vec2 pos;
    Vec2 old_pos;
    Vec2 accel;
    bool pinned = false;
    bool enabled = true;

    void update(float dt);
};

class PhysicsWorld {
public:
    std::vector<PhysicsPoint> points;
    std::vector<PhysicsBody> bodies;
    float gravity = 980.0f;

    void step(float dt);
    void addPoint(float x, float y);
    int addBody(float x, float y, float w, float h, BodyType type = BodyType::DYNAMIC);
    int addBody(const PhysicsBody& body);
    void removeBody(int index);

    void resolveCollisions();
};
