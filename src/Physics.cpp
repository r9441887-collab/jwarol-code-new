#include "../include/Physics.hpp"
#include <algorithm>

void PhysicsPoint::update(float dt) {
    if (pinned || !enabled) return;

    Vec2 velocity = pos - old_pos;
    old_pos = pos;

    pos = pos + velocity + accel * (dt * dt);
    accel = { 0, 0 };

    if (pos.y > 700) {
        pos.y = 700;
        old_pos.y = pos.y;
    }
}

void PhysicsWorld::step(float dt) {
    for (auto& p : points) {
        if (!p.enabled) continue;
        p.accel.y += gravity;
        p.update(dt);
    }

    for (auto& b : bodies) {
        if (!b.enabled || b.type == BodyType::STATIC) continue;
        b.velocity.y += gravity * dt;
        b.pos = b.pos + b.velocity * dt;

        if (b.type != BodyType::KINEMATIC && b.pos.y > 700 - b.size.y) {
            b.pos.y = 700 - b.size.y;
            b.velocity.y = 0;
        }
    }

    resolveCollisions();
}

void PhysicsWorld::addPoint(float x, float y) {
    points.push_back({ {x, y}, {x, y}, {0, 0}, false, true });
}

void PhysicsWorld::addBody(float x, float y, float w, float h, BodyType type) {
    bodies.push_back({ {x, y}, {w, h}, {0, 0}, type, true });
}

int PhysicsWorld::addBody(const PhysicsBody& body) {
    bodies.push_back(body);
    return (int)bodies.size() - 1;
}

void PhysicsWorld::removeBody(int index) {
    if (index >= 0 && index < (int)bodies.size())
        bodies.erase(bodies.begin() + index);
}

void PhysicsWorld::resolveCollisions() {
    for (size_t i = 0; i < bodies.size(); i++) {
        if (!bodies[i].enabled) continue;
        for (size_t j = i + 1; j < bodies.size(); j++) {
            if (!bodies[j].enabled) continue;
            if (bodies[i].type == BodyType::STATIC && bodies[j].type == BodyType::STATIC) continue;

            Rect a = { bodies[i].pos.x, bodies[i].pos.y, bodies[i].size.x, bodies[i].size.y };
            Rect b = { bodies[j].pos.x, bodies[j].pos.y, bodies[j].size.x, bodies[j].size.y };

            if (!a.intersects(b)) continue;

            Vec2 overlap = {
                std::min(a.x + a.w, b.x + b.w) - std::max(a.x, b.x),
                std::min(a.y + a.h, b.y + b.h) - std::max(a.y, b.y)
            };

            if (overlap.x < overlap.y) {
                if (bodies[i].pos.x < bodies[j].pos.x) {
                    bodies[i].pos.x -= overlap.x / 2;
                    bodies[j].pos.x += overlap.x / 2;
                } else {
                    bodies[i].pos.x += overlap.x / 2;
                    bodies[j].pos.x -= overlap.x / 2;
                }
            } else {
                if (bodies[i].pos.y < bodies[j].pos.y) {
                    bodies[i].pos.y -= overlap.y / 2;
                    bodies[j].pos.y += overlap.y / 2;
                } else {
                    bodies[i].pos.y += overlap.y / 2;
                    bodies[j].pos.y -= overlap.y / 2;
                }
            }
        }
    }
}
