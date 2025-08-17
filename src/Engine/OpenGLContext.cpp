#define GLM_ENABLE_EXPERIMENTAL
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>
#include <iostream>
#include "Engine/OpenGLContext.hpp"
#include <iostream>
// #include <windows.h>

#include "WorldObjects/Mesh/Mesh.hpp"
#include "WorldObjects/Mesh_DNA/Mesh_DNA.hpp"

OpenGLContext::OpenGLContext()
{
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenTextures(1, &fboTexture);
    glBindTexture(GL_TEXTURE_2D, fboTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTexture, 0);

    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "[OpenGLContext] ERROR : uncompleted Framebuffer !" << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    scene.initizalize();
    worldCenter = scene.worldCenter[0];
}

void OpenGLContext::addThreeDObjectToList(ThreeDObject *object)
{
    objects.push_back(object);
    std::cout << "[OpenGLContext] Adding object: " << object->getName() << std::endl;

    if(object->getIsMesh())
    {
        if (auto* mesh = dynamic_cast<Mesh*>(object))
        {
            MeshDNA* dna = new MeshDNA();
            dna->name = object->getName();
            mesh->setMeshDNA(dna);
            std::cout << "[OpenGLContext] MeshDNA attached to mesh: " << object->getName() << std::endl;
            dna->ensureInit(mesh->getModelMatrix());
                
        } 
    }
}

void OpenGLContext::removeThreeDobjectFromList(ThreeDObject *object)
{

    auto it = std::remove(objects.begin(), objects.end(), object);
    if (it != objects.end())
    {
        // message box

        std::cout << "[OpenGLContext] Removing object: " << object->getName() << std::endl;
        objects.erase(it, objects.end());
    }
    else
    {
        std::cerr << "[OpenGLContext] ERROR : Object not found in the list." << std::endl;
    }
}

void OpenGLContext::render()
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    if (!camera)
    {
        std::cerr << "[OpenGLContext] ERROR : No active camera. Rendering aborted.\n";
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width, height);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    viewMatrix = camera->getViewMatrix();
    projMatrix = camera->getProjectionMatrix((float)width / (float)height);

    scene.render(objects, projMatrix * viewMatrix);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
