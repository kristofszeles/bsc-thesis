#pragma once

#include "gl_core.h"

#include "gl_compat.h"
#include <SDL_image.h>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform2.hpp>
#include <sstream>
#include <string>
#include <vector>

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
};

struct Dimension {
    float width = 0, height = 0, depth = 0;
};

class Mesh {
private:
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;
    Dimension dimensions;
    GLuint texture;
    GLuint m_vaoID;
    GLuint m_vboID;
    GLuint m_ibID;
public:
    Mesh();
    ~Mesh();
    void init();
    void addVertex(Vertex vertex) { vertices.push_back(vertex); }
    void addIndex(GLuint index) { indices.push_back(index); }
    void setTexture(GLuint texture) { this->texture = texture; }
    void setWidth(float width) { dimensions.width = width; }
    void setHeight(float height) { dimensions.height = height; }
    void setDepth(float depth) { dimensions.depth = depth; }
    void loadOBJ(const std::string& path);
    std::vector<GLuint>& getIndices() { return indices; }
    GLuint getVaoID() const { return m_vaoID; }
    GLuint getTexture() const { return texture; }
    Dimension& getDimensions() { return dimensions; }
};