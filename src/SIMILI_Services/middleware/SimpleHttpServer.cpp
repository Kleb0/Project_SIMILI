#include "SimpleHttpServer.hpp"
#include <iostream>

namespace SIMILI {

namespace Server {

// ===========================
// HttpSession Implementation
// ===========================

HttpSession::HttpSession(tcp::socket&& socket)
    : stream_(std::move(socket)) 
{
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
    // Log the received message from JavaScript
    std::cout << "[HttpSession] " << req_.method_string() << " " << req_.target() << std::endl;
    
    if (!req_.body().empty()) 
    {
        std::cout << "[HttpSession] Received from JavaScript: " << req_.body() << std::endl;
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

    std::cout << "[HttpSession] Sending response: " << response_message << std::endl;

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

// ============================
// HttpsSession Implementation
// ============================

HttpsSession::HttpsSession(tcp::socket&& socket, ssl::context& ctx)
    : stream_(std::move(socket), ctx) 
{
}

void HttpsSession::run() 
{
    stream_.async_handshake(ssl::stream_base::server,
    beast::bind_front_handler(&HttpsSession::on_handshake, shared_from_this()));
}

void HttpsSession::on_handshake(beast::error_code ec) 
{
    if (ec) 
    {
        std::cerr << "[HttpsSession] Handshake error: " << ec.message() << std::endl;
        return;
    }
    do_read();
}

void HttpsSession::do_read() {
    req_ = {};
    beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));
    
    http::async_read(stream_, buffer_, req_,
        beast::bind_front_handler(&HttpsSession::on_read, shared_from_this()));
}

void HttpsSession::on_read(beast::error_code ec, std::size_t bytes_transferred) 
{
    boost::ignore_unused(bytes_transferred);
    
    if (ec == http::error::end_of_stream) 
    {
        return do_close();
    }
    
    if (ec) {
        std::cerr << "[HttpsSession] Read error: " << ec.message() << std::endl;
        return;
    }

    send_response();
}

void HttpsSession::send_response() 
{
    auto response = std::make_shared<http::response<http::string_body>>(
        http::status::ok, req_.version());
    
    response->set(http::field::server, "SIMILI/1.0");
    response->set(http::field::content_type, "application/json");
    response->keep_alive(req_.keep_alive());
    response->body() = R"({"status":"ok","message":"SIMILI HTTPS Server is running"})";
    response->prepare_payload();

    auto self = shared_from_this();

    http::async_write(stream_, *response,
        [self, response](beast::error_code ec, std::size_t) 
        {
            if (ec) 
            {
                std::cerr << "[HttpsSession] Write error: " << ec.message() << std::endl;
                return;
            }
            self->do_close();
        });
}

void HttpsSession::do_close() 
{
    beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));
    stream_.async_shutdown(
        beast::bind_front_handler(&HttpsSession::on_shutdown, shared_from_this()));
}

void HttpsSession::on_shutdown(beast::error_code ec) 
{
    if (ec) 
    {
        // Ignore common shutdown errors
    }
}

// ============================
// HttpListener Implementation
// ============================

HttpListener::HttpListener(net::io_context& ioc, tcp::endpoint endpoint)
: ioc_(ioc), acceptor_(net::make_strand(ioc)) 
{
    beast::error_code ec;

    acceptor_.open(endpoint.protocol(), ec);

    if (ec) 
    {
        std::cerr << "[HttpListener] Open error: " << ec.message() << std::endl;
        return;
    }

    acceptor_.set_option(net::socket_base::reuse_address(true), ec);

    if (ec) 
    {
        std::cerr << "[HttpListener] Set option error: " << ec.message() << std::endl;
        return;
    }

    acceptor_.bind(endpoint, ec);

    if (ec)
    {
        std::cerr << "[HttpListener] Bind error: " << ec.message() << std::endl;
        return;
    }

    acceptor_.listen(net::socket_base::max_listen_connections, ec);

    if (ec) 
    {
        std::cerr << "[HttpListener] Listen error: " << ec.message() << std::endl;
        return;
    }

    std::cout << "[HttpListener] HTTP Server listening on port " << endpoint.port() << std::endl;
}

void HttpListener::run() 
{
    do_accept();
}

void HttpListener::do_accept() 
{
    acceptor_.async_accept(net::make_strand(ioc_),
        beast::bind_front_handler(&HttpListener::on_accept, shared_from_this()));
}

void HttpListener::on_accept(beast::error_code ec, tcp::socket socket) 
{
    if (ec) 
    {
        std::cerr << "[HttpListener] Accept error: " << ec.message() << std::endl;
    } 
    else
    {
        std::make_shared<HttpSession>(std::move(socket))->run();
    }
    do_accept();
}

// =============================
// HttpsListener Implementation
// =============================

HttpsListener::HttpsListener(net::io_context& ioc, ssl::context& ctx, tcp::endpoint endpoint)
: ioc_(ioc), ctx_(ctx), acceptor_(net::make_strand(ioc)) 
{
    beast::error_code ec;

    acceptor_.open(endpoint.protocol(), ec);
    if (ec) 
    {
        std::cerr << "[HttpsListener] Open error: " << ec.message() << std::endl;
        return;
    }

    acceptor_.set_option(net::socket_base::reuse_address(true), ec);
    if (ec) 
    {
        std::cerr << "[HttpsListener] Set option error: " << ec.message() << std::endl;
        return;
    }

    acceptor_.bind(endpoint, ec);
    if (ec) 
    {
        std::cerr << "[HttpsListener] Bind error: " << ec.message() << std::endl;
        return;
    }

    acceptor_.listen(net::socket_base::max_listen_connections, ec);
    if (ec) 
    {
        std::cerr << "[HttpsListener] Listen error: " << ec.message() << std::endl;
        return;
    }

    std::cout << "[HttpsListener] HTTPS Server listening on port " << endpoint.port() << std::endl;
}

void HttpsListener::run() 
{
    do_accept();
}

void HttpsListener::do_accept() 
{
    acceptor_.async_accept(net::make_strand(ioc_),
        beast::bind_front_handler(&HttpsListener::on_accept, shared_from_this()));
}

void HttpsListener::on_accept(beast::error_code ec, tcp::socket socket) 
{
    if (ec) 
    {
        std::cerr << "[HttpsListener] Accept error: " << ec.message() << std::endl;
    } 
    else 
    {
        std::make_shared<HttpsSession>(std::move(socket), ctx_)->run();
    }
    do_accept();
}

// =================================
// SimpleHttpServer Implementation
// =================================

SimpleHttpServer::SimpleHttpServer() : running_(false) 
{
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

    std::cout << "[SimpleHttpServer] Starting server..." << std::endl;

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
