#pragma once

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
#include <boost/asio/ip/tcp.hpp>
#include <memory>
#include <string>

namespace SIMILI {
namespace Server {

namespace beast = boost::beast;
namespace http = beast::http;
using tcp = boost::asio::ip::tcp;

// Simple HTTP session handler
class HttpSession : public std::enable_shared_from_this<HttpSession> 
{
	beast::tcp_stream stream_;
	beast::flat_buffer buffer_;
	http::request<http::string_body> req_;
	std::string sessionId_; // Unique session ID

	// Generate a random session ID
	static std::string generateSessionId();

public:
	explicit HttpSession(tcp::socket&& socket);
	void run();
	
	const std::string& getSessionId() const { return sessionId_; }

private:
	void do_read();
	void on_read(beast::error_code ec, std::size_t bytes_transferred);
	void send_response();
	void do_close();
};

} // namespace Server
} // namespace SIMILI
