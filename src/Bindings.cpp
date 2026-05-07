#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "../include/Window.hpp"
#include "../include/Shader.hpp"
#include "../include/GLFunctions.hpp"
#include "../include/Math.hpp"
#include "../include/Input.hpp"
#include "../include/Clock.hpp"
#include "../include/Physics.hpp"
#include "../include/Renderer.hpp"
#include "../include/Texture.hpp"

namespace py = pybind11;

PYBIND11_MODULE(jwarol, m) {
    m.def("init_graphics", &LoadGLFunctions);

    py::class_<Vec2>(m, "Vec2")
        .def(py::init<float, float>())
        .def_readwrite("x", &Vec2::x)
        .def_readwrite("y", &Vec2::y)
        .def("__add__", [](const Vec2& a, const Vec2& b) { return a + b; })
        .def("__sub__", [](const Vec2& a, const Vec2& b) { return a - b; })
        .def("__mul__", [](const Vec2& a, float s) { return a * s; });

    py::class_<Rect>(m, "Rect")
        .def(py::init<float, float, float, float>())
        .def("intersects", &Rect::intersects)
        .def("contains", &Rect::contains)
        .def_readwrite("x", &Rect::x)
        .def_readwrite("y", &Rect::y)
        .def_readwrite("w", &Rect::w)
        .def_readwrite("h", &Rect::h);

    py::class_<AuraWindow>(m, "Window")
        .def(py::init<int, int, const std::string&>())
        .def("poll_events", &AuraWindow::pollEvents)
        .def("clear", &AuraWindow::clear)
        .def("swap_buffers", &AuraWindow::swapBuffers)
        .def_property_readonly("is_open", &AuraWindow::isOpen);

    py::class_<Renderer>(m, "Renderer")
        .def(py::init<float, float>())
        .def("draw_rect", &Renderer::draw_rect)
        .def("draw_texture", &Renderer::draw_texture,
            py::arg("tex"), py::arg("x"), py::arg("y"),
            py::arg("w"), py::arg("h"), py::arg("alpha") = 1.0f)
        .def("draw_texture_ex", &Renderer::draw_texture_ex,
            py::arg("tex"), py::arg("x"), py::arg("y"),
            py::arg("w"), py::arg("h"),
            py::arg("sx"), py::arg("sy"), py::arg("sw"), py::arg("sh"),
            py::arg("alpha") = 1.0f);

    py::class_<InputManager>(m, "Input")
        .def(py::init<>())
        .def("update", &InputManager::update)
        .def("is_down", &InputManager::isDown)
        .def("is_pressed", &InputManager::isPressed)
        .def("is_released", &InputManager::isReleased);

    py::class_<Clock>(m, "Clock")
        .def(py::init<>())
        .def("tick", &Clock::tick);

    py::class_<PhysicsPoint>(m, "Point")
        .def_readwrite("pos", &PhysicsPoint::pos)
        .def_readwrite("pinned", &PhysicsPoint::pinned)
        .def_readwrite("enabled", &PhysicsPoint::enabled);

    py::enum_<BodyType>(m, "BodyType")
        .value("DYNAMIC", BodyType::DYNAMIC)
        .value("STATIC", BodyType::STATIC)
        .value("KINEMATIC", BodyType::KINEMATIC);

    py::class_<PhysicsBody>(m, "Body")
        .def(py::init<>())
        .def_readwrite("pos", &PhysicsBody::pos)
        .def_readwrite("size", &PhysicsBody::size)
        .def_readwrite("velocity", &PhysicsBody::velocity)
        .def_readwrite("type", &PhysicsBody::type)
        .def_readwrite("enabled", &PhysicsBody::enabled)
        .def("apply_force", &PhysicsBody::applyForce);

    py::class_<PhysicsWorld>(m, "World")
        .def(py::init<>())
        .def_readwrite("gravity", &PhysicsWorld::gravity)
        .def("add_point", &PhysicsWorld::addPoint)
        .def("step", &PhysicsWorld::step)
        .def("add_body", (void(PhysicsWorld::*)(float,float,float,float,BodyType))&PhysicsWorld::addBody,
            py::arg("x"), py::arg("y"), py::arg("w"), py::arg("h"),
            py::arg("type") = BodyType::DYNAMIC)
        .def("add_body_obj", (int(PhysicsWorld::*)(const PhysicsBody&))&PhysicsWorld::addBody)
        .def("remove_body", &PhysicsWorld::removeBody)
        .def_readwrite("points", &PhysicsWorld::points)
        .def_readwrite("bodies", &PhysicsWorld::bodies);

    py::class_<AuraShader>(m, "Shader")
        .def(py::init<const std::string&, const std::string&>())
        .def("use", &AuraShader::use);

    py::class_<Texture>(m, "Texture")
        .def(py::init<>())
        .def(py::init<const std::string&>())
        .def(py::init([](int w, int h, std::string data, int ch) {
            return new Texture(w, h, (const unsigned char*)data.data(), ch);
        }), py::arg("width"), py::arg("height"),
            py::arg("data"), py::arg("channels") = 4)
        .def("load", &Texture::load)
        .def("from_data", &Texture::fromData)
        .def("bind", &Texture::bind, py::arg("slot") = 0)
        .def("overlay", &Texture::overlay,
            py::arg("other"), py::arg("x"), py::arg("y"),
            py::arg("alpha") = 1.0f)
        .def_property_readonly("width", &Texture::getWidth)
        .def_property_readonly("height", &Texture::getHeight)
        .def("is_supported", &Texture::isSupported)
        .def_static("load_all", &Texture::loadAll);
}
