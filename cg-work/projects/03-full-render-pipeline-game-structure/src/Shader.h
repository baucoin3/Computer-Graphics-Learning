#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>

class Shader {
public:
    GLuint id;

    Shader(const char* vertPath, const char* fragPath) {
        std::string vertSrc = readFile(vertPath);
        std::string fragSrc = readFile(fragPath);
        const char* vCode   = vertSrc.c_str();
        const char* fCode   = fragSrc.c_str();

        GLuint vert = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vert, 1, &vCode, nullptr);
        glCompileShader(vert);
        checkCompile(vert, "Vertex");

        GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(frag, 1, &fCode, nullptr);
        glCompileShader(frag);
        checkCompile(frag, "Fragment");

        id = glCreateProgram();
        glAttachShader(id, vert);
        glAttachShader(id, frag);
        glLinkProgram(id);
        checkLink(id);

        glDeleteShader(vert);
        glDeleteShader(frag);
    }

    ~Shader() { glDeleteProgram(id); }

    void use() const { glUseProgram(id); }

    // Typed uniform setters — location is cached after first lookup.
    void setMat4 (const std::string& name, const glm::mat4& v) const { glUniformMatrix4fv(loc(name), 1, GL_FALSE, glm::value_ptr(v)); }
    void setMat3 (const std::string& name, const glm::mat3& v) const { glUniformMatrix3fv(loc(name), 1, GL_FALSE, glm::value_ptr(v)); }
    void setVec3 (const std::string& name, const glm::vec3& v) const { glUniform3fv(loc(name), 1, glm::value_ptr(v)); }
    void setFloat(const std::string& name, float v)            const { glUniform1f(loc(name), v); }
    void setInt  (const std::string& name, int v)              const { glUniform1i(loc(name), v); }

private:
    mutable std::unordered_map<std::string, GLint> uniformCache;

    GLint loc(const std::string& name) const {
        auto it = uniformCache.find(name);
        if (it != uniformCache.end()) return it->second;
        
        GLint l = glGetUniformLocation(id, name.c_str());
        uniformCache[name] = l;
        return l;
    }

    std::string readFile(const char* path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "Shader not found: " << path << "\n";
            return "";
        }
        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    void checkCompile(GLuint shader, const char* type) {
        GLint ok;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[1024];
            glGetShaderInfoLog(shader, 1024, nullptr, log);
            std::cerr << "Compile error [" << type << "]: " << log << "\n";
        }
    }

    void checkLink(GLuint program) {
        GLint ok;
        glGetProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[1024];
            glGetProgramInfoLog(program, 1024, nullptr, log);
            std::cerr << "Link error: " << log << "\n";
        }
    }
};
