#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <memory>

namespace SIMILI {
namespace Router {

struct Message 
{
	std::string route;
	std::string method;
	std::unordered_map<std::string, std::string> params;
	std::string body;
	std::string requestId;
	
	Message() = default;
	Message(const std::string& r, const std::string& m = "GET") 
		: route(r), method(m) {}
};


struct Response 
{
	std::string requestId; 
	int statusCode;
	std::string statusMessage; 
	std::string body;
	std::unordered_map<std::string, std::string> headers;
	
	Response() : statusCode(200), statusMessage("OK") {}
	Response(const std::string& id, int code = 200, const std::string& msg = "OK")
		: requestId(id), statusCode(code), statusMessage(msg) {}
};

using RouteHandler = std::function<Response(const Message&)>;

struct RouterContext 
{
	std::unordered_map<std::string, std::string> globals;
	void* userData;
	
	RouterContext() : userData(nullptr) {}
};

using MiddlewareHandler = std::function<bool(Message&, const RouterContext&)>;

// this struct represents a registered route
struct Route 
{
	std::string pattern;
	std::string method; 
	RouteHandler handler; 
	std::string description;
	
	Route() = default;
	Route(const std::string& p, RouteHandler h, const std::string& m = "GET", const std::string& desc = "")
		: pattern(p), method(m), handler(h), description(desc) {}
};

} 
}