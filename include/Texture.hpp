#pragma once
#include <string>
#include <vector>

class Texture {
public:
    Texture();
    Texture(const std::string& filepath);
    Texture(int w, int h, const unsigned char* data, int channels = 4);
    ~Texture();
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    void bind(int slot = 0) const;
    void unbind() const;
    bool load(const std::string& filepath);
    bool fromData(int w, int h, const unsigned char* data, int channels = 4);

    int getWidth() const { return width; }
    int getHeight() const { return height; }
    unsigned int getID() const { return textureID; }
    const unsigned char* getPixels() const { return pixels.data(); }

    Texture overlay(const Texture& other, int x, int y, float alpha = 1.0f) const;

    static std::vector<Texture> loadAll(const std::string& directory);
    static bool isSupported(const std::string& filepath);

private:
    unsigned int textureID;
    int width, height, channels;
    std::vector<unsigned char> pixels;

    void createGLTexture();
    void release();
    bool loadFromPixels();
#ifdef _WIN32
    bool loadWithWIC(const std::string& filepath);
#endif
    bool loadBMP(const std::string& filepath);
};
