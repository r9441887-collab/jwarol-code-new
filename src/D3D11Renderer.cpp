#include "../include/D3D11Renderer.hpp"
#include "../include/Window.hpp"
#include "../include/Texture.hpp"

#ifdef _WIN32

#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <cmath>
#include <cstring>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

static float orthoLH(float* out, float l, float r, float b, float t, float zn, float zf) {
    std::memset(out, 0, 16 * sizeof(float));
    out[0]  = 2.0f / (r - l);
    out[5]  = 2.0f / (t - b);
    out[10] = 1.0f / (zf - zn);
    out[12] = -(r + l) / (r - l);
    out[13] = -(t + b) / (t - b);
    out[14] = zn / (zn - zf);
    out[15] = 1.0f;
    return 0;
}

struct ConstBuffer {
    float proj[16];
    float param[4];
};

static const char* rectHLSL_VS = R"(
cbuffer CB : register(b0) {
    float4x4 gProj;
};
struct VSInput {
    float3 pos : POSITION;
};
struct VSOutput {
    float4 pos : SV_POSITION;
};
VSOutput main(VSInput input) {
    VSOutput o;
    o.pos = mul(float4(input.pos, 1.0f), gProj);
    return o;
}
)";

static const char* rectHLSL_PS = R"(
cbuffer CB : register(b0) {
    float4 gProj;
    float4 gColor;
};
float4 main() : SV_TARGET {
    return gColor;
}
)";

static const char* texHLSL_VS = R"(
cbuffer CB : register(b0) {
    float4x4 gProj;
};
struct VSInput {
    float3 pos : POSITION;
    float2 uv  : TEXCOORD0;
};
struct VSOutput {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};
VSOutput main(VSInput input) {
    VSOutput o;
    o.pos = mul(float4(input.pos, 1.0f), gProj);
    o.uv = input.uv;
    return o;
}
)";

static const char* texHLSL_PS = R"(
Texture2D gTex : register(t0);
SamplerState gSamp : register(s0);
cbuffer CB : register(b0) {
    float4 gProj;
    float4 gParams;
};
float4 main(VSOutput input) : SV_TARGET {
    return gTex.Sample(gSamp, input.uv) * gParams.x;
}
)";

struct ColorVertex { float x, y, z; };
struct TexVertex { float x, y, z, u, v; };

D3D11Renderer::D3D11Renderer(float width, float height, AuraWindow& window)
    : screenW(width), screenH(height) {
    if (!window.isD3D11()) return;
    if (!initD3D(window)) return;
    createShaders();
    setupRectPipeline();
    setupTexPipeline();
}

D3D11Renderer::~D3D11Renderer() {
    for (int i = 0; i < texCacheCount; i++) {
        if (texCache[i].srv) texCache[i].srv->Release();
    }
    if (samplerState) samplerState->Release();
    if (texVB) texVB->Release();
    if (rectVB) rectVB->Release();
    if (texLayout) texLayout->Release();
    if (texPS) texPS->Release();
    if (texVS) texVS->Release();
    if (rectLayout) rectLayout->Release();
    if (rectPS) rectPS->Release();
    if (rectVS) rectVS->Release();
    if (colorConstantBuffer) colorConstantBuffer->Release();
    if (constantBuffer) constantBuffer->Release();
    if (renderTargetView) renderTargetView->Release();
    if (context) context->Release();
    if (device) device->Release();
}

