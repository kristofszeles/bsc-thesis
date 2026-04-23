#include "mesh.h"
#include "drawutils.h"

#include <SDL.h>
#include <cstdlib>

Mesh::Mesh() {
    m_vaoID = 0;
    m_vboID = 0;
    m_ibID = 0;
    texture = 0;
}

Mesh::~Mesh() {
    glDeleteBuffers(1, &m_vboID);
    glDeleteBuffers(1, &m_ibID);
    glCompatDeleteVertexArrays(1, &m_vaoID);
}

void Mesh::init() {
    glCompatGenVertexArrays(1, &m_vaoID);
    glGenBuffers(1, &m_vboID);
    glGenBuffers(1, &m_ibID);

    glCompatBindVertexArray(m_vaoID);
    glBindBuffer(GL_ARRAY_BUFFER, m_vboID);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(glm::vec3)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(2 * sizeof(glm::vec3)));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibID);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * indices.size(), indices.data(), GL_STATIC_DRAW);

    glCompatBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void Mesh::loadOBJ(const std::string& path) {
    size_t datasize = 0;
    void* raw = SDL_LoadFile(path.c_str(), &datasize);
    if (!raw) {
        const char* err = SDL_GetError();
        std::string msg = path;
        if (err && err[0]) {
            msg += "\n";
            msg += err;
        }
        SDL_Log("Mesh::loadOBJ: could not open %s: %s", path.c_str(), err ? err : "");
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "loadOBJ", msg.c_str(), nullptr);
        exit(1);
    }
    const char* buf = static_cast<const char*>(raw);
    std::string fileContent(buf, buf + datasize);
    SDL_free(raw);

    std::istringstream file(fileContent);
    std::string line;
    int indexedVerts = 0;
    float maxX = 0, minX = 0, maxY = 0, minY = 0, maxZ = 0, minZ = 0;
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texCoords;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        std::istringstream ss;
        std::string type;
        ss.str(line);
        ss >> type;
        if (type == "v") {
            float x = 0, y = 0, z = 0;
            ss >> x >> y >> z;
            positions.push_back(glm::vec3(x, y, z));
            if (x > maxX) maxX = x;
            if (x < minX) minX = x;
            if (y > maxY) maxY = y;
            if (y < minY) minY = y;
            if (z > maxZ) maxZ = z;
            if (z < minZ) minZ = z;
        } else if (type == "vt") {
            float u = 0, v = 0;
            ss >> u >> v;
            texCoords.push_back(glm::vec2(u, v));
        } else if (type == "vn") {
            float x = 0, y = 0, z = 0;
            ss >> x >> y >> z;
            normals.push_back(glm::vec3(x, y, z));
        } else if (type == "f") {
            while (!ss.eof()) {
                std::stringstream params;
                std::string value;
                ss >> value;
                params << value;
                GLuint vertexID = 0, texCoordID = 0, normalID = 0;
                getline(params, value, '/');
                if (!value.empty()) vertexID = stoi(value);
                getline(params, value, '/');
                if (!value.empty()) texCoordID = stoi(value);
                params >> value;
                if (!value.empty()) normalID = stoi(value);
                if (vertexID || texCoordID || normalID) {
                    Vertex v;
                    v.position = positions[vertexID-1];
                    v.normal = normals[normalID-1];
                    v.texCoord = texCoords[texCoordID-1];
                    addVertex(v);
                    addIndex(indexedVerts);
                    ++indexedVerts;
                }
            }
        } else if (type == "texture") {
            std::string fileName;
            ss >> fileName;
            {
                Texture* tex = createTextureFromImage(fileName);
                setTexture(tex->release());
                delete tex;
            }
        }
    }
    setWidth(std::abs(maxX - minX));
    setHeight(std::abs(maxY - minY));
    setDepth(std::abs(maxZ - minZ));
    init();
}