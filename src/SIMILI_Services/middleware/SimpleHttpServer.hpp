#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <windows.h>

#include <boost/beast/ssl.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <memory>
#include <thread>
#include "ContextRegistry.hpp"
#include "../router/RouterSim.hpp"

namespace SIMILI {
namespace Server {

namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

class SimpleHttpServer 
{
	net::io_context ioc_;
	std::unique_ptr<ssl::context> ssl_ctx_;
	std::thread server_thread_;
	bool running_;
	
	std::unique_ptr<ContextRegistry> contextRegistry_;
	std::unique_ptr<Router::RouterSim> router_;

	// Private constructor for singleton
	SimpleHttpServer();
	
	// Prevent copying
	SimpleHttpServer(const SimpleHttpServer&) = delete;
	SimpleHttpServer& operator=(const SimpleHttpServer&) = delete;

public:
	static SimpleHttpServer& getInstance();
	
	~SimpleHttpServer();

	void start(unsigned short http_port = 8080, unsigned short https_port = 8443);
	void stop();
	bool isRunning() const;
	
	ContextRegistry& getContextRegistry() { return *contextRegistry_; }
	const ContextRegistry& getContextRegistry() const { return *contextRegistry_; }
	
	Router::RouterSim& getRouter() { return *router_; }
	const Router::RouterSim& getRouter() const { return *router_; }
};

} // namespace Server
} // namespace SIMILI
