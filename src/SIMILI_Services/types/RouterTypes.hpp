#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <memory>

namespace SIMILI {
namespace Router {

// Structure pour un message reçu du frontend
struct Message {
	std::string route;           // Ex: "/object/create", "/scene/load"
	std::string method;          // GET, POST, PUT, DELETE
	std::unordered_map<std::string, std::string> params;  // Paramètres de query
	std::string body;            // Données JSON optionnelles
	std::string requestId;       // ID unique pour le matching des réponses
	
	Message() = default;
	Message(const std::string& r, const std::string& m = "GET") 
		: route(r), method(m) {}
};

// Structure pour une réponse vers le frontend
struct Response {
	std::string requestId;       // Même ID que la requête
	int statusCode;              // 200, 404, 500, etc.
	std::string statusMessage;   // "OK", "Not Found", "Internal Error"
	std::string body;            // Données JSON de réponse
	std::unordered_map<std::string, std::string> headers; // Headers optionnels
	
	Response() : statusCode(200), statusMessage("OK") {}
	Response(const std::string& id, int code = 200, const std::string& msg = "OK")
		: requestId(id), statusCode(code), statusMessage(msg) {}
};

// Type de handler pour traiter une route
using RouteHandler = std::function<Response(const Message&)>;

// Context pour passer des informations supplémentaires aux handlers
struct RouterContext {
	std::unordered_map<std::string, std::string> globals; // Variables globales
	void* userData;              // Pointeur vers des données utilisateur
	
	RouterContext() : userData(nullptr) {}
};

// Middleware function type - peut modifier le message avant le handler
using MiddlewareHandler = std::function<bool(Message&, const RouterContext&)>;

// Structure pour définir une route
struct Route {
	std::string pattern;         // Pattern de la route ("/object/*", "/scene/load")
	std::string method;          // Méthode HTTP (GET, POST, etc.)
	RouteHandler handler;        // Handler à exécuter
	std::string description;     // Description pour le debug
	
	Route() = default;
	Route(const std::string& p, RouteHandler h, const std::string& m = "GET", const std::string& desc = "")
		: pattern(p), method(m), handler(h), description(desc) {}
};

} // namespace Router
} // namespace SIMILI