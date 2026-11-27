#ifndef KBROTLI_H_99
#define KBROTLI_H_99
#include "KCompressStream.h"
#include "global.h"
#ifdef ENABLE_BROTLI
#include "brotli/encode.h"
/* content_len is the expected input size, or -1 when unknown. */
bool pipe_brotli_compress(int level, kgl_response_body* body, int64_t content_len = -1);
#endif
#endif
