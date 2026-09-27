#pragma once
#include <glad/glad.h>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
class Shader {
    // TODO should there be try and catch or anything needed for file reading?
    public:
        GLuint id;

        Shader(const char *vertPath, const char *fragPath) {
            std::string vertSrc = readFile(vertPath);
            std::string fragSrc = readFile(fragPath);
            const char* vCode = vertSrc.c_str();
            const char* fCode = fragSrc.c_str();

            // Create shaders
            GLuint vert = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(vert, 1, &vCode, nullptr);
            glCompileShader(vert);
            checkCompile(vert, "Vertex");

            GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(frag, 1, &fCode, nullptr);
            glCompileShader(frag);
            checkCompile(frag, "FRAGMENT");

            // a program is a vertex and fragment shader together you have to link them so the shaders can use the same out and in variables so they match
            id = glCreateProgram();
            glAttachShader(id, vert);
            glAttachShader(id, frag);
            glLinkProgram(id);

            checkLink(id);


            // Once the shaders live inside the glCreateProgram you dont need the compiles shaders anymore so delete them
            glDeleteShader(vert);
            glDeleteShader(frag);

        }
        ~Shader() {
            glDeleteProgram(id);
        }

        void use() const {
            glUseProgram(id);
        }

    private:
        std::string readFile(const char* path) {
            std::ifstream file(path);
            if(!file.is_open()) {
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
            if(!ok) {
                char log[1024];
                glGetProgramInfoLog(program, 1024, nullptr, log);
                std::cerr << "link error: " << log << "\n";
            }
        }
};