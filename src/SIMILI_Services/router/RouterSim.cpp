#include "RouterSim.hpp"
#include "../ThirdParty/json.hpp"
#include <algorithm>
#include <sstream>

using json = nlohmann::json;

namespace SIMILI {
namespace Router {

RouterSim::RouterSim() : debugMode_(false) {
	log("RouterSim initialized");
}

RouterSim::~RouterSim() {
	log("RouterSim destroyed");
}

// === Gestion des routes ===

void RouterSim::addRoute(const std::string& pattern, RouteHandler handler, 
						const std::string& method, const std::string& description) {
	std::lock_guard<std::mutex> lock(routesMutex_);
	routes_.emplace_back(pattern, handler, method, description);
	log("Route added: " + method + " " + pattern + " - " + description);
}

void RouterSim::addRoute(const Route& route) {
	std::lock_guard<std::mutex> lock(routesMutex_);
	routes_.push_back(route);
	log("Route added: " + route.method + " " + route.pattern + " - " + route.description);
}

void RouterSim::get(const std::string& pattern, RouteHandler handler, const std::string& description) {
	addRoute(pattern, handler, "GET", description);
}

void RouterSim::post(const std::string& pattern, RouteHandler handler, const std::string& description) {
	addRoute(pattern, handler, "POST", description);
}

void RouterSim::put(const std::string& pattern, RouteHandler handler, const std::string& description) {
	addRoute(pattern, handler, "PUT", description);
}

void RouterSim::del(const std::string& pattern, RouteHandler handler, const std::string& description) {
	addRoute(pattern, handler, "DELETE", description);
}

// === Middlewares ===

void RouterSim::addMiddleware(MiddlewareHandler middleware) {
	middlewares_.push_back(middleware);
	log("Middleware added");
}

// === Traitement des messages ===

bool RouterSim::parseMessage(const std::string& rawMessage, Message& outMessage) {
	try {
		json j = json::parse(rawMessage);
		
		outMessage.route = j.value("route", "");
		outMessage.method = j.value("method", "GET");
		outMessage.body = j.value("body", "");
		outMessage.requestId = j.value("requestId", "");
		
		// Parser les paramètres
		if (j.contains("params") && j["params"].is_object()) {
			for (auto& [key, value] : j["params"].items()) {
				if (value.is_string()) {
					outMessage.params[key] = value.get<std::string>();
				}
			}
		}
		
		log("Message parsed: " + outMessage.method + " " + outMessage.route);
		return true;
		
	} catch (const json::exception& e) {
		log("Failed to parse message: " + std::string(e.what()));
		return false;
	}
}

Response RouterSim::handleMessage(const Message& message) {
	log("Handling message: " + message.method + " " + message.route);
	
	// Créer une copie du message pour les middlewares
	Message processedMessage = message;
	
	// Appliquer les middlewares
	if (!applyMiddlewares(processedMessage)) {
		return createErrorResponse(message.requestId, 403, "Middleware rejected request");
	}
	
	// Trouver la route correspondante
	Route* route = findRoute(processedMessage.route, processedMessage.method);
	if (!route) {
		return createErrorResponse(message.requestId, 404, "Route not found: " + processedMessage.route);
	}
	
	try {
		// Extraire les paramètres de l'URL si nécessaire
		Message enrichedMessage = processedMessage;
		extractParams(route->pattern, processedMessage.route, enrichedMessage.params);
		
		// Exécuter le handler
		Response response = route->handler(enrichedMessage);
		response.requestId = message.requestId; // S'assurer que l'ID est correct
		
		log("Response generated with status: " + std::to_string(response.statusCode));
		return response;
		
	} catch (const std::exception& e) {
		log("Handler error: " + std::string(e.what()));
		return createErrorResponse(message.requestId, 500, "Internal server error: " + std::string(e.what()));
	}
}

std::string RouterSim::serializeResponse(const Response& response) {
	try {
		json j;
		j["requestId"] = response.requestId;
		j["statusCode"] = response.statusCode;
		j["statusMessage"] = response.statusMessage;
		j["body"] = response.body;
		
		if (!response.headers.empty()) {
			j["headers"] = response.headers;
		}
		
		return j.dump();
		
	} catch (const json::exception& e) {
		log("Failed to serialize response: " + std::string(e.what()));
		// Retour d'urgence
		return R"({"requestId":")" + response.requestId + R"(","statusCode":500,"statusMessage":"Serialization Error"})";
	}
}

// === Utilitaires ===

bool RouterSim::hasRoute(const std::string& pattern, const std::string& method) {
	std::lock_guard<std::mutex> lock(routesMutex_);
	return findRoute(pattern, method) != nullptr;
}

std::vector<Route> RouterSim::getRoutes() const {
	std::lock_guard<std::mutex> lock(routesMutex_);
	return routes_;
}

void RouterSim::setContext(const RouterContext& context) {
	std::lock_guard<std::mutex> lock(contextMutex_);
	context_ = context;
}

RouterContext& RouterSim::getContext() {
	std::lock_guard<std::mutex> lock(contextMutex_);
	return context_;
}

// === Méthodes privées ===

Route* RouterSim::findRoute(const std::string& pattern, const std::string& method) {
	for (auto& route : routes_) {
		if (route.method == method && matchRoute(route.pattern, pattern)) {
			return &route;
		}
	}
	return nullptr;
}

bool RouterSim::matchRoute(const std::string& routePattern, const std::string& requestPath) {
	// Support simple des wildcards
	if (routePattern == requestPath) {
		return true;
	}
	
	// Support du wildcard * à la fin
	if (routePattern.back() == '*') {
		std::string prefix = routePattern.substr(0, routePattern.length() - 1);
		return requestPath.substr(0, prefix.length()) == prefix;
	}
	
	// Support des paramètres de route (/object/{id})
	std::vector<std::string> routeParts = splitPath(routePattern);
	std::vector<std::string> requestParts = splitPath(requestPath);
	
	if (routeParts.size() != requestParts.size()) {
		return false;
	}
	
	for (size_t i = 0; i < routeParts.size(); ++i) {
		const std::string& routePart = routeParts[i];
		const std::string& requestPart = requestParts[i];
		
		// Si c'est un paramètre (commence par { et finit par })
		if (routePart.length() > 2 && routePart.front() == '{' && routePart.back() == '}') {
			continue; // Match n'importe quoi
		}
		
		// Sinon, doit matcher exactement
		if (routePart != requestPart) {
			return false;
		}
	}
	
	return true;
}

void RouterSim::extractParams(const std::string& routePattern, const std::string& requestPath, 
							 std::unordered_map<std::string, std::string>& params) {
	std::vector<std::string> routeParts = splitPath(routePattern);
	std::vector<std::string> requestParts = splitPath(requestPath);
	
	if (routeParts.size() != requestParts.size()) {
		return;
	}
	
	for (size_t i = 0; i < routeParts.size(); ++i) {
		const std::string& routePart = routeParts[i];
		const std::string& requestPart = requestParts[i];
		
		// Si c'est un paramètre (commence par { et finit par })
		if (routePart.length() > 2 && routePart.front() == '{' && routePart.back() == '}') {
			std::string paramName = routePart.substr(1, routePart.length() - 2);
			params[paramName] = requestPart;
		}
	}
}

bool RouterSim::applyMiddlewares(Message& message) {
	for (auto& middleware : middlewares_) {
		if (!middleware(message, context_)) {
			return false;
		}
	}
	return true;
}

Response RouterSim::createErrorResponse(const std::string& requestId, int statusCode, 
									  const std::string& message) {
	Response response(requestId, statusCode, message);
	json errorBody;
	errorBody["error"] = message;
	errorBody["code"] = statusCode;
	response.body = errorBody.dump();
	return response;
}

void RouterSim::log(const std::string& message) {
	if (debugMode_) {
		std::cout << "[RouterSim] " << message << std::endl;
	}
}

// Fonction helper pour diviser un chemin en parties
std::vector<std::string> RouterSim::splitPath(const std::string& path) {
	std::vector<std::string> parts;
	std::stringstream ss(path);
	std::string part;
	
	while (std::getline(ss, part, '/')) {
		if (!part.empty()) {
			parts.push_back(part);
		}
	}
	
	return parts;
}

} // namespace Router
} // namespace SIMILI
