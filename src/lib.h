/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */
#if	!defined(_LIB_H_INCLUDED_)
#define _LIB_H_INCLUDED_
#include <string>
#include <stdlib.h>
#include "global.h"
#include "kforwin32.h"
#include "log.h"
char *base64_encode(char*);
char *base64_decode(char*);
char *url_encode(const char *s, size_t len, size_t *new_length);
char *url_value_encode(const char *s, size_t len, size_t *new_length);
std::string url_encode(const char *str, size_t len_string);
void my_msleep(int msec);
time_t parse1123time(const char *str);
const char *mk1123time(time_t time, char *buf, int size);
void makeLastModifiedTime(time_t *a, char *b, size_t l);
void my_sleep(int);
void init_time_zone();
u_short string_hash(const char *str, u_short res = 1);

/*
 * 18446744073709551616
 * buf min size length is 22
 */
extern int program_rand_value;
extern int open_file_limit;
#endif	/* !_LIB_H_INCLUDED_ */
