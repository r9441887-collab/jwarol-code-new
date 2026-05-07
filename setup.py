import os, sys
from setuptools import setup
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
]

libraries = []
if sys.platform == "win32":
    libraries = ["user32", "gdi32", "opengl32", "ole32", "windowscodecs"]
else:
    libraries = ["GL", "X11"]

ext_modules = [
    Pybind11Extension(
        "jwarol",
        sources=sources,
        include_dirs=["include"],
        libraries=libraries,
        extra_compile_args=["/O2", "/EHsc", "/std:c++17"] if sys.platform == "win32" else ["-O2", "-std=c++17"],
    ),
]

setup(
    name="jwarol",
    version="0.6.0",
    author="jwarol-team",
    author_email="r9441887@gmail.com",
    url="https://github.com/r9441887-collab/jwarol-code",
    description="High-performance creative engine (Win32/OpenGL/Python)",
    long_description=jwarol_description,
    long_description_content_type="text/markdown",
    ext_modules=ext_modules,
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
