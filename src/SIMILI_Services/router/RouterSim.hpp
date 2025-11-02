#pragma once

#include "../types/RouterTypes.hpp"
#include <vector>
#include <memory>
#include <mutex>
#include <iostream>

namespace SIMILI {
namespace Router {

class RouterSim 
{
public:
	RouterSim();
	~RouterSim();

	// === Gestion des routes ===
	
	// Ajouter une route simple
	void addRoute(const std::string& pattern, RouteHandler handler, 
				  const std::string& method = "GET", const std::string& description = "");
	
	// Ajouter une route avec tous les paramètres
	void addRoute(const Route& route);
	
	// Méthodes helper pour les différents verbes HTTP
	void get(const std::string& pattern, RouteHandler handler, const std::string& description = "");
	void post(const std::string& pattern, RouteHandler handler, const std::string& description = "");
	void put(const std::string& pattern, RouteHandler handler, const std::string& description = "");
	void del(const std::string& pattern, RouteHandler handler, const std::string& description = "");
	
	// === Gestion des middlewares ===
	void addMiddleware(MiddlewareHandler middleware);
	
	// === Traitement des messages ===
	
	// Parser un message JSON brut en structure Message
	bool parseMessage(const std::string& rawMessage, Message& outMessage);
	
	// Traiter un message et retourner une réponse
	Response handleMessage(const Message& message);
	
	// Sérialiser une réponse en JSON
	std::string serializeResponse(const Response& response);
	
	// === Utilitaires ===
	
	// Vérifier si une route existe
	bool hasRoute(const std::string& pattern, const std::string& method = "GET");
	
	// Lister toutes les routes enregistrées (pour debug)
	std::vector<Route> getRoutes() const;
	
	// Définir le context global
	void setContext(const RouterContext& context);
	RouterContext& getContext();
	
	// === Configuration ===
	void setDebugMode(bool enabled) { debugMode_ = enabled; }
	bool isDebugMode() const { return debugMode_; }

private:
	// === Méthodes internes ===
	
	// Trouver la route correspondante
	Route* findRoute(const std::string& pattern, const std::string& method);
	
	// Vérifier si un pattern matche une route (support wildcards simples)
	bool matchRoute(const std::string& routePattern, const std::string& requestPath);
	
	// Extraire les paramètres d'une URL
	void extractParams(const std::string& routePattern, const std::string& requestPath, 
					  std::unordered_map<std::string, std::string>& params);
	
	// Appliquer les middlewares
	bool applyMiddlewares(Message& message);
	
	// Créer une réponse d'erreur
	Response createErrorResponse(const std::string& requestId, int statusCode, 
							   const std::string& message);
	
	// Log pour debug
	void log(const std::string& message);
	
	// Helper pour diviser un chemin en parties
	std::vector<std::string> splitPath(const std::string& path);

private:
	std::vector<Route> routes_;
	std::vector<MiddlewareHandler> middlewares_;
	RouterContext context_;
	bool debugMode_;
	mutable std::mutex routesMutex_;  // Thread safety
	mutable std::mutex contextMutex_;
};

} // namespace Router
} // namespace SIMILI
