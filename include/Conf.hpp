#pragma once
#define STRIP_FLAG_HELP 1
#include <gflags/gflags.h>

DECLARE_string(log_level);

DECLARE_string(rtmp_server_ip);
DECLARE_int32(rtmp_server_port);
DECLARE_uint32(rtmp_read_buffer_size);
DECLARE_uint32(rtmp_chunk_size);
DECLARE_bool(rtmp_gop_cache);
