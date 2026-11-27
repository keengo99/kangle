#ifndef KZSTD_H_99
#define KZSTD_H_99
#include "KCompressStream.h"
#include "global.h"
#ifdef ENABLE_ZSTD
#include "zstd.h"
/* content_len is the expected input size, or -1 when unknown. */
bool pipe_zstd_compress(int level, kgl_response_body* body, int64_t content_len = -1);
#endif
#endif
