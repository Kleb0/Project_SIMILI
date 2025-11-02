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
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <memory>

namespace SIMILI {
namespace Server {

namespace beast = boost::beast;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

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

} // namespace Server
} // namespace SIMILI
