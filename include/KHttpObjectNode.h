/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 *
 * kangle web server              http://www.kanglesoft.com/
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
#ifndef KHTTPOBJECTNODE_H_
#define KHTTPOBJECTNODE_H_
#include "KHttpObject.h"
#include "log.h"
#include "http.h"
#include "krbtree.h"
#if 0
class KHttpObjectNode
{
public:
	inline KHttpObjectNode(KUrl *rqUrl){
		head=NULL;
		rqUrl->clone_to(&url);
		//url_decode(url.path,0,NULL);
		//KFileName::tripDir3(url.path, '/');
	};
	virtual ~KHttpObjectNode(){
		url.destroy();
//		delete url;
	};
	inline int purge()
	{
		int count = 0;
		KHttpObject *obj=head;
		while (obj) {
			if (!TEST(obj->index.flags,FLAG_DEAD)) {
				SET(obj->index.flags,FLAG_DEAD);
				count++;
				klog(KLOG_NOTICE,"%s%s purged\n",url.host,url.path);
			}
			obj = obj->next;
		}
		return count;
	}
	inline KHttpObject *get(bool gzip,bool internal)
	{
		KHttpObject *obj=head;
		while (obj) {
			if (!TEST(obj->index.flags,FLAG_DEAD)) {
				if (TEST(obj->index.flags,FLAG_BIG_OBJECT) 
					|| (gzip == (bool)(TEST(obj->index.flags,FLAG_RQ_GZIP)>0)
						&& internal == TEST(obj->index.flags,FLAG_RQ_INTERNAL)>0)
					){
					//大物件，或者是普通物件(但要对到gzip和internal)
					return obj;
				}
			}
			obj=obj->next;
		}	
		return NULL;
	}
	bool remove(KHttpObject *obj)
	{
		KHttpObject *next=head;
		KHttpObject *last=NULL;
		while(next){
			if(next == obj){
				if(last){
					last->next = obj->next;
				}else{
					assert(head == obj);
					head = obj->next;
				}
				return true;
			}
			last=next;
			next=next->next;
		}
		assert(false);
		return false;
	}
	void put(KHttpObject *obj)
	{
		obj->next = head;
		if (!TEST(obj->index.flags,FLAG_URL_FREE)) {
			obj->url = &url;
		}
		head = obj;
	}
	void putEnd(KHttpObject *obj)
	{
		KHttpObject *last=head;
		if(last == NULL){
			put(obj);
			return;
		}
		if (!TEST(obj->index.flags,FLAG_URL_FREE)) {
			obj->url = &url;
		}
		KHttpObject *prev;
		do{
			prev=last;
			last=last->next;
		}while(last);
		prev->next=obj;	
		obj->next=NULL;
	}
	bool isEmpty()
	{
		return (head==NULL);
	}
	int getCount()
	{
		int count=0;
		KHttpObject *obj = head;
		while(obj){
			count++;
			obj=obj->next;
		}
		return count;
	}
	friend class KHttpObjectHash;
	KUrl url;
	KHttpObject *head;
};
#endif
#endif /*KHTTPOBJECTNODE_H_*/
