#pragma once
#include <glm/glm.hpp>
#include <string>

class ThreeDObject; 
class Vertice
{
public:
    Vertice();
    ~Vertice();

    void initialize();
    void render(const glm::mat4& viewProj, const glm::mat4& modelMatrix);
    void destroy();

    void setPosition(const glm::vec3 &pos);
    glm::vec3 getPosition() const;

    void setColor(const glm::vec4 &color);
    glm::vec4 getColor() const;

    void setName(const std::string &name);
    const std::string& getName() const;

    glm::mat4 getModelMatrix() const;

    void setLocalPosition(const glm::vec3 &pos);
    glm::vec3 getLocalPosition() const;

    void setSelected(bool isSelected);
    bool isSelected() const;

    void applyTranslationToLocal(const glm::vec3& translation, const glm::mat4& parentModelMatrix);

    void setMeshParent(ThreeDObject* parent);
    ThreeDObject* getMeshParent() const;

private:
    ThreeDObject* meshParent = nullptr;
    unsigned int vao = 0;
    unsigned int vbo = 0;
    unsigned int shaderProgram = 0;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec4 color = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
    std::string name;
     glm::vec3 localPosition;

    void compileShaders();
    bool VerticeSelected = false;
};
