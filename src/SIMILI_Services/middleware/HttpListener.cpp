#include "HttpListener.hpp"
#include "HttpSession.hpp"
#include <iostream>

namespace SIMILI {
namespace Server {

namespace beast = boost::beast;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

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

} // namespace Server
} // namespace SIMILI
