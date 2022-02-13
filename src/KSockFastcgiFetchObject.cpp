/*
 * KSockFastcgiFetchObject.cpp
 *
 *  Created on: 2010-6-15
 *      Author: keengo
 */
#include "do_config.h"
#include "KSockFastcgiFetchObject.h"
#if 0
KSockFastcgiFetchObject::KSockFastcgiFetchObject(KPoolableRedirect *as, KFileName *file) {
	this->as = as;
	as->addRef();
	this->file = file;
}

KSockFastcgiFetchObject::~KSockFastcgiFetchObject() {
	if (as) {
		as->release();
	}
}
bool KSockFastcgiFetchObject::createConnection(KHttpRequest *rq,
		KHttpObject *obj, bool mustCreate) {
	client = as->createConnection(rq, mustCreate);
	return client != NULL;

}
bool KSockFastcgiFetchObject::freeConnection(KHttpRequest *rq, bool realClose) {
	if(client){
		//as->freeConnection((KClientPoolSocket *) client, realClose);
		//client = NULL;
		delete client;
	}
	return true;
}
#endif
