#include <spdlog/spdlog.h>
#include "Server.hpp"
#include "RtmpServer.hpp"

namespace ms777 {
Server::Server()
    : io_context_(1)
{
}

Server::~Server()
{
    SPDLOG_INFO("Server exit");
}

void Server::run()
{
    RtmpServer rtmpServer(*this);
    boost::asio::signal_set signals(io_context_);
    signals.add(SIGINT);
    signals.add(SIGTERM);
#if defined(SIGQUIT)
    signals.add(SIGQUIT);
#endif
    signals.async_wait(
    [this, &rtmpServer](std::error_code /*ec*/, int signo) {
        SPDLOG_INFO("Stop server by signal {}", signo);
        rtmpServer.stop();
    });
    rtmpServer.start();
    io_context_.run();
}

boost::asio::io_context &Server::get_io_context()
{
    return io_context_;
}
}
