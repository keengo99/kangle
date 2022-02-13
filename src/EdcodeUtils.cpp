#include <string>
#include <sstream>
#include <string.h>
#include <stdlib.h>
#include "kmalloc.h"
static const char *b64alpha =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
//static unsigned char hexchars[] = "0123456789ABCDEF";
#define B64PAD '='
using namespace std;

unsigned int str_chr(const char *s, int c) {
	register char ch;
	register const char *t;

	ch = c;
	t = s;
	for (;;) {
		if (!*t)
			break;
		if (*t == ch)
			break;
		++t;
		if (!*t)
			break;
		if (*t == ch)
			break;
		++t;
		if (!*t)
			break;
		if (*t == ch)
			break;
		++t;
		if (!*t)
			break;
		if (*t == ch)
			break;
		++t;
	}
	return t - s;
}

char * b64decode(const unsigned char *in, int *l) {
	int i, j;
//	int len;
	unsigned char a[4];
	unsigned char b[3];
	char *s;
	if (*l <= 0) {
		*l = strlen((const char *) in);
	}
	char *out = (char *) xmalloc(2*(*l)+2);
	if (out == NULL) {
		return NULL;
	}
	s = out;
	for (i = 0; i < *l; i += 4) {
		for (j = 0; j < 4; j++)
			if ((i + j) < *l && in[i + j] != B64PAD) {
				a[j] = str_chr(b64alpha, in[i + j]);
				if (a[j] > 63) {
					//		printf("bad char=%c,j=%d\n",a[j],j);
					free(out);
					return NULL;
					//	return -1;
				}
			} else{
				a[j] = 0;
			}

		b[0] = (a[0] << 2) | (a[1] >> 4);
		b[1] = (a[1] << 4) | (a[2] >> 2);
		b[2] = (a[2] << 6) | (a[3]);

		*s = b[0];
		s++;

		if (in[i + 1] == B64PAD)
			break;
		*s = b[1];
		s++;

		if (in[i + 2] == B64PAD)
			break;
		*s = b[2];
		s++;
	}

	*l = s - out;
	//  printf("len=%d\n",len);
	while (*l && !out[*l - 1])
		--*l;
	out[*l] = 0;
	//	string result = out;
	//	free(out);
	return out;
	// return len;

	//  return s.str();
}

string b64encode(const unsigned char *in, int len)
/* not null terminated */
{
	unsigned char a, b, c;
	int i;
	// char *s;
	stringstream s;
	if (len == 0) {
		len = strlen((const char *) in);
	}

	// if (!stralloc_ready(out,in->len / 3 * 4 + 4)) return -1;
	// s = out->s;
	for (i = 0; i < len; i += 3) {
		a = in[i];
		b = i + 1 < len ? in[i + 1] : 0;
		c = i + 2 < len ? in[i + 2] : 0;

		s << b64alpha[a >> 2];
		s << b64alpha[((a & 3) << 4) | (b >> 4)];

		if (i + 1 >= len)
			s << B64PAD;
		else
			s << b64alpha[((b & 15) << 2) | (c >> 6)];

		if (i + 2 >= len)
			s << B64PAD;
		else
			s << b64alpha[c & 63];
	}
	return s.str();
}