bool D3D11Renderer::initD3D(AuraWindow& window) {
    HWND hwnd = window.getHWND();
    if (!hwnd) return false;

    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 1;
    scd.BufferDesc.Width = (UINT)screenW;
    scd.BufferDesc.Height = (UINT)screenH;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hwnd;
    scd.SampleDesc.Count = 1;
    scd.SampleDesc.Quality = 0;
    scd.Windowed = TRUE;

    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION, &scd,
        &swapChain, &device, &featureLevel, &context
    );
    if (FAILED(hr)) {
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
            nullptr, 0, D3D11_SDK_VERSION, &scd,
            &swapChain, &device, &featureLevel, &context
        );
        if (FAILED(hr)) return false;
    }

    ID3D11Texture2D* backBuffer = nullptr;
    swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    if (backBuffer) {
        device->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView);
        backBuffer->Release();
    }

    context->OMSetRenderTargets(1, &renderTargetView, nullptr);

    D3D11_VIEWPORT vp = {};
    vp.Width = screenW;
    vp.Height = screenH;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    context->RSSetViewports(1, &vp);

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(ConstBuffer);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    device->CreateBuffer(&bd, nullptr, &constantBuffer);
    device->CreateBuffer(&bd, nullptr, &colorConstantBuffer);

    D3D11_SAMPLER_DESC sd = {};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    device->CreateSamplerState(&sd, &samplerState);

    return true;
}

static ID3DBlob* compileHLSL(const char* src, const char* target) {
    ID3DBlob* blob = nullptr;
    ID3DBlob* err = nullptr;
    HRESULT hr = D3DCompile(src, strlen(src), nullptr, nullptr, nullptr,
                            "main", target, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &blob, &err);
    if (err) err->Release();
    if (FAILED(hr) && blob) { blob->Release(); return nullptr; }
    return blob;
}

void D3D11Renderer::createShaders() {
    ID3DBlob* blob;

    blob = compileHLSL(rectHLSL_VS, "vs_4_0");
    if (blob) {
        device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(),
                                   nullptr, &rectVS);
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
        device->CreateInputLayout(layout, 1, blob->GetBufferPointer(),
                                  blob->GetBufferSize(), &rectLayout);
        blob->Release();
    }

    blob = compileHLSL(rectHLSL_PS, "ps_4_0");
    if (blob) {
        device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &rectPS);
        blob->Release();
    }

    blob = compileHLSL(texHLSL_VS, "vs_4_0");
    if (blob) {
        device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(),
                                   nullptr, &texVS);
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
        device->CreateInputLayout(layout, 2, blob->GetBufferPointer(),
                                  blob->GetBufferSize(), &texLayout);
        blob->Release();
    }

    blob = compileHLSL(texHLSL_PS, "ps_4_0");
    if (blob) {
        device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &texPS);
        blob->Release();
    }
}

void D3D11Renderer::setupRectPipeline() {
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(ColorVertex) * 4;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    device->CreateBuffer(&bd, nullptr, &rectVB);
}

void D3D11Renderer::setupTexPipeline() {
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(TexVertex) * 4;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    device->CreateBuffer(&bd, nullptr, &texVB);
}

void D3D11Renderer::updateConstantBuffer(float x, float y, float w, float h) {
    ConstBuffer cb;
    orthoLH(cb.proj, 0, screenW, screenH, 0, -1.0f, 1.0f);
    cb.param[0] = 0; cb.param[1] = 0; cb.param[2] = 0; cb.param[3] = 0;
    context->UpdateSubresource(constantBuffer, 0, nullptr, &cb, 0, 0);
}

ID3D11ShaderResourceView* D3D11Renderer::getOrCreateSRV(Texture& tex) {
    for (int i = 0; i < texCacheCount; i++) {
        if (texCache[i].tex == &tex) return texCache[i].srv;
    }
    const unsigned char* pixels = tex.getPixels();
    if (!pixels) return nullptr;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = tex.getWidth();
    desc.Height = tex.getHeight();
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = pixels;
    initData.SysMemPitch = tex.getWidth() * 4;

    ID3D11Texture2D* d3dTex = nullptr;
    HRESULT hr = device->CreateTexture2D(&desc, &initData, &d3dTex);
    if (FAILED(hr)) return nullptr;

    ID3D11ShaderResourceView* srv = nullptr;
    device->CreateShaderResourceView(d3dTex, nullptr, &srv);
    d3dTex->Release();

    if (srv && texCacheCount < 64) {
        texCache[texCacheCount] = { &tex, srv };
        texCacheCount++;
    }
    return srv;
}

