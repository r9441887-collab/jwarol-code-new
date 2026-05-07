#pragma once

class Texture;

class Renderer {
private:
    unsigned int rectVAO, rectVBO;
    unsigned int texVAO, texVBO;
    unsigned int texProgram;
    unsigned int rectProgram;
    float screenW, screenH;
public:
    Renderer(float width, float height);
    ~Renderer();
    void draw_rect(float x, float y, float w, float h, float r, float g, float b);
    void draw_texture(Texture& tex, float x, float y, float w, float h, float alpha = 1.0f);
    void draw_texture_ex(Texture& tex, float x, float y, float w, float h,
                         float sx, float sy, float sw, float sh, float alpha = 1.0f);
};
