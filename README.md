# jwarol

Движок на C++, который дает Python прямой доступ к видеокарте. Win32 API + сырой OpenGL (и X11 на Linux).

## Возможности

| Функция | Описание |
|---------|----------|
| **Window** | Создание окна с OpenGL-контекстом (Win32 / X11) |
| **Shader** | Компиляция GLSL-шейдеров из Python |
| **Renderer** | Рендеринг прямоугольников и текстур через GPU |
| **Texture** | Загрузка PNG/JPG/BMP, процедурные текстуры, наложение |
| **Input** | Клавиатурный ввод (GetAsyncKeyState) |
| **Physics** | Verlet-интеграция, гравитация, коллизии, типы тел |
| **Clock** | Замер дельта-тайма |
| **Vec2/Rect** | 2D-математика |

## Установка

```bash
pip install jwarol
```

## Быстрый старт

```python
import jwarol as jl

jl.init_graphics()
win = jl.Window(800, 600, "Hello jwarol")
renderer = jl.Renderer(800, 600)

while win.is_open:
    win.poll_events()
    win.clear(0.1, 0.1, 0.2)

    renderer.draw_rect(100, 100, 200, 150, 1.0, 0.3, 0.5)
    renderer.draw_rect(350, 200, 100, 100, 0.2, 0.7, 1.0)

    win.swap_buffers()
```

## Текстуры

```python
# Загрузка из файла (PNG/JPG/BMP на Windows, BMP на Linux)
tex = jl.Texture("sprite.png")

# Процедурная текстура из пикселей (RGBA)
import struct
w, h = 64, 64
pixels = bytearray()
for y in range(h):
    for x in range(w):
        pixels.extend([255, 0, 0, 255])  # красный пиксель
tex = jl.Texture(w, h, bytes(pixels), 4)

# Рисование с текстурой
renderer.draw_texture(tex, 100, 100, 200, 200)

# Текстура с прозрачностью
renderer.draw_texture(tex, 100, 100, 200, 200, alpha=0.5)

# Спрайт из tileset'а (draw_texture_ex)
renderer.draw_texture_ex(tileset, 0, 0, 32, 32, 0, 0, 32, 32)

# Умная загрузка всех текстур из папки
textures = jl.Texture.load_all("assets/textures/")

# Наложение текстур
combined = tex.overlay(other_tex, 10, 10, alpha=0.7)
```

## Физика

```python
world = jl.World()
world.gravity = 980.0

# Verlet-точки
p = world.add_point(400, 100)  # падает вниз

# Физические тела (прямоугольники)
body = world.add_body(300, 0, 50, 50, jl.BodyType.DYNAMIC)
static = world.add_body(0, 650, 800, 50, jl.BodyType.STATIC)

# Исключение из физики
body.enabled = False  # не участвует в симуляции

# Шаг физики
dt = clock.tick()
world.step(dt)

# Чтение позиции
x, y = body.pos.x, body.pos.y
```

## Ввод и время

```python
input = jl.Input()
clock = jl.Clock()

while win.is_open:
    dt = clock.tick()
    input.update()

    if input.is_down(ord('W')): pass      # клавиша зажата
    if input.is_pressed(ord(' ')): pass   # только что нажали
    if input.is_released(ord('A')): pass  # только что отпустили
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

## Сборка из исходников

```bash
# Windows (MSVC)
python setup.py build

# Linux (GCC)
python setup.py build
```

## Лицензия

MIT License — Руслан (r9441887@gmail.com)
