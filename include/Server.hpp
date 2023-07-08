#pragma once
#include <boost/asio.hpp>

namespace ms777 {
class Server
{
public:
    Server();
    ~Server();

    void run();

    boost::asio::io_context &get_io_context();

private:
    void stop();

private:
    boost::asio::io_context io_context_;
};
}
