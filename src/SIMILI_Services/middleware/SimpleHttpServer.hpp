#pragma once

// Must be included BEFORE any Windows headers
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <windows.h>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <memory>
#include <thread>

namespace SIMILI {
namespace Server {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

// Forward declarations
class HttpSession;
class HttpsSession;
class HttpListener;
class HttpsListener;

// Simple HTTP session handler
class HttpSession : public std::enable_shared_from_this<HttpSession> 
{
	beast::tcp_stream stream_;
	beast::flat_buffer buffer_;
	http::request<http::string_body> req_;

public:
	explicit HttpSession(tcp::socket&& socket);
	void run();

private:
	void do_read();
	void on_read(beast::error_code ec, std::size_t bytes_transferred);
	void send_response();
	void do_close();
};

// Simple HTTPS session handler
class HttpsSession : public std::enable_shared_from_this<HttpsSession> 
{
	beast::ssl_stream<beast::tcp_stream> stream_;
	beast::flat_buffer buffer_;
	http::request<http::string_body> req_;

public:
	explicit HttpsSession(tcp::socket&& socket, ssl::context& ctx);
	void run();

private:
	void on_handshake(beast::error_code ec);
	void do_read();
	void on_read(beast::error_code ec, std::size_t bytes_transferred);
	void send_response();
	void do_close();
	void on_shutdown(beast::error_code ec);
};

// HTTP Listener
class HttpListener : public std::enable_shared_from_this<HttpListener> 
{
	net::io_context& ioc_;
	tcp::acceptor acceptor_;

public:
	HttpListener(net::io_context& ioc, tcp::endpoint endpoint);
	void run();

private:
	void do_accept();
	void on_accept(beast::error_code ec, tcp::socket socket);
};

// HTTPS Listener
class HttpsListener : public std::enable_shared_from_this<HttpsListener> 
{
	net::io_context& ioc_;
	ssl::context& ctx_;
	tcp::acceptor acceptor_;

public:
	HttpsListener(net::io_context& ioc, ssl::context& ctx, tcp::endpoint endpoint);
	void run();

private:
	void do_accept();
	void on_accept(beast::error_code ec, tcp::socket socket);
};

// Main server class
class SimpleHttpServer 
{
	net::io_context ioc_;
	std::unique_ptr<ssl::context> ssl_ctx_;
	std::thread server_thread_;
	bool running_;

public:
	SimpleHttpServer();
	~SimpleHttpServer();

	void start(unsigned short http_port = 8080, unsigned short https_port = 8443);
	void stop();
	bool isRunning() const;
};

} // namespace Server
} // namespace SIMILI
