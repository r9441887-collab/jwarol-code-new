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
#include "../include/Camera3D.hpp"
#include "../include/Mesh3D.hpp"
#include "../include/Renderer3D.hpp"
#include "../include/VulkanRenderer3D.hpp"

#ifdef _WIN32
#include "../include/D3D11Renderer.hpp"
#include "../include/D3D11Renderer3D.hpp"
#endif

namespace py = pybind11;

PYBIND11_MODULE(jwarol2, m) {
    m.doc() = "High-performance creative engine (OpenGL/Vulkan/DirectX11, Win32/Wayland/X11)";

    m.def("init_graphics", &LoadGLFunctions);

    py::class_<Vec2>(m, "Vec2")
        .def(py::init<float, float>())
        .def_readwrite("x", &Vec2::x)
        .def_readwrite("y", &Vec2::y)
        .def("__add__", [](const Vec2& a, const Vec2& b) { return a + b; })
        .def("__sub__", [](const Vec2& a, const Vec2& b) { return a - b; })
        .def("__mul__", [](const Vec2& a, float s) { return a * s; });

    py::class_<Vec3>(m, "Vec3")
        .def(py::init<float, float, float>())
        .def_readwrite("x", &Vec3::x)
        .def_readwrite("y", &Vec3::y)
        .def_readwrite("z", &Vec3::z)
        .def("__add__", [](const Vec3& a, const Vec3& b) { return a + b; })
        .def("__sub__", [](const Vec3& a, const Vec3& b) { return a - b; })
        .def("__mul__", [](const Vec3& a, float s) { return a * s; })
        .def("length", &Vec3::length)
        .def("normalized", &Vec3::normalized)
        .def("dot", &Vec3::dot)
        .def("cross", &Vec3::cross);

    py::class_<Mat4>(m, "Mat4")
        .def(py::init<>())
        .def_static("identity", &Mat4::identity)
        .def_static("perspective", &Mat4::perspective)
        .def_static("ortho", &Mat4::ortho)
        .def_static("look_at", &Mat4::lookAt)
        .def_static("translate", &Mat4::translate)
        .def_static("scale", &Mat4::scale)
        .def_static("rotate_x", &Mat4::rotateX)
        .def_static("rotate_y", &Mat4::rotateY)
        .def_static("rotate_z", &Mat4::rotateZ)
        .def("__mul__", [](const Mat4& a, const Mat4& b) { return a * b; })
        .def("transform_point", &Mat4::transformPoint)
        .def("data", &Mat4::data, py::return_value_policy::reference);

    py::class_<Rect>(m, "Rect")
        .def(py::init<float, float, float, float>())
        .def("intersects", &Rect::intersects)
        .def("contains", &Rect::contains)
        .def_readwrite("x", &Rect::x)
        .def_readwrite("y", &Rect::y)
        .def_readwrite("w", &Rect::w)
        .def_readwrite("h", &Rect::h);

    py::class_<AuraWindow>(m, "Window")
        .def(py::init<int, int, const std::string&, bool>(),
             py::arg("width"), py::arg("height"), py::arg("title"),
             py::arg("use_d3d11") = false)
        .def("poll_events", &AuraWindow::pollEvents)
        .def("clear", &AuraWindow::clear)
        .def("swap_buffers", &AuraWindow::swapBuffers)
        .def_property_readonly("is_open", &AuraWindow::isOpen)
        .def_property_readonly("is_d3d11", &AuraWindow::isD3D11)
        .def("set_input_manager", &AuraWindow::setInputManager);

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

#ifdef _WIN32
    py::class_<D3D11Renderer>(m, "D3D11Renderer")
        .def(py::init([](float w, float h, AuraWindow& win) {
            return new D3D11Renderer(w, h, win);
        }), py::arg("width"), py::arg("height"), py::arg("window"))
        .def("draw_rect", &D3D11Renderer::draw_rect)
        .def("draw_texture", &D3D11Renderer::draw_texture,
            py::arg("tex"), py::arg("x"), py::arg("y"),
            py::arg("w"), py::arg("h"), py::arg("alpha") = 1.0f)
        .def("draw_texture_ex", &D3D11Renderer::draw_texture_ex,
            py::arg("tex"), py::arg("x"), py::arg("y"),
            py::arg("w"), py::arg("h"),
            py::arg("sx"), py::arg("sy"), py::arg("sw"), py::arg("sh"),
            py::arg("alpha") = 1.0f)
        .def("clear", &D3D11Renderer::clear)
        .def("present", &D3D11Renderer::present);
#endif

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
        .def("add_body", (int(PhysicsWorld::*)(float,float,float,float,BodyType))&PhysicsWorld::addBody,
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

    // ============ 3D Classes ============

    py::class_<Vertex>(m, "Vertex")
        .def(py::init<>())
        .def_readwrite("position", &Vertex::position)
        .def_readwrite("normal", &Vertex::normal)
        .def_readwrite("uv", &Vertex::uv);

    py::enum_<RenderBackend>(m, "RenderBackend")
        .value("OPENGL", RenderBackend::OPENGL)
        .value("VULKAN", RenderBackend::VULKAN)
        .value("DX11", RenderBackend::DX11);

    py::class_<Camera3D>(m, "Camera3D")
        .def(py::init<>())
        .def(py::init<float>(), py::arg("aspect"))
        .def_readwrite("position", &Camera3D::position)
        .def_readwrite("target", &Camera3D::target)
        .def_readwrite("up", &Camera3D::up)
        .def_readwrite("fov", &Camera3D::fov)
        .def_readwrite("aspect", &Camera3D::aspect)
        .def_readwrite("near_plane", &Camera3D::nearPlane)
        .def_readwrite("far_plane", &Camera3D::farPlane)
        .def("get_view_matrix", &Camera3D::getViewMatrix)
        .def("get_projection_matrix", &Camera3D::getProjectionMatrix)
        .def("look_at", &Camera3D::lookAt)
        .def("move_forward", &Camera3D::moveForward)
        .def("move_right", &Camera3D::moveRight)
        .def("move_up", &Camera3D::moveUp)
        .def("rotate", &Camera3D::rotate);

    py::class_<Mesh3D>(m, "Mesh3D")
        .def(py::init<>())
        .def("load_cube", &Mesh3D::loadCube)
        .def("load_plane", &Mesh3D::loadPlane)
        .def("load_sphere", &Mesh3D::loadSphere,
            py::arg("rings") = 16, py::arg("sectors") = 32)
        .def("load_from_vertices", &Mesh3D::loadFromVertices)
        .def("load_from_positions", &Mesh3D::loadFromPositions,
            py::arg("positions"), py::arg("stride") = 3)
        .def("destroy", &Mesh3D::destroy)
        .def_property_readonly("index_count", [](const Mesh3D& m) { return m.indexCount; });

    py::class_<Renderer3D>(m, "Renderer3D")
        .def(py::init<float, float, RenderBackend>(),
            py::arg("width"), py::arg("height"),
            py::arg("backend") = RenderBackend::OPENGL)
        .def("begin_frame", &Renderer3D::beginFrame)
        .def("end_frame", &Renderer3D::endFrame)
        .def("draw_mesh", &Renderer3D::drawMesh,
            py::arg("mesh"), py::arg("model"),
            py::arg("r"), py::arg("g"), py::arg("b"),
            py::arg("a") = 1.0f)
        .def("draw_mesh_lit", &Renderer3D::drawMeshLit,
            py::arg("mesh"), py::arg("model"),
            py::arg("r"), py::arg("g"), py::arg("b"),
            py::arg("light_dir") = Vec3{0.5f, -1.0f, 0.3f},
            py::arg("light_intensity") = 1.0f,
            py::arg("ambient") = 0.15f)
        .def("set_light_direction", &Renderer3D::setLightDirection)
        .def("set_ambient", &Renderer3D::setAmbient)
        .def("on_resize", &Renderer3D::onResize)
        .def_property_readonly("screen_width", &Renderer3D::getScreenWidth)
        .def_property_readonly("screen_height", &Renderer3D::getScreenHeight)
        .def_property_readonly("backend", &Renderer3D::getBackend);

    py::class_<VulkanRenderer3D>(m, "VulkanRenderer3D")
        .def(py::init([](float w, float h, AuraWindow& win) {
            return new VulkanRenderer3D(w, h, win);
        }), py::arg("width"), py::arg("height"), py::arg("window"))
        .def("begin_frame", &VulkanRenderer3D::beginFrame)
        .def("end_frame", &VulkanRenderer3D::endFrame)
        .def("draw_mesh", &VulkanRenderer3D::drawMesh)
        .def("draw_mesh_lit", &VulkanRenderer3D::drawMeshLit)
        .def("set_light_direction", &VulkanRenderer3D::setLightDirection)
        .def("set_ambient", &VulkanRenderer3D::setAmbient)
        .def("on_resize", &VulkanRenderer3D::onResize)
        .def_property_readonly("is_initialized", &VulkanRenderer3D::isInitialized)
        .def_property_readonly("screen_width", &VulkanRenderer3D::getScreenWidth)
        .def_property_readonly("screen_height", &VulkanRenderer3D::getScreenHeight);

#ifdef _WIN32
    py::class_<D3D11Renderer3D>(m, "D3D11Renderer3D")
        .def(py::init([](float w, float h, AuraWindow& win) {
            return new D3D11Renderer3D(w, h, win);
        }), py::arg("width"), py::arg("height"), py::arg("window"))
        .def("begin_frame", &D3D11Renderer3D::beginFrame)
        .def("end_frame", &D3D11Renderer3D::endFrame)
        .def("draw_mesh", &D3D11Renderer3D::drawMesh)
        .def("draw_mesh_lit", &D3D11Renderer3D::drawMeshLit)
        .def("set_light_direction", &D3D11Renderer3D::setLightDirection)
        .def("set_ambient", &D3D11Renderer3D::setAmbient)
        .def("on_resize", &D3D11Renderer3D::onResize)
        .def_property_readonly("is_initialized", &D3D11Renderer3D::isInitialized);
#endif
}
