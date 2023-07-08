#include <spdlog/spdlog.h>
#include "Server.hpp"
#include "Conf.hpp"

spdlog::level::level_enum getLogLevel()
{
    if(FLAGS_log_level == "debug") {
        return spdlog::level::level_enum::debug;
    } else if(FLAGS_log_level == "info") {
        return spdlog::level::level_enum::info;
    } else if(FLAGS_log_level == "warn") {
        return spdlog::level::level_enum::warn;
    } else if(FLAGS_log_level == "error") {
        return spdlog::level::level_enum::err;
    } else if(FLAGS_log_level == "critical") {
        return spdlog::level::level_enum::critical;
    } else if(FLAGS_log_level == "off") {
        return spdlog::level::level_enum::off;
    } else {
        return spdlog::level::level_enum::info;
    }
}

int main(int argc, char **argv)
{
    gflags::SetUsageMessage("Usage: ms777 --flagfile=ms777.conf\n");
    gflags::SetVersionString("1.0");
    google::ParseCommandLineFlags(&argc, &argv, true);
    try {
        spdlog::set_level(getLogLevel());
        ms777::Server().run();
    } catch(std::exception &e) {
        SPDLOG_ERROR("Server exception: {}", e.what());
    }
    google::ShutDownCommandLineFlags();
    SPDLOG_INFO("Bye");
    spdlog::shutdown();
    return 0;
}
