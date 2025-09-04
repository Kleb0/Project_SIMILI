#define GLM_ENABLE_EXPERIMENTAL
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Engine/ThreeDScene.hpp"
#include <iostream>
#include <filesystem>
#include <vector>
#include <algorithm> 
#include "WorldObjects/Mesh/Mesh.hpp"
#include "WorldObjects/Mesh_DNA/Mesh_DNA.hpp"
#include "WorldObjects/Camera/Camera.hpp"
#include "UI/HierarchyInspectorLogic/HierarchyInspector.hpp"

// === SHADERS ===

const char *vertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 viewProj;
uniform mat4 model;
void main()
{
    
    gl_Position = viewProj * model * vec4(aPos, 1.0);
}
)";

const char *fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
void main()
{
    FragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
)";

// === CLASS ===
ThreeDScene::ThreeDScene() {}

static inline void erasePtr(std::list<ThreeDObject*>& L, ThreeDObject* p) 
{
    L.remove(p);
}


glm::mat4 ThreeDScene::getViewMatrix() const
{
    if (!activeCamera) return glm::mat4(1.0f);
    return activeCamera->getViewMatrix();
}

glm::mat4 ThreeDScene::getProjectionMatrix() const
{
    if (!activeCamera) return glm::mat4(1.0f);
    float aspect = 1.0f;
    if (glctx && glctx->getHeight() > 0)
        aspect = float(glctx->getWidth()) / float(glctx->getHeight());
    return activeCamera->getProjectionMatrix(aspect);
}

glm::mat4 ThreeDScene::getViewProjMatrix() const
{
    return getProjectionMatrix() * getViewMatrix();
}

void ThreeDScene::setActiveCamera(Camera* cam)
{
    if (!cam) 
    { 
        activeCamera = nullptr; 
        return; 
    }

    activeCamera = cam;

}


GLuint ThreeDScene::getTexture() const 
{
    return glctx ? glctx->getTexture() : 0;
}



// ----- Graveyard ------- //

static inline void clearSelectionRecursive(ThreeDObject* o)
{
    if (!o) return;
    o->setSelected(false);
    for (auto* child : o->getChildren())
        clearSelectionRecursive(child);
}


bool ThreeDScene::containsObject(const ThreeDObject* obj) const
{
    return std::find(objects.begin(), objects.end(), obj) != objects.end();
}


void ThreeDScene::softRemove(ThreeDObject* obj)
{
    if (!obj) return;

    clearSelectionRecursive(obj);

    erasePtr(objects, obj);
    graveyard.push_back(obj);
}

bool ThreeDScene::revive(ThreeDObject* obj)
{
    if (!obj) return false;

    clearSelectionRecursive(obj);

    if (auto itG = std::find(graveyard.begin(), graveyard.end(), obj); itG != graveyard.end())
        graveyard.erase(itG);


    if (!containsObject(obj))
        objects.push_back(obj);

    if (auto* mesh = dynamic_cast<Mesh*>(obj))
    {
        if (!mesh->getMeshDNA())
            mesh->setMeshDNA(new MeshDNA(), true);

        mesh->finalize();
        return true;
    }

    obj->initialize();
    return true;
}


void ThreeDScene::compileShaders()
{
	unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
	glCompileShader(vertexShader);

	unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
	glCompileShader(fragmentShader);

	shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	
}

void ThreeDScene::setSceneDNA(ThreeDScene_DNA* dna, bool takeOwnership)
{
    if (sceneDNA && ownsSceneDNA && sceneDNA != dna) {
        delete sceneDNA;
    }
    sceneDNA = dna;
    ownsSceneDNA = takeOwnership;
}


