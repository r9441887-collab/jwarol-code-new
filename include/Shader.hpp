#pragma once
#include <string>

class AuraShader {
public:
    unsigned int programID;
    AuraShader(const std::string& vSrc, const std::string& fSrc);
    void use();
};