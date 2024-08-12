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
#ifndef cache_h_saldkfjsaldkfjalsfdjasdf987a9sd7f9adsf7
#define cache_h_saldkfjsaldkfjalsfdjasdf987a9sd7f9adsf7

#include <string>
#include <assert.h>
#include "global.h"
#include "KMutex.h"
#include "KHttpObject.h"
#include "KVirtualHost.h"
#include "KObjectList.h"
#include "KHttpObjectHash.h"
#include "do_config.h"
#include "KCache.h"
//16  32   64   128  256  512   1024  2048  4096
//0xF 0x1F 0x3F 0x7F 0xFF 0x1FF 0x3FF 0x7FF 0xFFF
#define CACHE_DIR_MASK1    0x7F
#define CACHE_DIR_MASK2    0x7F
//#define CACHE_DIR_MASK1    0x1
//#define CACHE_DIR_MASK2    0x1


bool saveCacheIndex();
void init_cache();
void release_obj(KHttpObject *obj);
void dead_all_obj();
void caculateCacheSize(INT64 &csize,INT64 &cdsize,INT64 &hsize,INT64 &hdsize);

//内容过滤链更新时
//void change_content_filter(int flag = GLOBAL_KEY_CHECKED, KVirtualHost *vh =
//		NULL);
inline bool objCanCache(KHttpRequest *rq,KHttpObject *obj)
{
	if (conf.default_cache == 0) {
		//默认不缓存并且也没有说明要缓存的
		return false;
	}
	if (KBIT_TEST(rq->sink->data.flags,RQ_HAS_AUTHORIZATION)) {
		//如果是有认证用户的必须要回源验证。
		KBIT_SET(obj->index.flags,OBJ_MUST_REVALIDATE);
	}
	if (KBIT_TEST(obj->index.flags,FLAG_DEAD|ANSW_NO_CACHE)) {
		//死物件和标记为不缓存的
		return false;
	}
	return true;
}
inline KHttpObject * find_cache_object(KHttpRequest *rq, bool create_flags) {
	u_short url_hash = cache.hash_url(rq->sink->data.url);
	KHttpObject *obj = cache.find(rq,url_hash);
	if (obj == NULL && create_flags) {
		obj = new KHttpObject(rq);
		//cache the url_hash
		assert(obj->in_cache==0);
		obj->h = url_hash;
	}
	return obj;
}
inline void release_obj(KHttpObject *obj) {
	obj->release();
}
int clean_cache(KReg *reg,int flag);
inline int clean_cache(const char *str,bool wide)
{
	KSafeUrl url(new KUrl());
	if (!parse_url(str,url.get())) {
		return 0;
	}
	int count = cache.clean_cache(url.get(),wide);
	return count;
}
inline int get_cache_info(const char *str,bool wide,KCacheInfo *ci)
{
	KSafeUrl url(new KUrl());
	if (!parse_url(str,url.get())) {
		return 0;
	}
	int count = cache.get_cache_info(url.get(),wide,ci);
	return count;
}
bool cache_prefetch(const char *url);
#endif
