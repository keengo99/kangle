/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 *
 * kangle web server              http://www.kangleweb.net/
 * ---------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *  See COPYING file for detail.
 *
 *  Author: KangHongjiu <keengo99@gmail.com>
 */
#include "global.h"

#include <string.h>
#include <stdlib.h>
#include <vector>
#include "http.h"
#include "log.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <sstream>
#include "cache.h"
#include "kmalloc.h"
#include "KHttpObjectHash.h"
#include "kthread.h"
#include "KObjectList.h"
#include "KCache.h"
#include "KSimulateRequest.h"
using namespace std;
void dump_object(KHttpObject *obj) {

}
int clean_cache(KReg *reg,int flag)
{
	return cache.clean_cache(reg,flag);
}
void dead_all_obj()
{
	cache.dead_all_obj();
}
void clean_static_obj_header(KHttpObject *obj) {
	assert(obj->data);
	KHttpHeader *h = obj->data->headers;
	KHttpHeader *last = NULL;
	while (h) {
		KHttpHeader *next = h->next;
		if (kgl_is_attr(h,_KS("Set-Cookie")) || kgl_is_attr(h,_KS("Set-Cookie2"))) {
			if (last) {
				last->next = next;
			} else {
				obj->data->headers = next;
			}
			xfree_header(h);
			h = next;
			continue;
		}
		last = h;
		h = next;
	}
}
bool stored_obj(KHttpObject *obj, int list_state) {
	return cache.add(obj,list_state);
}
bool stored_obj(KHttpRequest *rq, KHttpObject *obj,KHttpObject *old_obj) {
	//	printf("try stored obj now,path=%s\n",obj->url.path);
	if (obj == NULL){
		return false;
	}
	assert(!obj->in_cache);
	if (!obj->cache_is_ready) {
		return false;
	}
	if (!objCanCache(rq,obj)) {
		return false;
	}
	/*
	if (KBIT_TEST(rq->sink->data.flags,RQ_HAS_GZIP)) {
		KBIT_SET(obj->index.flags,FLAG_RQ_GZIP);
	}
	*/
	if (KBIT_TEST(rq->ctx.filter_flags, RF_NO_DISK_CACHE)) {
		KBIT_SET(obj->index.flags, FLAG_NO_DISK_CACHE);
	}
	if (rq->ctx.internal) {
		KBIT_SET(obj->index.flags,FLAG_RQ_INTERNAL);
	}
	if (KBIT_TEST(obj->index.flags,OBJ_IS_STATIC2)) {
		clean_static_obj_header(obj);
	}
	if (old_obj) {
		old_obj->Dead();
	}
	if (stored_obj(obj,(KBIT_TEST(obj->index.flags,FLAG_IN_MEM)?LIST_IN_MEM:LIST_IN_DISK))) {
		KBIT_SET(rq->sink->data.flags,RQ_OBJ_STORED);
#ifdef ENABLE_DB_DISK_INDEX
		if (KBIT_TEST(obj->index.flags, FLAG_IN_DISK)) {
			dci->start(ci_add, obj);
		}
#endif
		return true;
	}
	return false;
}
void caculateCacheSize(INT64 &csize,INT64 &cdsize,INT64 &hsize,INT64 &hdsize)
{
	csize = 0;
	cdsize = 0;
	hsize = 0;
	hdsize = 0;
	/*
	cacheLock.Lock();
	int i;
	for (i = 0; i < HASH_SIZE; i++) {
		hash_table[i].getSize(hsize, hdsize);
	}
	for (i=0;i<2;i++) {
		objList[i].getSize(csize,cdsize);
	}
	cacheLock.Unlock();
	*/
}
#ifdef ENABLE_SIMULATE_HTTP
bool cache_prefetch(const char *url)
{
	kgl_async_http ctx;
	memset(&ctx,0,sizeof(ctx));
	ctx.meth = "GET";
	ctx.url = (char *)url;
	ctx.flags = KF_SIMULATE_LOCAL|KF_SIMULATE_CACHE;
	KHttpHeader head;
	memset(&head, 0, sizeof(head));
	head.name_is_know = 1;
	head.know_header = kgl_header_accept_encoding;
	head.buf = (char *)"gzip";
	head.val_len = sizeof("gzip") - 1;
	ctx.rh = &head;
	return kgl_simuate_http_request(&ctx)==0;
}
#endif