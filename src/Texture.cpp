#include "../include/Texture.hpp"
#include "../include/GLFunctions.hpp"
#include <GL/gl.h>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <filesystem>

#ifdef _WIN32
#include <wincodec.h>
#include <objbase.h>
#else
#include "stb_image.h"
#endif

Texture::Texture() : textureID(0), width(0), height(0), channels(4) {}

Texture::Texture(const std::string& filepath) : textureID(0), width(0), height(0), channels(4) {
    load(filepath);
}

Texture::Texture(int w, int h, const unsigned char* data, int ch)
    : textureID(0), width(w), height(h), channels(ch) {
    if (data) {
        pixels.assign(data, data + w * h * ch);
        createGLTexture();
    }
}

Texture::~Texture() { release(); }

Texture::Texture(Texture&& other) noexcept
    : textureID(other.textureID), width(other.width), height(other.height),
      channels(other.channels), pixels(std::move(other.pixels)) {
    other.textureID = 0;
    other.width = other.height = other.channels = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        release();
        textureID = other.textureID;
        width = other.width;
        height = other.height;
        channels = other.channels;
        pixels = std::move(other.pixels);
        other.textureID = 0;
        other.width = other.height = other.channels = 0;
    }
    return *this;
}

void Texture::release() {
    if (textureID) {
        glDeleteTextures(1, &textureID);
        textureID = 0;
    }
    pixels.clear();
    width = height = channels = 0;
}

void Texture::createGLTexture() {
    if (textureID) glDeleteTextures(1, &textureID);
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    GLenum fmt = (channels == 4) ? GL_RGBA : (channels == 3) ? GL_RGB : GL_ALPHA;
    glTexImage2D(GL_TEXTURE_2D, 0, (int)fmt, width, height, 0, fmt, GL_UNSIGNED_BYTE, pixels.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D);
}

bool Texture::loadFromPixels() {
    if (pixels.empty() || width <= 0 || height <= 0) return false;
    createGLTexture();
    return true;
}

void Texture::bind(int slot) const {
    if (textureID) {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, textureID);
    }
}

void Texture::unbind() const {
    glBindTexture(GL_TEXTURE_2D, 0);
}

bool Texture::load(const std::string& filepath) {
    release();
    auto ext = std::filesystem::path(filepath).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".bmp") return loadBMP(filepath);
#ifdef _WIN32
    return loadWithWIC(filepath);
#else
    return loadWithStb(filepath);
#endif
}

bool Texture::fromData(int w, int h, const unsigned char* data, int ch) {
    release();
    width = w;
    height = h;
    channels = ch;
    if (data) pixels.assign(data, data + w * h * ch);
    return createGLTexture(), true;
}

bool Texture::loadBMP(const std::string& filepath) {
    std::ifstream f(filepath, std::ios::binary);
    if (!f) return false;

    unsigned char header[54];
    f.read((char*)header, 54);
    if (header[0] != 'B' || header[1] != 'M') return false;

    width = *(int*)&header[18];
    height = *(int*)&header[22];
    int bpp = *(short*)&header[28];
    channels = (bpp == 32) ? 4 : (bpp == 24) ? 3 : 0;
    if (!channels) return false;

    int rowSize = ((width * bpp + 31) / 32) * 4;
    std::vector<unsigned char> row(rowSize);
    pixels.resize(width * height * channels);

    for (int y = 0; y < height; y++) {
        f.read((char*)row.data(), rowSize);
        for (int x = 0; x < width; x++) {
            int srcIdx = x * (bpp / 8);
            int dstIdx = (y * width + x) * channels;
            pixels[dstIdx + 2] = row[srcIdx + 0];
            pixels[dstIdx + 1] = row[srcIdx + 1];
            pixels[dstIdx + 0] = row[srcIdx + 2];
            if (channels == 4) pixels[dstIdx + 3] = row[srcIdx + 3];
        }
    }
    return createGLTexture(), true;
}

#ifdef _WIN32
bool Texture::loadWithWIC(const std::string& filepath) {
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    IWICImagingFactory* factory = NULL;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                  IID_IWICImagingFactory, (void**)&factory);
    if (FAILED(hr) || !factory) return loadBMP(filepath);

    IWICBitmapDecoder* decoder = NULL;
    hr = factory->CreateDecoderFromFilename(
        std::wstring(filepath.begin(), filepath.end()).c_str(),
        NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);

    IWICBitmapFrameDecode* frame = NULL;
    if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, &frame);

    if (SUCCEEDED(hr)) {
        frame->GetSize((UINT*)&width, (UINT*)&height);
        channels = 4;
        pixels.resize(width * height * 4);

        WICPixelFormatGUID pf;
        frame->GetPixelFormat(&pf);

        IWICFormatConverter* converter = NULL;
        factory->CreateFormatConverter(&converter);
        if (converter) {
            converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA,
                                  WICBitmapDitherTypeNone, NULL, 0, WICBitmapPaletteTypeCustom);
            converter->CopyPixels(NULL, width * 4, width * height * 4, pixels.data());
            converter->Release();
        }
        frame->Release();
    }

    if (decoder) decoder->Release();
    factory->Release();
    CoUninitialize();

    if (pixels.empty()) return loadBMP(filepath);
    return createGLTexture(), true;
}
#else
bool Texture::loadWithStb(const std::string& filepath) {
    int w, h, ch;
    stbi_set_flip_vertically_on_load(0);
    unsigned char* data = stbi_load(filepath.c_str(), &w, &h, &ch, 4);
    if (!data) return false;

    width = w;
    height = h;
    channels = 4;
    pixels.assign(data, data + w * h * 4);
    stbi_image_free(data);

    return createGLTexture(), true;
}
#endif

Texture Texture::overlay(const Texture& other, int x, int y, float alpha) const {
    Texture result(width, height, nullptr, 4);
    result.pixels = pixels;

    for (int oy = 0; oy < other.height; oy++) {
        for (int ox = 0; ox < other.width; ox++) {
            int dx = x + ox;
            int dy = y + oy;
            if (dx < 0 || dx >= width || dy < 0 || dy >= height) continue;

            int srcIdx = (oy * other.width + ox) * other.channels;
            int dstIdx = (dy * width + dx) * 4;

            float sa = (other.channels >= 4 ? other.pixels[srcIdx + 3] : 255) / 255.0f * alpha;
            float da = 1.0f - sa;

            result.pixels[dstIdx + 0] = (unsigned char)(other.pixels[srcIdx + 0] * sa + result.pixels[dstIdx + 0] * da);
            result.pixels[dstIdx + 1] = (unsigned char)(other.pixels[srcIdx + 1] * sa + result.pixels[dstIdx + 1] * da);
            result.pixels[dstIdx + 2] = (unsigned char)(other.pixels[srcIdx + 2] * sa + result.pixels[dstIdx + 2] * da);
            float dstA = result.pixels[dstIdx + 3] / 255.0f;
            result.pixels[dstIdx + 3] = (unsigned char)((sa + dstA * da) * 255.0f);
        }
    }
    result.createGLTexture();
    return result;
}

std::vector<Texture> Texture::loadAll(const std::string& directory) {
    std::vector<Texture> textures;
    if (!std::filesystem::exists(directory)) return textures;

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            Texture tex(entry.path().string());
            if (tex.getID()) textures.push_back(std::move(tex));
        }
    }
    return textures;
}

bool Texture::isSupported(const std::string& filepath) {
    auto ext = std::filesystem::path(filepath).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".bmp" || ext == ".png" || ext == ".jpg" || ext == ".jpeg"
        || ext == ".gif" || ext == ".tga" || ext == ".psd";
}
