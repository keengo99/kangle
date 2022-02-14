/*
 * KApiRedirect.h
 *
 *  Created on: 2010-6-13
 *      Author: keengo
 */

#ifndef KAPIREDIRECT_H_
#define KAPIREDIRECT_H_
#include <map>
#include <list>
#include <string.h>
#include "KRedirect.h"

#include "KMutex.h"
#include "KPipeStream.h"
#include "KExtendProgram.h"
#include "ksocket.h"
#include "KApiDso.h"
#include <string>

class KApiRedirect: public KRedirect,public KExtendProgram {
public:
	KApiRedirect();
	virtual ~KApiRedirect();

	const char *getName()
	{
		return name.c_str();
	}
	KFetchObject *makeFetchObject(KHttpRequest *rq, KFileName *file);
	bool load();
	void setFile(std::string file);
	bool load(std::string file);

	//KClientSocket *createConnection(KHttpRequest *rq);
	//void freeConnection(KHttpRequest *rq, KClientSocket *st, bool mustClose);

	const char *getType() {
		return "api";
	}
	bool createProcess(KVirtualHost *vh, KPipeStream *st);
	bool isChanged(KExtendProgram *rd)
	{
		//todo:当前api不支持重新加载。
		return false;
	}
public:
	void buildXML(std::stringstream &s);
public:

	void setDelayLoad(bool delayLoad)
	{
		this->delayLoad = delayLoad;
	}

public:
	std::string apiFile;
	KApiDso dso;
//public:
	//KPipeStream *createPipeStream(KVirtualHost *vh);
private:
	KMutex lock;
	bool delayLoad;

	/*
	是否是whm模块。
	*/
};
#endif /* KAPIREDIRECT_H_ */
