import os, sys, subprocess
from setuptools import setup, Extension
from pybind11.setup_helpers import Pybind11Extension, build_ext

with open("README.md", "r", encoding="utf-8") as f:
    jwarol_description = f.read()

sources = [
    "src/Window.cpp",
    "src/Shader.cpp",
    "src/GLFunctions.cpp",
    "src/Bindings.cpp",
    "src/Input.cpp",
    "src/Physics.cpp",
    "src/Renderer.cpp",
    "src/Texture.cpp",
    "src/stb_image_impl.cpp",
    "src/Mesh3D.cpp",
    "src/Camera3D.cpp",
    "src/Renderer3D.cpp",
    "src/VulkanRenderer3D.cpp",
]

if sys.platform == "win32":
    sources.append("src/D3D11Renderer.cpp")
    sources.append("src/D3D11Renderer3D.cpp")

libraries = []
extra_compile_args = []
extra_link_args = []

if sys.platform == "win32":
    libraries = ["user32", "gdi32", "opengl32", "ole32", "windowscodecs",
                 "d3d11", "dxgi", "d3dcompiler"]
    extra_compile_args = ["/O2", "/EHsc", "/std:c++17"]
else:
    libraries = ["GL", "X11", "dl", "vulkan"]
    extra_compile_args = ["-O2", "-std=c++17"]

    # Try to find wayland-egl, wayland-client, EGL, xkbcommon
    def try_pkg_config(name):
        try:
            out = subprocess.check_output(
                ["pkg-config", "--libs", "--cflags", name],
                stderr=subprocess.DEVNULL).decode().strip()
            return out.split()
        except Exception:
            return []

    for pkg in ["wayland-client", "wayland-egl", "egl", "xkbcommon"]:
        flags = try_pkg_config(pkg)
        for f in flags:
            if f.startswith("-l"):
                lib = f[2:]
                if lib not in libraries:
                    libraries.append(lib)
            elif f.startswith("-I"):
                extra_compile_args.append(f)
            elif f.startswith("-L"):
                extra_link_args.append(f)

ext_modules = [
    Pybind11Extension(
        "jwarol2",
        sources=sources,
        include_dirs=["include"],
        libraries=libraries,
        extra_compile_args=extra_compile_args,
        extra_link_args=extra_link_args,
    ),
]

setup(
    name="jwarol2",
    version="1.1.0",
    author="jwarol-team",
    author_email="r9441887@gmail.com",
    url="https://github.com/r9441887-collab/jwarol-code-new",
    description="High-performance creative engine (OpenGL/DirectX11, Win32/Wayland/X11)",
    long_description=jwarol_description,
    long_description_content_type="text/markdown",
    ext_modules=ext_modules,
    py_modules=["jwarol_compiler"],
    entry_points={
        "console_scripts": [
            "jwarol=jwarol_compiler:main",
        ],
    },
    cmdclass={"build_ext": build_ext},
    zip_safe=False,
    python_requires=">=3.8",
    classifiers=[
        "Programming Language :: Python :: 3",
        "Programming Language :: C++",
        "License :: OSI Approved :: MIT License",
        "Operating System :: Microsoft :: Windows",
        "Operating System :: POSIX :: Linux",
    ],
)
