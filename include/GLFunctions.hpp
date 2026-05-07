#pragma once
#include <windows.h>
#include <GL/gl.h>
#include <cstddef>

typedef unsigned int(WINAPI* PFNGLCREATESHADERPROC)(unsigned int type);
typedef void(WINAPI* PFNGLSHADERSOURCEPROC)(unsigned int shader, int count, const char* const* string, const int* length);
typedef void(WINAPI* PFNGLCOMPILESHADERPROC)(unsigned int shader);
typedef unsigned int(WINAPI* PFNGLCREATEPROGRAMPROC)(void);
typedef void(WINAPI* PFNGLATTACHSHADERPROC)(unsigned int program, unsigned int shader);
typedef void(WINAPI* PFNGLLINKPROGRAMPROC)(unsigned int program);
typedef void(WINAPI* PFNGLUSEPROGRAMPROC)(unsigned int program);
typedef void(WINAPI* PFNGLGETSHADERIVPROC)(unsigned int shader, unsigned int pname, int* params);
typedef void(WINAPI* PFNGLGETSHADERINFOLOGPROC)(unsigned int shader, int bufSize, int* length, char* infoLog);

// Функции для буферов
typedef void (WINAPI *PFNGLGENBUFFERSPROC) (int n, unsigned int *buffers);
typedef void (WINAPI *PFNGLBINDBUFFERPROC) (unsigned int target, unsigned int buffer);
typedef void (WINAPI *PFNGLBUFFERDATAPROC) (unsigned int target, ptrdiff_t size, const void *data, unsigned int usage);
typedef void (WINAPI *PFNGLGENVERTEXARRAYSPROC) (int n, unsigned int *arrays);
typedef void (WINAPI *PFNGLBINDVERTEXARRAYPROC) (unsigned int array);
typedef void (WINAPI *PFNGLENABLEVERTEXATTRIBARRAYPROC) (unsigned int index);
typedef void (WINAPI *PFNGLVERTEXATTRIBPOINTERPROC) (unsigned int index, int size, unsigned int type, bool normalized, int stride, const void *pointer);
typedef int (WINAPI *PFNGLGETUNIFORMLOCATIONPROC) (unsigned int program, const char *name);
typedef void (WINAPI *PFNGLUNIFORM4FPROC) (int location, float v0, float v1, float v2, float v3);
typedef void (WINAPI *PFNGLUNIFORM1FPROC) (int location, float v0);
typedef void (WINAPI *PFNGLUNIFORM1IPROC) (int location, int v0);
typedef void (WINAPI *PFNGLUNIFORMMATRIX4FVPROC) (int location, int count, bool transpose, const float *value);
typedef void (WINAPI *PFNGLDELETEPROGRAMPROC) (unsigned int program);
typedef void (WINAPI *PFNGLDELETEBUFFERSPROC) (int n, unsigned int *buffers);
typedef void (WINAPI *PFNGLDELETEVERTEXARRAYSPROC) (int n, unsigned int *arrays);

#define GL_ARRAY_BUFFER 0x8892
#define GL_STATIC_DRAW  0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_COMPILE_STATUS 0x8B81
#define GL_FLOAT 0x1406
#define GL_TRIANGLE_FAN 0x0006
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30

extern PFNGLCREATESHADERPROC glCreateShader;
extern PFNGLSHADERSOURCEPROC glShaderSource;
extern PFNGLCOMPILESHADERPROC glCompileShader;
extern PFNGLCREATEPROGRAMPROC glCreateProgram;
extern PFNGLATTACHSHADERPROC glAttachShader;
extern PFNGLLINKPROGRAMPROC glLinkProgram;
extern PFNGLUSEPROGRAMPROC glUseProgram;
extern PFNGLGETSHADERIVPROC glGetShaderiv;
extern PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;

extern PFNGLGENBUFFERSPROC glGenBuffers;
extern PFNGLBINDBUFFERPROC glBindBuffer;
extern PFNGLBUFFERDATAPROC glBufferData;
extern PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
extern PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
extern PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
extern PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
extern PFNGLUNIFORM4FPROC glUniform4f;
extern PFNGLUNIFORM1FPROC glUniform1f;
extern PFNGLUNIFORM1IPROC glUniform1i;
extern PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;
extern PFNGLDELETEPROGRAMPROC glDeleteProgram;
extern PFNGLDELETEBUFFERSPROC glDeleteBuffers;
extern PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;

// Текстурные функции
typedef void (WINAPI *PFNGLACTIVETEXTUREPROC) (unsigned int tex);
typedef void (WINAPI *PFNGLGENERATEMIPMAPPROC) (unsigned int target);

extern PFNGLACTIVETEXTUREPROC glActiveTexture;
extern PFNGLGENERATEMIPMAPPROC glGenerateMipmap;

#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE0 0x84C0
#define GL_RGBA 0x1908
#define GL_RGB 0x1907
#define GL_ALPHA 0x1906
#define GL_UNSIGNED_BYTE 0x1401
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_REPEAT 0x2901
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_BLEND 0x0BE2
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303

void LoadGLFunctions();