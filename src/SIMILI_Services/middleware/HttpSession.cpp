#include "HttpSession.hpp"
#include "ConsoleLogger.hpp"
#include <iostream>
#include <sstream>
#include <random>
#include <iomanip>

namespace SIMILI {
namespace Server {

namespace beast = boost::beast;
namespace http = beast::http;

std::string HttpSession::generateSessionId() 
{
    // Generate a random 8-character hexadecimal ID
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 0xFFFFFFFF);
    
    std::ostringstream oss;
    oss << std::hex << std::setw(8) << std::setfill('0') << dis(gen);
    return oss.str();
}

HttpSession::HttpSession(tcp::socket&& socket)
    : stream_(std::move(socket)), sessionId_(generateSessionId())
{
    // Don't log individual HTTP connections - they are ephemeral
    // Only the application session ID matters
}

void HttpSession::run() 
{
    do_read();
}

void HttpSession::do_read() 
{
    req_ = {};
    stream_.expires_after(std::chrono::seconds(30));
    
    http::async_read(stream_, buffer_, req_,
        beast::bind_front_handler(&HttpSession::on_read, shared_from_this()));
}

void HttpSession::on_read(beast::error_code ec, std::size_t bytes_transferred) 
{
    boost::ignore_unused(bytes_transferred);
    
    if (ec == http::error::end_of_stream) {
        return do_close();
    }
    
    if (ec) {
        std::cerr << "[HttpSession] Read error: " << ec.message() << std::endl;
        return;
    }

    send_response();
}

void HttpSession::send_response() 
{
    // Handle CORS preflight requests globally: reply to OPTIONS with appropriate headers
    if (req_.method() == http::verb::options) {
        auto response = std::make_shared<http::response<http::string_body>>(http::status::ok, req_.version());
        response->set(http::field::server, "SIMILI/1.0");
        response->set(http::field::content_type, "text/plain");
        response->set("Access-Control-Allow-Origin", "*");
        response->set("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        response->set("Access-Control-Allow-Headers", "Content-Type, Authorization");
        response->set("Access-Control-Max-Age", "3600");
        response->keep_alive(req_.keep_alive());
        response->body() = "";
        response->prepare_payload();

        auto self = shared_from_this();
        http::async_write(stream_, *response,
            [self, response](beast::error_code ec, std::size_t) 
            {
                self->stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
            });
        return;
    }

    // Handle /console/logs endpoint (don't log this to avoid infinite loop)
    if (req_.target() == "/console/logs") {
        auto logs = ConsoleLogger::getInstance().getLogs(false); // Don't clear logs - preserve them across sessions
        
        auto response = std::make_shared<http::response<http::string_body>>(
            http::status::ok, req_.version());
        
        response->set(http::field::server, "SIMILI/1.0");
        response->set(http::field::content_type, "application/json");
        response->set(http::field::access_control_allow_origin, "*");
        response->keep_alive(req_.keep_alive());
        
        // Build JSON array of logs
        std::ostringstream json;
        json << "{\"logs\":[";
        for (size_t i = 0; i < logs.size(); ++i) {
            if (i > 0) json << ",";
            json << "{\"message\":\"" << logs[i].message << "\",\"type\":\"" << logs[i].type << "\"}";
        }
        json << "]}";
        
        response->body() = json.str();
        response->prepare_payload();

        auto self = shared_from_this();
        http::async_write(stream_, *response,
            [self, response](beast::error_code ec, std::size_t) 
            {
                self->stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
            });
        return;
    }
    
    // Handle /console/clear endpoint - explicit clear command
    if (req_.target() == "/console/clear") {
        ConsoleLogger::getInstance().clear();
        
        auto response = std::make_shared<http::response<http::string_body>>(
            http::status::ok, req_.version());
        
        response->set(http::field::server, "SIMILI/1.0");
        response->set(http::field::content_type, "application/json");
        response->set(http::field::access_control_allow_origin, "*");
        response->keep_alive(req_.keep_alive());
        response->body() = R"({"status":"ok","message":"Console cleared"})";
        response->prepare_payload();

        auto self = shared_from_this();
        http::async_write(stream_, *response,
            [self, response](beast::error_code ec, std::size_t) 
            {
                self->stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
            });
        return;
    }
    
    // Handle /console/test endpoint
    if (req_.target() == "/console/test") {
        std::string testMsg = "[HttpSession] Console frontend connected";
        std::cout << testMsg << std::endl;
        ConsoleLogger::getInstance().addLog(testMsg, "info");
        
        auto response = std::make_shared<http::response<http::string_body>>(
            http::status::ok, req_.version());
        
        response->set(http::field::server, "SIMILI/1.0");
        response->set(http::field::content_type, "application/json");
        response->set(http::field::access_control_allow_origin, "*");
        response->keep_alive(req_.keep_alive());
        response->body() = R"({"status":"ok","message":"Console test successful"})";
        response->prepare_payload();

        auto self = shared_from_this();
        http::async_write(stream_, *response,
            [self, response](beast::error_code ec, std::size_t) 
            {
                self->stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
            });
        return;
    }
    
    // Handle /hello endpoint - Test message from JavaScript
    if (req_.target() == "/hello") {
        std::string serverMsg = "[Server] Get Hello World from Javascript to server !";
        std::cout << serverMsg << std::endl;
        ConsoleLogger::getInstance().addLog(serverMsg, "server");
        
        if (!req_.body().empty()) {
            std::ostringstream bodyMsg;
            bodyMsg << "[Server] Received: " << req_.body();
            std::cout << bodyMsg.str() << std::endl;
            ConsoleLogger::getInstance().addLog(bodyMsg.str(), "server");
        }
        
        auto response = std::make_shared<http::response<http::string_body>>(
            http::status::ok, req_.version());
        
        response->set(http::field::server, "SIMILI/1.0");
        response->set(http::field::content_type, "application/json");
        response->set(http::field::access_control_allow_origin, "*");
        response->keep_alive(req_.keep_alive());
        response->body() = R"({"status":"ok","message":"Get Hello World from Javascript to server !"})";
        response->prepare_payload();

        auto self = shared_from_this();
        http::async_write(stream_, *response,
            [self, response](beast::error_code ec, std::size_t) 
            {
                self->stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
            });
        return;
    }
    
    // Log the received message from JavaScript (for all other endpoints)
    std::ostringstream logMsg;
    logMsg << "[HttpSession] " << req_.method_string() << " " << req_.target();
    std::cout << logMsg.str() << std::endl;
    ConsoleLogger::getInstance().addLog(logMsg.str(), "server");
    
    if (!req_.body().empty()) 
    {
        std::ostringstream bodyMsg;
        bodyMsg << "[HttpSession] Received from JavaScript: " << req_.body();
        std::cout << bodyMsg.str() << std::endl;
        ConsoleLogger::getInstance().addLog(bodyMsg.str(), "server");
    }
    
    auto response = std::make_shared<http::response<http::string_body>>(
        http::status::ok, req_.version());
    
    response->set(http::field::server, "SIMILI/1.0");
    response->set(http::field::content_type, "application/json");
    response->set(http::field::access_control_allow_origin, "*"); // Enable CORS
    response->keep_alive(req_.keep_alive());
    
    std::string response_message = "Get message from HTML hello world";
    response->body() = R"({"status":"ok","message":")" + response_message + R"("})";
    response->prepare_payload();

    std::ostringstream responseMsg;
    responseMsg << "[HttpSession] Sending response: " << response_message;
    std::cout << responseMsg.str() << std::endl;
    ConsoleLogger::getInstance().addLog(responseMsg.str(), "server");

    auto self = shared_from_this();

    http::async_write(stream_, *response,
        [self, response](beast::error_code ec, std::size_t) 
        {
            self->stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
        });
}

void HttpSession::do_close() 
{
    beast::error_code ec;
    stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
}

} // namespace Server
} // namespace SIMILI
