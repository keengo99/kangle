/*
 * KWebDavService.h
 *
 *  Created on: 2010-8-7
 *      Author: keengo
 */
#ifndef KWEBDAVSERVICE_H_
#define KWEBDAVSERVICE_H_
#include "KISAPIServiceProvider.h"
#include "KResource.h"
#include "KXmlDocument.h"

class KLockToken;
class KWebDavService {
public:
	KWebDavService();
	virtual ~KWebDavService();
	bool service(KISAPIServiceProvider*provider);
	bool doOptions();
	bool doPut();
	bool doLock();
	bool doUnlock();
	bool doMkcol();
	bool doProppatch();
	bool doPropfind();
	bool doMove();
	bool doCopy();
	bool doDelete();
	bool doGet(bool head);
private:
	bool send(int status_code);
	bool writeResourceProp(KResource *rs);
	bool listResourceProp(KResource *rs,int depth);
	bool writeXmlHeader();
	bool response_lock_body(KLockToken* token);
	KISAPIServiceProvider *provider;
	bool parseDocument(khttpd::KXmlDocument &document);
	uint16_t check_file_locked(KLockToken* lock_token);
	const char *getIfToken();
	char *if_token;

};

#endif /* KWEBDAVSERVICE_H_ */
