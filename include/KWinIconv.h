#ifndef KANGLE_WIN_ICONV_H
#define KANGLE_WIN_ICONV_H

// Interface implemented by src/win_iconv.cpp; no external libiconv is needed.
#include <stddef.h>
typedef void* iconv_t;
iconv_t iconv_open(const char* tocode, const char* fromcode);
int iconv_close(iconv_t cd);
size_t iconv(iconv_t cd, const char** inbuf, size_t* inbytesleft,
             char** outbuf, size_t* outbytesleft);

#endif
