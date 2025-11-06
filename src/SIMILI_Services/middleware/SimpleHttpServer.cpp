#include "SimpleHttpServer.hpp"
#include "HttpListener.hpp"
#include "ConsoleLogger.hpp"
#include <iostream>
#include <sstream>

namespace SIMILI {
namespace Server {

namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

// =================================
// SimpleHttpServer Implementation
// =================================

SimpleHttpServer::SimpleHttpServer() 
    : running_(false), 
      contextRegistry_(std::make_unique<ContextRegistry>()),
      router_(std::make_unique<Router::RouterSim>())
{
    std::cout << "[SimpleHttpServer] Initialized with ECS ContextRegistry" << std::endl;
    router_->setDebugMode(true);
}

SimpleHttpServer& SimpleHttpServer::getInstance() 
{
	// Thread-safe singleton initialization (C++11 magic statics)
	static SimpleHttpServer instance;
	return instance;
}

SimpleHttpServer::~SimpleHttpServer()
{
    stop();
}

void SimpleHttpServer::start(unsigned short http_port, unsigned short https_port) 
{
    if (running_) 
    {
        std::cerr << "[SimpleHttpServer] Server already running" << std::endl;
        return;
    }

    std::cout << "[SimpleHttpServer] Starting HTTP server on port " << http_port << "..." << std::endl;
    
    // Initialize ConsoleLogger session once at server startup
    if (ConsoleLogger::getInstance().initializeSession()) {
        std::string appSessionId = ConsoleLogger::getInstance().getApplicationSessionId();
        std::ostringstream initMsg;
        initMsg << "[SIMILI Server] Application Session ID: " << appSessionId;
        ConsoleLogger::getInstance().addLog(initMsg.str(), "server");
        ConsoleLogger::getInstance().addLog("[SIMILI Server] HTTP Server started - Memory is persistent", "server");
        ConsoleLogger::getInstance().addLog("[SIMILI Server] All logs are preserved across requests and page reloads", "info");
        std::cout << "[SimpleHttpServer] ConsoleLogger initialized with session ID: " << appSessionId << std::endl;
    }

    // Start HTTP listener
    auto http_address = net::ip::make_address("127.0.0.1");
    std::make_shared<HttpListener>(ioc_, tcp::endpoint{http_address, http_port})->run();

    // Setup SSL context for HTTPS (self-signed certificate for POC)
    ssl_ctx_ = std::make_unique<ssl::context>(ssl::context::tlsv12_server);
    
    // For a simple POC, we'll skip SSL certificate setup
    // In production, you would load certificates here
    // ssl_ctx_->use_certificate_chain_file("server.crt");
    // ssl_ctx_->use_private_key_file("server.key", ssl::context::pem);
    
    // Start HTTPS listener (commented out as it needs certificates)
    // std::make_shared<HttpsListener>(ioc_, *ssl_ctx_, tcp::endpoint{http_address, https_port})->run();

    running_ = true;

    // Run the I/O context in a separate thread
    server_thread_ = std::thread([this]() {
        std::cout << "[SimpleHttpServer] Server thread started" << std::endl;
        ioc_.run();
        std::cout << "[SimpleHttpServer] Server thread stopped" << std::endl;
    });

    std::cout << "[SimpleHttpServer] HTTP server started on port " << http_port << std::endl;
    std::cout << "[SimpleHttpServer] Test with: http://localhost:" << http_port << std::endl;
}

void SimpleHttpServer::stop() 
{
    if (!running_) return;
    
    std::cout << "[SimpleHttpServer] Stopping server..." << std::endl;
    running_ = false;
    ioc_.stop();
    
    if (server_thread_.joinable()) 
    {
        server_thread_.join();
    }
    
    std::cout << "[SimpleHttpServer] Server stopped" << std::endl;
}

bool SimpleHttpServer::isRunning() const 
{
    return running_;
}

} // namespace Server
} // namespace SIMILI
