/*
 * KWebDavService.h
 *
 *  Created on: 2010-8-7
 *      Author: keengo
 */
#ifndef KWEBDAVSERVICE_H_
#define KWEBDAVSERVICE_H_
#include "KServiceProvider.h"
#include "KResource.h"
#include "KXmlDocument.h"
class KWebDavService {
public:
	KWebDavService();
	virtual ~KWebDavService();
	bool service(KServiceProvider *provider);
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
	KServiceProvider *provider;
	bool parseDocument(KXmlDocument &document);
	const char *getIfToken();
	char *if_token;

};

#endif /* KWEBDAVSERVICE_H_ */
