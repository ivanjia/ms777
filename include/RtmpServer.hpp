#pragma once
#include <unordered_map>
#include <string>
#include <utility>
#include "RtmpSession.hpp"
#include "Stream.hpp"

namespace ms777 {
class Server;

class RtmpServer
{
public:
    RtmpServer(Server &server);
    ~RtmpServer();

    void start();
    void stop();
    void stop(std::shared_ptr<RtmpSession> c);
    bool publish(std::shared_ptr<RtmpSession> c);
    void subscribe(std::shared_ptr<RtmpSession> c);

private:
    void doAccept();
    std::shared_ptr<Stream> getStream(std::string &app, std::string &name);

private:
    Server &server_;
    boost::asio::ip::tcp::acceptor acceptor_;
    SpIntrusiveList<RtmpSession> sessions_;
    std::unordered_map<std::string, std::shared_ptr<Stream>> streams_;
};
}
