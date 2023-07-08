#include <spdlog/spdlog.h>
#include "RtmpServer.hpp"
#include "Server.hpp"
#include "Conf.hpp"

namespace ms777 {
RtmpServer::RtmpServer(Server &server) : server_(server), acceptor_(server.get_io_context())
{
}

RtmpServer::~RtmpServer()
{
}

void RtmpServer::start()
{
    boost::asio::ip::tcp::resolver resolver(server_.get_io_context());
    boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(FLAGS_rtmp_server_ip,
            std::to_string(FLAGS_rtmp_server_port)).begin();
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
#if (defined(unix) || defined(__unix) || defined(__unix__) || defined(__APPLE__)) && !defined(__CYGWIN__)
    typedef boost::asio::detail::socket_option::boolean<SOL_SOCKET, SO_REUSEPORT> reuse_port;
    acceptor_.set_option(reuse_port(true));
#endif
    acceptor_.bind(endpoint);
    acceptor_.listen();
    SPDLOG_INFO("RTMP server listening ({}:{})", FLAGS_rtmp_server_ip, FLAGS_rtmp_server_port);
    doAccept();
}

void RtmpServer::stop()
{
    SPDLOG_INFO("Stop RTMP server, close all clients");
    acceptor_.close();
    std::shared_ptr<RtmpSession> c = sessions_.front();
    while(c) {
        c->stop();
        c = SpIntrusiveList<RtmpSession>::next(c);
    }
    sessions_.clear();
    for(auto &s : streams_) {
        s.second->stop();
    }
    streams_.clear();
}

void RtmpServer::doAccept()
{
    acceptor_.async_accept(
    [this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
        if(!acceptor_.is_open()) {
            SPDLOG_DEBUG("RTMP server is closed, ignore new clients");
            return;
        }
        if(!ec) {
            auto c = std::make_shared<RtmpSession>(*this, std::move(socket));
            sessions_.addFront(c);
            c->start();
        }
        doAccept();
    });
}

void RtmpServer::stop(std::shared_ptr<RtmpSession> c)
{
    SPDLOG_INFO("RTMP client {} is closed", (void *)c.get());
    sessions_.erase(c);
    c->stop();
}

std::shared_ptr<Stream> RtmpServer::getStream(std::string &app, std::string &name)
{
    auto key = app + "/" + name;
    auto i = streams_.find(key);
    if(i != streams_.end()) {
        return i->second;
    }
    auto stream = std::make_shared<Stream>(app, name);
    streams_[key] = stream;
    return stream;
}

bool RtmpServer::publish(std::shared_ptr<RtmpSession> c)
{
    sessions_.erase(c);
    auto s = getStream(c->app(), c->name());
    if(s->publish(c)) {
        c->setStream(s);
        return true;
    }
    return false;
}

void RtmpServer::subscribe(std::shared_ptr<RtmpSession> c)
{
    sessions_.erase(c);
    auto s = getStream(c->app(), c->name());
    s->subscribe(c);
    c->setStream(s);
}
}
