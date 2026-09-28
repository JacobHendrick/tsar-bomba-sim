#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
inline std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("cannot open " + path);
    std::stringstream s;
    s << f.rdbuf();
    return s.str();
}
struct Prog {
    GLuint id = 0;
    GLint loc(const char* n) const { return glGetUniformLocation(id, n); }
    void use() const { glUseProgram(id); }
    void set(const char* n, int v) const { glProgramUniform1i(id, loc(n), v); }
    void set(const char* n, float v) const { glProgramUniform1f(id, loc(n), v); }
    void set(const char* n, glm::vec3 v) const { glProgramUniform3fv(id, loc(n), 1, &v[0]); }
    void set(const char* n, glm::vec4 v) const { glProgramUniform4fv(id, loc(n), 1, &v[0]); }
    void set (const char* n, glm::ivec3 v) const { glProgramUniform3iv(id, loc(n), 1, &v[0]); }
    void set(const char* n, const glm::mat4& m) const { glProgramUniformMatrix4fv(id, loc(n), 1, GL_FALSE, &m[0][0]); }
};

inline Prog makeProgram(std::vector<std::string> files) {
    const std::string dir = SHADER_DIR "/", common = readFile(dir + "common.glsl");
    Prog p{glCreateProgram()};
    std::vector<GLuint> shaders;
    try {
    for (auto& f : files) {
        auto ext = f.substr(f.rfind('.'));
        GLenum type = ext == ".comp" ? GL_COMPUTE_SHADER : ext == ".vert" ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER;
        std::string src = "#version 430 core\n" + std::string(ext == ".comp" ? "#define COMPUTE\n" : "") + common +
                          "\n#line 1 1\n" + readFile(dir + f);
        const char* s = src.c_str();
        GLuint sh = glCreateShader(type);
        shaders.push_back(sh);
        glShaderSource(sh, 1, &s, nullptr);
        glCompileShader(sh);
        GLint ok;
        char log[8192];
        glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
        if (!ok) { glGetShaderInfoLog(sh, sizeof log, nullptr, log); throw std:: runtime_error(f + ":\n" + log); }
        glAttachShader(p.id, sh);
    }
    glLinkProgram(p.id);
    GLint ok;
    glGetProgramiv(p.id, GL_LINK_STATUS, &ok);
    if (!ok) { char log[8192]; glGetProgramInfoLog(p.id, sizeof log, nullptr, log); throw std::runtime_error(files[0] + " link:\n" + log); }
    } catch (...) {
        for (GLuint sh : shaders) glDeleteShader(sh);
        glDeleteProgram(p.id);
        throw;
    }
    for (GLuint sh : shaders) { glDetachShader(p.id, sh); glDeleteShader(sh); }
    return p;
}
inline int mipLevels(int n) { int l = 1; while (n >>= 1) ++l; return l; }
inline GLuint makeTex(GLenum target, GLenum fmt, int w, int h = 1, int d = 1, int levels = 1) {
    GLuint t;
    glGenTextures(1,&t);
    glBindTexture(target, t);
    if (target == GL_TEXTURE_3D) glTexStorage3D(target, levels, fmt, w, h, d);
    else if (target == GL_TEXTURE_2D) glTexStorage2D(target, levels, fmt, w, h);
    else glTexStorage1D(target, levels, fmt, w);
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, target == GL_TEXTURE_3D && levels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    for (GLenum wrap : {GL_TEXTURE_WRAP_S, GL_TEXTURE_WRAP_T, GL_TEXTURE_WRAP_R}) glTexParameteri(target, wrap, GL_CLAMP_TO_EDGE);
    if (GLenum error = glGetError(); error != GL_NO_ERROR) {
        glDeleteTextures(1, &t);
        throw std::runtime_error("texture allocation failed (OpenGL error " + std::to_string(error) + ")");
    }
    return t;
}
inline void zero3D(GLuint t, glm::ivec3 n, int channels) {
    std::vector<float> z(size_t(n.x) * n.y * n.z * channels, 0.f);
    glBindTexture(GL_TEXTURE_3D, t);
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, n.x, n.y, n.z, channels == 1 ? GL_RED : GL_RGBA, GL_FLOAT, z.data());
}
inline void bindTex(int unit, GLenum target, GLuint t) { glActiveTexture(GL_TEXTURE0 + unit); glBindTexture(target, t); }
inline void bindImg(int unit, GLuint t, GLenum fmt) { glBindImageTexture(unit, t, 0, GL_TRUE, 0, GL_WRITE_ONLY, fmt); }
inline void dispatch(glm::ivec3 n) {
    glDispatchCompute((n.x + 7) / 8, (n.y + 7) / 8, (n.z + 7) / 8);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
}
