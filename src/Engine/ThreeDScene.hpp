#pragma once

#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "Engine/OpenGLContext.hpp"
#include "Engine/Guizmo.hpp"
#include <WorldObjects/Entities/ThreeDObject.hpp>
#include "WorldObjects/Camera/Camera.hpp"
#include "Engine/ThreeDScene_DNA/ThreeDScene_DNA.hpp"
#include <iostream>
#include <list>

class HistoryLogic; 
class OpenGLContext;
class Camera;
class HierarchyInspector;
class ThreeDWindow;

struct GraveyardEntry {
    ThreeDObject* object;
    std::vector<Vertice*> vertices;
    std::vector<Edge*> edges;
    std::vector<Face*> faces;
};

class ThreeDScene
{
public:
    ThreeDScene();
    ~ThreeDScene() = default;

    void setOpenGLContext(OpenGLContext* ctx) { glctx = ctx; }
    OpenGLContext* getOpenGLContext() const { return glctx; }

    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;
    glm::mat4 getViewProjMatrix() const;

    void initizalize();
    void resize(int w, int h);
    void render();
    void drawBackgroundGradient();

    GLuint getTexture() const; 
    std::vector<glm::vec3> worldCenter;

    std::list<ThreeDObject*>& getObjectsRef() { return objects; }
    void addObject(ThreeDObject* object);
    bool removeObject(ThreeDObject* object);

    void pushInGraveyard(ThreeDObject * obj);
    // bool reviveFromGraveyardById(uint64_t id);
    const std::list<ThreeDObject *>& getGraveyard() const { return graveyard; }

    ThreeDScene_DNA* getSceneDNA() const { return sceneDNA; }
    void setSceneDNA(ThreeDScene_DNA* dna, bool takeOwnership = true);

    void setActiveCamera(Camera* cam);
    Camera* getActiveCamera() const { return activeCamera; }

    bool containsObject(const ThreeDObject* obj) const;
    bool removeObjectFromSceneDNA(uint64_t objectID);

    void setHierarchyInspector(HierarchyInspector* inspector) { hierarchyInspector = inspector; }
    void getHierarchyInspector(HierarchyInspector** inspector) { *inspector = hierarchyInspector; }
    HierarchyInspector* getHierarchyInspector() const { return hierarchyInspector; }

    void setThreeDWindow(ThreeDWindow* window) { threeDWindow = window; }
    ThreeDWindow* getThreeDWindow() const { return threeDWindow; }

private:
    friend class HistoryLogic;
    friend class OpenGLContext;

    HierarchyInspector* hierarchyInspector = nullptr;
    ThreeDWindow* threeDWindow = nullptr;

    glm::mat4 lastViewProj{1.0f};
    std::list<ThreeDObject *> objects;
    std::list<ThreeDObject *> graveyard;
    std::vector<GraveyardEntry> meshGraveyard;

    Camera camera;
    Camera* activeCamera{nullptr};

    OpenGLContext* glctx{nullptr};

    void compileShaders();
    GLuint gridVAO;
    GLuint gridVBO;
    GLuint shaderProgram;

    ThreeDScene_DNA* sceneDNA{nullptr};
    
    bool ownsSceneDNA{true};
    bool ownsViewproj{false};
};
