#include "Shader.h"
#include "../../platform/log.h"

Shader* currentShader = nullptr;

const char* kDefaultVertexShader = R"(
attribute vec3 a_position;
attribute vec2 a_texCoord;
attribute vec4 a_color;

uniform mat4 u_modelView;
uniform mat4 u_projection;

varying vec2 v_texCoord;
varying vec4 v_color;

void main() {
    v_texCoord = a_texCoord;
    v_color = a_color;
    gl_Position = u_projection * u_modelView * vec4(a_position, 1.0);
}
)";

const char* kDefaultFragmentShader = R"(
precision mediump float;
varying vec2 v_texCoord;
varying vec4 v_color;

uniform sampler2D u_texture;
uniform vec4 u_color;
uniform bool u_useTexture;
uniform bool u_alphaTest;
uniform float u_alphaRef;

void main() {
    vec4 texColor = vec4(1.0);
    if (u_useTexture) {
        texColor = texture2D(u_texture, v_texCoord);
    }

    // GLES1 语义：启用颜色数组时，逐顶点颜色取代 glColor；
    // 未启用时，使用当前颜色。二者只能生效一个，不能相乘两次
    // （由 CPU 侧在启用颜色数组时把 u_color 置为白色来保证）。
    vec4 finalColor = texColor * v_color * u_color;

    if (u_alphaTest && finalColor.a < u_alphaRef) {
        discard;
    }

    gl_FragColor = finalColor;
}
)";

Shader::Shader()
    : programId(0)
{
}

Shader::~Shader()
{
    if (programId) {
        glDeleteProgram(programId);
        programId = 0;
    }
}

GLuint Shader::compileShader(GLenum type, const std::string& source)
{
    GLuint shader = glCreateShader(type);
    if (shader == 0) return 0;

    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        LOGE("Shader compilation failed: %s", infoLog);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool Shader::load(const std::string& vsSource, const std::string& fsSource)
{
    if (programId) {
        glDeleteProgram(programId);
        programId = 0;
    }

    GLuint vertex = compileShader(GL_VERTEX_SHADER, vsSource);
    GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fsSource);
    if (vertex == 0 || fragment == 0) {
        if (vertex) glDeleteShader(vertex);
        if (fragment) glDeleteShader(fragment);
        return false;
    }

    programId = glCreateProgram();
    glAttachShader(programId, vertex);
    glAttachShader(programId, fragment);
    glLinkProgram(programId);

    GLint linked = 0;
    glGetProgramiv(programId, GL_LINK_STATUS, &linked);
    if (!linked) {
        char infoLog[512];
        glGetProgramInfoLog(programId, 512, nullptr, infoLog);
        LOGE("Shader linking failed: %s", infoLog);
        glDeleteProgram(programId);
        programId = 0;
    }

    glDeleteShader(vertex);
    glDeleteShader(fragment);
    return programId != 0;
}

void Shader::bind()
{
    glUseProgram(programId);
    currentShader = this;
}

void Shader::unbind()
{
    glUseProgram(0);
    currentShader = nullptr;
}

GLint Shader::getUniformLocation(const std::string& name)
{
    if (!programId) return -1;
    return glGetUniformLocation(programId, name.c_str());
}

GLint Shader::getAttribLocation(const std::string& name)
{
    if (!programId) return -1;
    return glGetAttribLocation(programId, name.c_str());
}

void Shader::setUniformMatrix4(const std::string& name, const float* matrix)
{
    if (!programId) return;
    GLint loc = getUniformLocation(name);
    if (loc != -1) glUniformMatrix4fv(loc, 1, GL_FALSE, matrix);
}

void Shader::setUniform1i(const std::string& name, int value)
{
    if (!programId) return;
    GLint loc = getUniformLocation(name);
    if (loc != -1) glUniform1i(loc, value);
}

void Shader::setUniform1f(const std::string& name, float value)
{
    if (!programId) return;
    GLint loc = getUniformLocation(name);
    if (loc != -1) glUniform1f(loc, value);
}

void Shader::setUniform4f(const std::string& name, float x, float y, float z, float w)
{
    if (!programId) return;
    GLint loc = getUniformLocation(name);
    if (loc != -1) glUniform4f(loc, x, y, z, w);
}

Shader* createDefaultShader()
{
    Shader* shader = new Shader();
    if (!shader->load(kDefaultVertexShader, kDefaultFragmentShader)) {
        delete shader;
        return nullptr;
    }
    return shader;
}