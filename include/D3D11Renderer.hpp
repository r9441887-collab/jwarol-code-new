#pragma once

#if defined(_WIN32) && !defined(__linux__)

#include <string>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11RenderTargetView;
struct ID3D11Buffer;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11InputLayout;
struct ID3D11ShaderResourceView;
struct ID3D11SamplerState;
struct IDXGISwapChain;

class AuraWindow;
class Texture;

class D3D11Renderer {
public:
    D3D11Renderer(float width, float height, AuraWindow& window);
    ~D3D11Renderer();

    void clear(float r, float g, float b);
    void present();

    void draw_rect(float x, float y, float w, float h, float r, float g, float b);
    void draw_texture(Texture& tex, float x, float y, float w, float h, float alpha = 1.0f);
    void draw_texture_ex(Texture& tex, float x, float y, float w, float h,
                         float sx, float sy, float sw, float sh, float alpha = 1.0f);

    void* getDevice() const { return device; }
    void* getContext() const { return context; }

private:
    float screenW, screenH;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    ID3D11RenderTargetView* renderTargetView = nullptr;
    ID3D11Buffer* constantBuffer = nullptr;
    ID3D11Buffer* colorConstantBuffer = nullptr;

    ID3D11VertexShader* rectVS = nullptr;
    ID3D11PixelShader* rectPS = nullptr;
    ID3D11InputLayout* rectLayout = nullptr;

    ID3D11VertexShader* texVS = nullptr;
    ID3D11PixelShader* texPS = nullptr;
    ID3D11InputLayout* texLayout = nullptr;

    ID3D11Buffer* rectVB = nullptr;
    ID3D11Buffer* texVB = nullptr;
    IDXGISwapChain* swapChain = nullptr;
    ID3D11SamplerState* samplerState = nullptr;

    struct TexCache {
        Texture* tex = nullptr;
        ID3D11ShaderResourceView* srv = nullptr;
    };
    TexCache texCache[64];
    int texCacheCount = 0;

    bool initD3D(AuraWindow& window);
    void createShaders();
    void setupRectPipeline();
    void setupTexPipeline();

    void updateConstantBuffer(float x, float y, float w, float h);
    ID3D11ShaderResourceView* getOrCreateSRV(Texture& tex);
    void setTexConstantBuffer(float alpha);
};

#endif
