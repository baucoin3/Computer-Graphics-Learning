#pragma once
#include <glad/glad.h>
#include <cstddef>
#include <utility>

// Expects interleaved pos+normal layout: vec3 pos, vec3 normal (6 floats per vertex).
// layout(location = 0) = position, layout(location = 1) = normal.
class Mesh {
public:
    Mesh(const float* verts, size_t vertexByteSize,const unsigned int* idx, size_t indexCount): indexCount(static_cast<GLsizei>(indexCount))
    {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertexByteSize), verts, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(indexCount * sizeof(unsigned int)),
                     idx, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);
    }

    ~Mesh() {
        if (VAO) glDeleteVertexArrays(1, &VAO);
        if (VBO) glDeleteBuffers(1, &VBO);
        if (EBO) glDeleteBuffers(1, &EBO);
    }

    Mesh(const Mesh&)            = delete;
    Mesh& operator=(const Mesh&) = delete;

    Mesh(Mesh&& o) noexcept
        : VAO(o.VAO), VBO(o.VBO), EBO(o.EBO), indexCount(o.indexCount)
    {
        o.VAO = o.VBO = o.EBO = 0;
    }

    Mesh& operator=(Mesh&& o) noexcept {
        if (this != &o) {
            if (VAO) glDeleteVertexArrays(1, &VAO);
            if (VBO) glDeleteBuffers(1, &VBO);
            if (EBO) glDeleteBuffers(1, &EBO);
            VAO = o.VAO; VBO = o.VBO; EBO = o.EBO; indexCount = o.indexCount;
            o.VAO = o.VBO = o.EBO = 0;
        }
        return *this;
    }

    void draw() const {
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

private:
    GLuint  VAO        = 0;
    GLuint  VBO        = 0;
    GLuint  EBO        = 0;
    GLsizei indexCount = 0;
};
