#ifdef _WIN32
#include "../include/D3D11Renderer3D.hpp"
#include "../include/Window.hpp"
#include <cstdio>

D3D11Renderer3D::D3D11Renderer3D(float width, float height, AuraWindow& win)
    : screenW(width), screenH(height), initialized(false), window(&win) {
    fprintf(stderr, "D3D11Renderer3D: backend not yet implemented\n");
}

D3D11Renderer3D::~D3D11Renderer3D() {}
void D3D11Renderer3D::beginFrame(Camera3D& camera) {}
void D3D11Renderer3D::endFrame() {}
void D3D11Renderer3D::drawMesh(Mesh3D& mesh, const Mat4& model,
                                float r, float g, float b, float a) {}
void D3D11Renderer3D::drawMeshLit(Mesh3D& mesh, const Mat4& model,
                                    float r, float g, float b,
                                    const Vec3& lightDir, float lightIntensity,
                                    float ambient) {}
void D3D11Renderer3D::setLightDirection(float x, float y, float z) {}
void D3D11Renderer3D::setAmbient(float a) {}
void D3D11Renderer3D::onResize(float w, float h) { screenW = w; screenH = h; }

#endif
