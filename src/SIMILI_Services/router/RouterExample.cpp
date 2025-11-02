// RouterExample.cpp - Exemple d'utilisation du RouterSim avec IPC Server
#pragma once

#include "../Engine/ipc_server.hpp"
#include "../SIMILI_Services/router/RouterSim.hpp"
#include "../SIMILI_Services/types/RouterTypes.hpp"
#include "../ThirdParty/json.hpp"
#include <iostream>
#include <memory>
#include <chrono>
#include <iomanip>
#include <sstream>

using json = nlohmann::json;

class SimiliApplication {
public:
    SimiliApplication() : ipcServer_("simili_app"), router_() {
        setupRouter();
    }

    bool start() {
        // Activer le mode debug du router
        router_.setDebugMode(true);
        
        // Démarrer l'IPC server avec callback vers le router
        return ipcServer_.start([this](const std::string& message) {
            handleIPCMessage(message);
        });
    }

    void stop() {
        ipcServer_.stop();
    }

private:
    void setupRouter() {
        // === Routes simples ===
        
        // Route de base pour tester la connexion
        router_.get("/ping", [](const SIMILI::Router::Message& msg) -> SIMILI::Router::Response {
            SIMILI::Router::Response response(msg.requestId);
            response.body = R"({"message":"pong","timestamp":")" + getCurrentTimestamp() + R"("})";
            return response;
        }, "Test de connectivité");

        // Route pour obtenir des informations sur l'application
        router_.get("/app/info", [](const SIMILI::Router::Message& msg) -> SIMILI::Router::Response {
            SIMILI::Router::Response response(msg.requestId);
            response.body = R"({
                "name": "SIMILI",
                "version": "1.0.0",
                "status": "running"
            })";
            return response;
        }, "Informations de l'application");

        // === Routes pour la gestion des objets 3D ===
        
        // Lister tous les objets de la scène
        router_.get("/scene/objects", [this](const SIMILI::Router::Message& msg) -> SIMILI::Router::Response {
            return handleGetSceneObjects(msg);
        }, "Obtenir la liste des objets 3D");

        // Créer un nouvel objet
        router_.post("/scene/objects", [this](const SIMILI::Router::Message& msg) -> SIMILI::Router::Response {
            return handleCreateObject(msg);
        }, "Créer un nouvel objet 3D");

        // Obtenir un objet spécifique par ID
        router_.get("/scene/objects/{id}", [this](const SIMILI::Router::Message& msg) -> SIMILI::Router::Response {
            return handleGetObject(msg);
        }, "Obtenir un objet 3D par ID");

        // Supprimer un objet
        router_.del("/scene/objects/{id}", [this](const SIMILI::Router::Message& msg) -> SIMILI::Router::Response {
            return handleDeleteObject(msg);
        }, "Supprimer un objet 3D");

        // === Routes pour la gestion de la caméra ===
        
        router_.get("/camera/position", [this](const SIMILI::Router::Message& msg) -> SIMILI::Router::Response {
            return handleGetCameraPosition(msg);
        }, "Obtenir la position de la caméra");

        router_.put("/camera/position", [this](const SIMILI::Router::Message& msg) -> SIMILI::Router::Response {
            return handleSetCameraPosition(msg);
        }, "Définir la position de la caméra");

        // === Middleware d'authentification simple ===
        router_.addMiddleware([](SIMILI::Router::Message& msg, const SIMILI::Router::RouterContext& ctx) -> bool {
            // Pour cet exemple, on accepte tout
            // Dans un vrai projet, vous pourriez vérifier des tokens, etc.
            std::cout << "[Middleware] Processing request: " << msg.route << std::endl;
            return true;
        });

        std::cout << "[SimiliApplication] Router configured with " << router_.getRoutes().size() << " routes" << std::endl;
    }

    void handleIPCMessage(const std::string& rawMessage) {
        std::cout << "[IPC] Received: " << rawMessage << std::endl;

        // Parser le message JSON
        SIMILI::Router::Message message;
        if (!router_.parseMessage(rawMessage, message)) {
            std::cerr << "[IPC] Failed to parse message" << std::endl;
            return;
        }

        // Traiter le message avec le router
        SIMILI::Router::Response response = router_.handleMessage(message);

        // Sérialiser et envoyer la réponse
        std::string responseJson = router_.serializeResponse(response);
        
        if (!ipcServer_.sendToUI(responseJson)) {
            std::cerr << "[IPC] Failed to send response" << std::endl;
        } else {
            std::cout << "[IPC] Response sent" << std::endl;
        }
    }

    // === Handlers spécifiques ===

    SIMILI::Router::Response handleGetSceneObjects(const SIMILI::Router::Message& msg) {
        SIMILI::Router::Response response(msg.requestId);
        
        // Simuler une liste d'objets (dans un vrai projet, ça viendrait de votre scène 3D)
        response.body = R"({
            "objects": [
                {"id": "1", "name": "Cube", "type": "mesh", "visible": true},
                {"id": "2", "name": "Sphere", "type": "mesh", "visible": true},
                {"id": "3", "name": "Light1", "type": "light", "visible": false}
            ],
            "count": 3
        })";
        
        return response;
    }

    SIMILI::Router::Response handleCreateObject(const SIMILI::Router::Message& msg) {
        SIMILI::Router::Response response(msg.requestId);
        
        try {
            // Parser le body pour obtenir les paramètres de création
            json requestData = json::parse(msg.body);
            std::string objectType = requestData.value("type", "cube");
            std::string objectName = requestData.value("name", "NewObject");
            
            // Simuler la création (dans un vrai projet, créer l'objet dans la scène)
            std::string newId = "obj_" + std::to_string(rand() % 10000);
            
            response.body = R"({
                "id": ")" + newId + R"(",
                "name": ")" + objectName + R"(",
                "type": ")" + objectType + R"(",
                "created": true
            })";
            
        } catch (const std::exception& e) {
            response.statusCode = 400;
            response.statusMessage = "Bad Request";
            response.body = R"({"error": "Invalid request body"})";
        }
        
        return response;
    }

    SIMILI::Router::Response handleGetObject(const SIMILI::Router::Message& msg) {
        SIMILI::Router::Response response(msg.requestId);
        
        // Récupérer l'ID depuis les paramètres de route
        auto it = msg.params.find("id");
        if (it != msg.params.end()) {
            std::string objectId = it->second;
            
            // Simuler la récupération de l'objet
            response.body = R"({
                "id": ")" + objectId + R"(",
                "name": "Object_)" + objectId + R"(",
                "type": "mesh",
                "position": {"x": 0, "y": 0, "z": 0},
                "rotation": {"x": 0, "y": 0, "z": 0},
                "scale": {"x": 1, "y": 1, "z": 1}
            })";
        } else {
            response.statusCode = 400;
            response.statusMessage = "Bad Request";
            response.body = R"({"error": "Missing object ID"})";
        }
        
        return response;
    }

    SIMILI::Router::Response handleDeleteObject(const SIMILI::Router::Message& msg) {
        SIMILI::Router::Response response(msg.requestId);
        
        auto it = msg.params.find("id");
        if (it != msg.params.end()) {
            std::string objectId = it->second;
            
            // Simuler la suppression
            response.body = R"({
                "id": ")" + objectId + R"(",
                "deleted": true
            })";
        } else {
            response.statusCode = 400;
            response.statusMessage = "Bad Request";
            response.body = R"({"error": "Missing object ID"})";
        }
        
        return response;
    }

    SIMILI::Router::Response handleGetCameraPosition(const SIMILI::Router::Message& msg) {
        SIMILI::Router::Response response(msg.requestId);
        
        // Simuler la position de la caméra
        response.body = R"({
            "position": {"x": 10.0, "y": 5.0, "z": 15.0},
            "target": {"x": 0.0, "y": 0.0, "z": 0.0},
            "up": {"x": 0.0, "y": 1.0, "z": 0.0}
        })";
        
        return response;
    }

    SIMILI::Router::Response handleSetCameraPosition(const SIMILI::Router::Message& msg) {
        SIMILI::Router::Response response(msg.requestId);
        
        try {
            json requestData = json::parse(msg.body);
            
            // Extraire la nouvelle position (dans un vrai projet, appliquer à la caméra)
            auto position = requestData["position"];
            
            response.body = R"({
                "updated": true,
                "position": {
                    "x": )" + std::to_string(position["x"].get<float>()) + R"(,
                    "y": )" + std::to_string(position["y"].get<float>()) + R"(,
                    "z": )" + std::to_string(position["z"].get<float>()) + R"(
                }
            })";
            
        } catch (const std::exception& e) {
            response.statusCode = 400;
            response.statusMessage = "Bad Request";
            response.body = R"({"error": "Invalid position data"})";
        }
        
        return response;
    }

    static std::string getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

private:
    IPCServer ipcServer_;
    SIMILI::Router::RouterSim router_;
};

// === Exemple d'utilisation dans votre main.cpp ===

/*
int main() {
    SimiliApplication app;
    
    std::cout << "Starting SIMILI Application with Router..." << std::endl;
    
    if (app.start()) {
        std::cout << "Application started successfully. Waiting for connections..." << std::endl;
        
        // Garder l'application en vie
        std::string input;
        std::cout << "Press Enter to stop..." << std::endl;
        std::getline(std::cin, input);
        
        app.stop();
        std::cout << "Application stopped." << std::endl;
    } else {
        std::cerr << "Failed to start application" << std::endl;
        return 1;
    }
    
    return 0;
}
*/