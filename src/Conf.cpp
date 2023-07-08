#include "Conf.hpp"

DEFINE_string(log_level, "info", "log level (debug, info, warn, error, critical, off)");

DEFINE_string(rtmp_server_ip, "0.0.0.0", "rtmp server ip address");
DEFINE_int32(rtmp_server_port, 1935, "rtmp server port");
DEFINE_uint32(rtmp_read_buffer_size, 8192, "rtmp buffer size");
DEFINE_uint32(rtmp_chunk_size, 4096, "rtmp chunk size");
DEFINE_bool(rtmp_gop_cache, true, "rtmp enable GOP cache");
