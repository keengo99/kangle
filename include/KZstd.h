#ifndef KZSTD_H_99
#define KZSTD_H_99
#include "KCompressStream.h"
#include "global.h"
#ifdef ENABLE_ZSTD
#include "zstd.h"
bool pipe_zstd_compress(int level, kgl_response_body* body);
#endif
#endif
