#ifndef md5_h_slkdjfs097fd9s7df98sflsjdfjlsdf2o3u234
#define md5_h_slkdjfs097fd9s7df98sflsjdfjlsdf2o3u234
#include "kfeature.h"

typedef struct {
        uint32_t state[4];        /* state (ABCD) */
		uint32_t count[2];
        unsigned char buffer[64];     /* input buffer */
} KMD5_CTX;

typedef KMD5_CTX KMD5Context;
KBEGIN_DECLS
void KMD5Init(KMD5_CTX *);
void KMD5Update(KMD5_CTX *, const unsigned char *, unsigned int);
void KMD5Final(unsigned char[16], KMD5_CTX *);
//digest最少33个字节
void KMD5(const char *buf,int len,char *digest);
//result最少16个字节
void KMD5BIN(const char *buf,int len,char *result);
void make_digest(char *md5str, unsigned char *digest);

KEND_DECLS
#endif
