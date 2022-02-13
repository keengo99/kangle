/*
 * KApiFastcgiFetchObject.cpp
 *
 *  Created on: 2010-6-15
 *      Author: keengo
 */
#include <vector>
#include "KApiFastcgiFetchObject.h"
#include "extern.h"
#include "KVirtualHost.h"
#if 0
KApiFastcgiFetchObject::KApiFastcgiFetchObject(KApiRedirect *rd,
		KFileName *file) {
	this->rd = rd;
	rd->addRef();
	this->file = file;
}
KApiFastcgiFetchObject::~KApiFastcgiFetchObject() {
	if (rd) {
		rd->release();
	}
}
bool KApiFastcgiFetchObject::createConnection(KHttpRequest *rq,
		KHttpObject *obj, bool mustCreate) {
	/*
	 * 我们忽略mustCreate参数，这种链接是可靠的,不比socket.
	 */
	client = rd->createConnection(rq);
	if (client == NULL) {
		return false;
	}
	client->expireTime = 0;
#ifndef _WIN32
#ifdef ENABLE_VH_RUN_AS
	if (my_uid == 0 && rq->svh) {
		chrooted = rq->svh->vh->chroot;
	}
#endif
#endif
	return true;
}
bool KApiFastcgiFetchObject::freeConnection(KHttpRequest *rq, bool realClose) {
	if(client){
		rd->freeConnection(rq, (KPipeStream *) client, realClose);
		client = NULL;
	}
	return true;
}

#endif
