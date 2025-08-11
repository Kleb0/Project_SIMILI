#include "WorldObjects/Basic/Face.hpp"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

// === SHADERS ===
static const char* faceVertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec3 aPos;

uniform mat4 viewProj;
uniform mat4 model;

out vec3 vLocalPos; 

void main()
{
    vLocalPos = aPos;
    gl_Position = viewProj * model * vec4(aPos, 1.0);
}
)";

static const char* faceFragmentShaderSrc = R"(
#version 330 core
layout(location = 0) out vec4 FragColor;

in vec3 vLocalPos;

uniform bool  uSelected;
uniform vec4  uBaseColor; 
uniform vec2  uStripeScale;  
uniform float uStripeWidth; 

void main()
{
    if (!uSelected)
    {
        FragColor = uBaseColor;
        return;
    }

    vec3 orange = vec3(uBaseColor.rgb);
    vec3 blue   = vec3(0.1, 0.3, 1.0);

    vec2 uv = vLocalPos.xy;

    uv *= uStripeScale;

    float angle = radians(45.0);
    float c = cos(angle), s = sin(angle);
    mat2 R = mat2(c, -s, s, c);
    uv = R * uv;


    float band = step(fract(uv.y), uStripeWidth);

    vec3 finalRGB = mix(orange, blue, band);
    FragColor = vec4(finalRGB, 1.0);
}
)";


Face::Face(Vertice* v0, Vertice* v1, Vertice* v2, Vertice* v3,
           Edge* e0, Edge* e1, Edge* e2, Edge* e3)
{
    vertices = {v0, v1, v2, v3};
    edges = {e0, e1, e2, e3};
}

Face::~Face()
{
    destroy();
}

void Face::compileShaders()
{
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &faceVertexShaderSrc, nullptr);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &faceFragmentShaderSrc, nullptr);
    glCompileShader(fragmentShader);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void Face::initialize()
{
    compileShaders();

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    glBufferData(GL_ARRAY_BUFFER, 4 * 3 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void Face::uploadFromVertices()
{
    float faceData[4 * 3];
    for (int i = 0; i < 4; ++i)
    {
        const glm::vec3 p = vertices[i]->getLocalPosition();
        faceData[i * 3 + 0] = p.x;
        faceData[i * 3 + 1] = p.y;
        faceData[i * 3 + 2] = p.z;
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(faceData), faceData);
}

void Face::render(const glm::mat4& viewProj, const glm::mat4& modelMatrix)
{
    glUseProgram(shaderProgram);
    glm::mat4 modelWithFace = modelMatrix * faceTransform;

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "viewProj"), 1, GL_FALSE, glm::value_ptr(viewProj));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"),    1, GL_FALSE, glm::value_ptr(modelWithFace));

    GLint locSelected    = glGetUniformLocation(shaderProgram, "uSelected");
    GLint locBaseColor   = glGetUniformLocation(shaderProgram, "uBaseColor");
    GLint locStripeScale = glGetUniformLocation(shaderProgram, "uStripeScale");
    GLint locStripeWidth = glGetUniformLocation(shaderProgram, "uStripeWidth");

    if (selected)
    {
     
        const glm::vec4 orange(1.0f, 0.5f, 0.0f, 1.0f);
        glUniform1i(locSelected, GL_TRUE);
        glUniform4fv(locBaseColor, 1, glm::value_ptr(orange));

        glUniform2f(locStripeScale, 3.0f, 3.0f);
        glUniform1f(locStripeWidth, 0.25f);      
    }
    else
    {

        const glm::vec4 white(1.0f);
        glUniform1i(locSelected, GL_FALSE);
        glUniform4fv(locBaseColor, 1, glm::value_ptr(white));

    
        glUniform2f(locStripeScale, 1.0f, 1.0f);
        glUniform1f(locStripeWidth, 0.5f);
    }

    glBindVertexArray(vao);
    uploadFromVertices(); 

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glBindVertexArray(0);
}



void Face::destroy()
{
    if (vao != 0)
    {
        glDeleteVertexArrays(1, &vao);
        vao = 0;
    }

    if (vbo != 0)
    {
        glDeleteBuffers(1, &vbo);
        vbo = 0;
    }

    if (shaderProgram != 0)
    {
        glDeleteProgram(shaderProgram);
        shaderProgram = 0;
    }
}


const std::vector<Vertice*>& Face::getVertices() const
{
    return vertices;
}

const std::vector<Edge*>& Face::getEdges() const
{
    return edges;
}

void Face::applyWorldDelta(const glm::mat4& deltaWorld, const glm::mat4& parentModel, bool bakeToVertices)
{
    if (!bakeToVertices)
    {
        glm::mat4 faceLocalDelta = glm::inverse(parentModel) * deltaWorld * parentModel;
        faceTransform = faceLocalDelta * faceTransform;
        return;
    }


    for (auto* v : vertices)
    {
        glm::vec4 L  = glm::vec4(v->getLocalPosition(), 1.0f);
        glm::vec4 W  = parentModel * faceTransform * L;
        glm::vec4 W2 = deltaWorld * W;
        glm::vec4 L2 = glm::inverse(parentModel) * W2;

        v->setLocalPosition(glm::vec3(L2));
        v->setPosition(glm::vec3(W2)); 
    }

    faceTransform = glm::mat4(1.0f);
}

