#ifndef MCPE_RENDERER_SHADER_H
#define MCPE_RENDERER_SHADER_H

#include "gles.h"
#include <string>

class Shader {
    GLuint programId;
    GLuint compileShader(GLenum type, const std::string& source);
public:
    Shader();
    ~Shader();

    bool load(const std::string& vsSource, const std::string& fsSource);
    bool isLoaded() const { return programId != 0; }

    void bind();
    void unbind();

    GLuint getProgram() const { return programId; }

    GLint getUniformLocation(const std::string& name);
    GLint getAttribLocation(const std::string& name);

    void setUniformMatrix4(const std::string& name, const float* matrix);
    void setUniform1i(const std::string& name, int value);
    void setUniform1f(const std::string& name, float value);
    void setUniform4f(const std::string& name, float x, float y, float z, float w);
};

// 内置 default shader（GLES2 固定管线模拟用的顶点/片元着色器）
extern const char* kDefaultVertexShader;
extern const char* kDefaultFragmentShader;
Shader* createDefaultShader();

extern Shader* currentShader;

#endif