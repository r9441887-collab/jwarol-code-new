# jwarol

Движок на C++, который дает Python прямой доступ к видеокарте. 
**Win32 + OpenGL / DirectX11** на Windows, **X11 + Wayland + EGL** на Linux.

## Возможности

| Функция | Описание |
|---------|----------|
| **Window** | Создание окна с OpenGL-контекстом (Win32 / X11 / Wayland+EGL) |
| **Shader** | Компиляция GLSL-шейдеров из Python |
| **Renderer** | Рендеринг прямоугольников и текстур через GPU (OpenGL) |
| **D3D11Renderer** | Рендеринг через DirectX 11 (только Windows) |
| **Texture** | Загрузка PNG/JPG/BMP/GIF/TGA, процедурные текстуры, наложение |
| **Input** | Клавиатурный ввод (Win32 / X11 / Wayland) |
| **Physics** | Verlet-интеграция, гравитация, коллизии, типы тел |
| **Clock** | Замер дельта-тайма |
| **Vec2/Rect** | 2D-математика |

## Платформы

| | Windows | Linux |
|---|---------|-------|
| **Окнинг** | Win32 API | X11 (GLX) или Wayland (EGL) |
| **Рендеринг** | OpenGL 3.3 / DirectX 11 | OpenGL 3.3 |
| **Ввод** | GetAsyncKeyState | X11 / Wayland keyboard |
| **Текстуры** | WIC (PNG/JPG/BMP/GIF) | stb_image (PNG/JPG/BMP/GIF/TGA) |

Автоматический выбор Wayland vs X11 через переменную `WAYLAND_DISPLAY`.

## Установка

```bash
pip install jwarol2
```

### Сборка из исходников

```bash
# Linux (требуется: libgl-dev, libx11-dev, wayland-dev, libegl-dev, libxkbcommon-dev)
python setup.py build

# Windows (MSVC)
python setup.py build
```

## Быстрый старт — OpenGL

```python
import jwarol as jl

jl.init_graphics()
win = jl.Window(800, 600, "Hello jwarol")
renderer = jl.Renderer(800, 600)

while win.is_open:
    win.poll_events()
    win.clear(0.1, 0.1, 0.2)
    renderer.draw_rect(100, 100, 200, 150, 1.0, 0.3, 0.5)
    win.swap_buffers()
```

## DirectX 11 (Windows)

```python
import jwarol as jl

jl.init_graphics()
win = jl.Window(800, 600, "D3D11 Mode", use_d3d11=True)
renderer = jl.D3D11Renderer(800, 600, win)

while win.is_open:
    win.poll_events()
    renderer.clear(0.1, 0.1, 0.2)
    renderer.draw_rect(100, 100, 200, 150, 1.0, 0.3, 0.5)
    renderer.present()
```

## Текстуры

```python
# Загрузка из файла (PNG/JPG/BMP/GIF/TGA — все платформы)
tex = jl.Texture("sprite.png")

# Процедурная текстура из пикселей (RGBA)
import struct
w, h = 64, 64
pixels = bytearray()
for y in range(h):
    for x in range(w):
        pixels.extend([255, 0, 0, 255])
tex = jl.Texture(w, h, bytes(pixels), 4)

# Рисование с текстурой
renderer.draw_texture(tex, 100, 100, 200, 200)

# Умная загрузка всех текстур из папки
textures = jl.Texture.load_all("assets/textures/")
```

## Физика

```python
world = jl.World()
world.gravity = 980.0

p = world.add_point(400, 100)
body = world.add_body(300, 0, 50, 50, jl.BodyType.DYNAMIC)
static = world.add_body(0, 650, 800, 50, jl.BodyType.STATIC)

dt = clock.tick()
world.step(dt)
```

## Ввод и время

```python
input = jl.Input()
clock = jl.Clock()

while win.is_open:
    dt = clock.tick()
    input.update()
    if input.is_down(ord('W')): pass
    if input.is_pressed(ord(' ')): pass
    if input.is_released(ord('A')): pass
```

## Шейдеры

```python
shader = jl.Shader("""
#version 330 core
layout(location = 0) in vec2 aPos;
void main() { gl_Position = vec4(aPos, 0.0, 1.0); }
""", """
#version 330 core
uniform vec4 uColor;
out vec4 FragColor;
void main() { FragColor = uColor; }
""")
shader.use()
```

## Лицензия

MIT License — Руслан (r9441887@gmail.com)