void ThreeDScene::initizalize()
{
    compileShaders();

    if (!sceneDNA)
    {
        auto* dna = new ThreeDScene_DNA();
        dna->name = "MainSceneDNA";
        setSceneDNA(dna, true);
    }
    sceneDNA->ensureInit();

    std::vector<float> gridVertices;
    const int gridSize = 10;

    for (int i = -gridSize / 2; i <= gridSize / 2; ++i)
    {
        gridVertices.insert(gridVertices.end(), {
            -gridSize / 2.0f, 0.0f, (float)i,
             gridSize / 2.0f, 0.0f, (float)i
        });
        gridVertices.insert(gridVertices.end(), {
            (float)i, 0.0f, -gridSize / 2.0f,
            (float)i, 0.0f,  gridSize / 2.0f
        });
    }

    glGenVertexArrays(1, &gridVAO);
    glGenBuffers(1, &gridVBO);

    glBindVertexArray(gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER, gridVertices.size() * sizeof(float), gridVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    worldCenter.clear();
    worldCenter.emplace_back(0.0f, 0.0f, 0.0f);
}


void ThreeDScene::resize(int w, int h)
{
    if (glctx) glctx->resize(w, h);
}


void ThreeDScene::render()
{
    if (!glctx) {
        std::cerr << "[ThreeDScene] ERROR: No OpenGLContext set (call setOpenGLContext).\n";
        return;
    }
    if (!activeCamera) {
        std::cerr << "[ThreeDScene] ERROR: No active camera set.\n";
        return;
    }

    glctx->bindForRendering();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const int w = glctx->getWidth();
    const int h = glctx->getHeight();
    const float aspect = (h > 0) ? float(w) / float(h) : 1.0f;

    glm::mat4 view = activeCamera->getViewMatrix();
    glm::mat4 proj = activeCamera->getProjectionMatrix(aspect);
    glm::mat4 viewProj = proj * view;

    if (!ownsViewproj) {
        lastViewProj = viewProj;
        std::cout << "[ThreeDScene] New view projection matrix set.\n";
        ownsViewproj = true;
    }

    glDisable(GL_DEPTH_TEST);
    drawBackgroundGradient();
    glEnable(GL_DEPTH_TEST);

    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "viewProj"), 1, GL_FALSE, glm::value_ptr(viewProj));

    glBindVertexArray(gridVAO);
    glm::mat4 model(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glDrawArrays(GL_LINES, 0, 44);
    glBindVertexArray(0);

    for (auto *obj : objects) {
        if (obj) obj->render(viewProj);
    }

    glctx->unbind();
}



void ThreeDScene::drawBackgroundGradient()
{
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
}



void ThreeDScene::addObject(ThreeDObject* object)
{
    if (!object) return;

    objects.push_back(object);
    std::cout << "[ThreeDScene] Adding object: " << object->getName() << std::endl;

    if (auto* sdna = getSceneDNA())
        sdna->trackAddObject(object->getName(), object);

    if (object->getIsMesh())
    {
        if (auto* mesh = dynamic_cast<Mesh*>(object))
        {
            if (!mesh->getMeshDNA())
            {
                auto* dna = new MeshDNA();
                dna->name = object->getName();

                mesh->setMeshDNA(dna, true);
                std::cout << "[ThreeDScene] MeshDNA attached to mesh: " << object->getName() << std::endl;
            }

            mesh->getMeshDNA()->ensureInit(mesh->getModelMatrix());
        }
    }

    object->initialize();
}


bool ThreeDScene::removeObject(ThreeDObject* object)
{
    if (!object) return false;

    auto it = std::find(objects.begin(), objects.end(), object);
    if (it == objects.end())
    {
        std::cerr << "[ThreeDScene] ERROR: Object not found in scene objects." << std::endl;
        return false;
    }

    std::cout << "[ThreeDScene] Removing object: " << object->getName() << " with ID : " << object->getID() << std::endl;

    if (auto* sdna = getSceneDNA())
        sdna->trackRemoveObject(object->getName(), object);
    softRemove(object);
    return true;
}

bool ThreeDScene::removeObjectFromSceneDNA(uint64_t objectID)
{
    for (auto it = objects.begin(); it != objects.end(); ++it)
    {
        ThreeDObject* obj = *it;
        if (obj && obj->getID() == objectID)
        {
            std::cout << "[ThreeDScene] Deleting object from scene (no tracking): " 
            << obj->getName() << " (ID=" << objectID << ")" << std::endl;

            obj->destroy();
            hierarchyInspector->redrawSlotsList();
            erasePtr(objects, obj);
            return true;
        }
    }

    return false;
}

