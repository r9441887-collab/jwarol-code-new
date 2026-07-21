#pragma once

#ifdef _WIN32
#include <windows.h>
#else
#define GL_GLEXT_PROTOTYPES
#endif

#include <GL/gl.h>

#ifdef _WIN32

#define JW_CALL WINAPI

typedef unsigned int (JW_CALL *PFNGLCREATESHADERPROC)(unsigned int type);
typedef void (JW_CALL *PFNGLSHADERSOURCEPROC)(unsigned int shader, int count, const char* const* string, const int* length);
typedef void (JW_CALL *PFNGLCOMPILESHADERPROC)(unsigned int shader);
typedef unsigned int (JW_CALL *PFNGLCREATEPROGRAMPROC)(void);
typedef void (JW_CALL *PFNGLATTACHSHADERPROC)(unsigned int program, unsigned int shader);
typedef void (JW_CALL *PFNGLLINKPROGRAMPROC)(unsigned int program);
typedef void (JW_CALL *PFNGLUSEPROGRAMPROC)(unsigned int program);
typedef void (JW_CALL *PFNGLGETSHADERIVPROC)(unsigned int shader, unsigned int pname, int* params);
typedef void (JW_CALL *PFNGLGETSHADERINFOLOGPROC)(unsigned int shader, int bufSize, int* length, char* infoLog);
typedef void (JW_CALL *PFNGLGENBUFFERSPROC)(int n, unsigned int *buffers);
typedef void (JW_CALL *PFNGLBINDBUFFERPROC)(unsigned int target, unsigned int buffer);
typedef void (JW_CALL *PFNGLBUFFERDATAPROC)(unsigned int target, ptrdiff_t size, const void *data, unsigned int usage);
typedef void (JW_CALL *PFNGLGENVERTEXARRAYSPROC)(int n, unsigned int *arrays);
typedef void (JW_CALL *PFNGLBINDVERTEXARRAYPROC)(unsigned int array);
typedef void (JW_CALL *PFNGLENABLEVERTEXATTRIBARRAYPROC)(unsigned int index);
typedef void (JW_CALL *PFNGLVERTEXATTRIBPOINTERPROC)(unsigned int index, int size, unsigned int type, bool normalized, int stride, const void *pointer);
typedef int  (JW_CALL *PFNGLGETUNIFORMLOCATIONPROC)(unsigned int program, const char *name);
typedef void (JW_CALL *PFNGLUNIFORM4FPROC)(int location, float v0, float v1, float v2, float v3);
typedef void (JW_CALL *PFNGLUNIFORM1FPROC)(int location, float v0);
typedef void (JW_CALL *PFNGLUNIFORM1IPROC)(int location, int v0);
typedef void (JW_CALL *PFNGLUNIFORMMATRIX4FVPROC)(int location, int count, bool transpose, const float *value);
typedef void (JW_CALL *PFNGLDETACHSHADERPROC)(unsigned int program, unsigned int shader);
typedef void (JW_CALL *PFNGLDELETESHADERPROC)(unsigned int shader);
typedef void (JW_CALL *PFNGLDELETEPROGRAMPROC)(unsigned int program);
typedef void (JW_CALL *PFNGLDELETEBUFFERSPROC)(int n, unsigned int *buffers);
typedef void (JW_CALL *PFNGLDELETEVERTEXARRAYSPROC)(int n, unsigned int *arrays);
typedef void (JW_CALL *PFNGLACTIVETEXTUREPROC)(unsigned int tex);
typedef void (JW_CALL *PFNGLGENERATEMIPMAPPROC)(unsigned int target);

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
extern PFNGLDETACHSHADERPROC glDetachShader;
extern PFNGLDELETESHADERPROC glDeleteShader;
extern PFNGLDELETEPROGRAMPROC glDeleteProgram;
extern PFNGLDELETEBUFFERSPROC glDeleteBuffers;
extern PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;
extern PFNGLACTIVETEXTUREPROC glActiveTexture;
extern PFNGLGENERATEMIPMAPPROC glGenerateMipmap;

#endif

void LoadGLFunctions();
