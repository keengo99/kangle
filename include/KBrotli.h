#ifndef KBROTLI_H_99
#define KBROTLI_H_99
#include "KCompressStream.h"
#include "global.h"
#ifdef ENABLE_BROTLI
#include "brotli/encode.h"
bool pipe_brotli_compress(int level, kgl_response_body* body);
#endif
#endif
