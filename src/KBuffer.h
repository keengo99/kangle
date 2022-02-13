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
#ifndef KBUFFER_H
#define KBUFFER_H
#include "kbuf.h"
#include "ksocket.h"
#include "KSendable.h"
#include "KStream.h"
#include "KAutoBuffer.h"
#include "kmalloc.h"
#define 	CHUNK		NBUFF_SIZE
inline kbuf * new_buff(char *data,int len)
{
	kbuf *b = (kbuf *)xmalloc(sizeof(kbuf));
	b->used = len;
	b->flags = 0;
	b->data = data;
	return b;
}
#define KBuffer KAutoBuffer
#endif

