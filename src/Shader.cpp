#include "../include/Shader.hpp"
#include "../include/GLFunctions.hpp"

AuraShader::AuraShader(const std::string& vSrc, const std::string& fSrc) {
    const char* vPtr = vSrc.c_str();
    const char* fPtr = fSrc.c_str();

    unsigned int vShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vShader, 1, &vPtr, NULL);
    glCompileShader(vShader);

    unsigned int fShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fShader, 1, &fPtr, NULL);
    glCompileShader(fShader);

    programID = glCreateProgram();
    glAttachShader(programID, vShader);
    glAttachShader(programID, fShader);
    glLinkProgram(programID);
}

void AuraShader::use() { glUseProgram(programID); }