void D3D11Renderer::setTexConstantBuffer(float alpha) {
    ConstBuffer cb;
    orthoLH(cb.proj, 0, screenW, screenH, 0, -1.0f, 1.0f);
    cb.param[0] = alpha; cb.param[1] = 0; cb.param[2] = 0; cb.param[3] = 0;
    context->UpdateSubresource(constantBuffer, 0, nullptr, &cb, 0, 0);
}

void D3D11Renderer::clear(float r, float g, float b) {
    if (!renderTargetView) return;
    float clearColor[4] = { r, g, b, 1.0f };
    context->ClearRenderTargetView(renderTargetView, clearColor);
}

void D3D11Renderer::present() {
    if (swapChain) swapChain->Present(1, 0);
}

void D3D11Renderer::draw_rect(float x, float y, float w, float h, float r, float g, float b) {
    if (!context) return;

    updateConstantBuffer(x, y, w, h);
    context->VSSetConstantBuffers(0, 1, &constantBuffer);

    ConstBuffer colorCB;
    orthoLH(colorCB.proj, 0, screenW, screenH, 0, -1.0f, 1.0f);
    colorCB.param[0] = r; colorCB.param[1] = g; colorCB.param[2] = b; colorCB.param[3] = 1.0f;
    context->UpdateSubresource(colorConstantBuffer, 0, nullptr, &colorCB, 0, 0);
    context->PSSetConstantBuffers(0, 1, &colorConstantBuffer);

    ColorVertex verts[4] = {
        { x,     y,     0.0f },
        { x + w, y,     0.0f },
        { x + w, y + h, 0.0f },
        { x,     y + h, 0.0f }
    };

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(context->Map(rectVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        memcpy(mapped.pData, verts, sizeof(verts));
        context->Unmap(rectVB, 0);
    }

    UINT stride = sizeof(ColorVertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &rectVB, &stride, &offset);
    context->IASetInputLayout(rectLayout);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLEFAN);

    context->VSSetShader(rectVS, nullptr, 0);
    context->PSSetShader(rectPS, nullptr, 0);
    context->Draw(4, 0);
}

void D3D11Renderer::draw_texture(Texture& tex, float x, float y, float w, float h, float alpha) {
    draw_texture_ex(tex, x, y, w, h, 0, 0, (float)tex.getWidth(), (float)tex.getHeight(), alpha);
}

void D3D11Renderer::draw_texture_ex(Texture& tex, float x, float y, float w, float h,
                                     float sx, float sy, float sw, float sh, float alpha) {
    if (!context) return;

    ID3D11ShaderResourceView* srv = getOrCreateSRV(tex);
    if (!srv) return;

    float tw = (float)tex.getWidth();
    float th = (float)tex.getHeight();
    float u0 = sx / tw, v0 = sy / th;
    float u1 = (sx + sw) / tw, v1 = (sy + sh) / th;

    setTexConstantBuffer(alpha);
    context->VSSetConstantBuffers(0, 1, &constantBuffer);
    context->PSSetConstantBuffers(0, 1, &constantBuffer);

    TexVertex verts[4] = {
        { x,     y,     0.0f, u0, v0 },
        { x + w, y,     0.0f, u1, v0 },
        { x + w, y + h, 0.0f, u1, v1 },
        { x,     y + h, 0.0f, u0, v1 }
    };

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(context->Map(texVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        memcpy(mapped.pData, verts, sizeof(verts));
        context->Unmap(texVB, 0);
    }

    UINT stride = sizeof(TexVertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &texVB, &stride, &offset);
    context->IASetInputLayout(texLayout);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLEFAN);

    context->VSSetShader(texVS, nullptr, 0);
    context->PSSetShader(texPS, nullptr, 0);

    context->PSSetShaderResources(0, 1, &srv);
    context->PSSetSamplers(0, 1, &samplerState);

    context->Draw(4, 0);
}

#endif
