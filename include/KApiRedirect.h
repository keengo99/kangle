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
	KFetchObject *makeFetchObject(KHttpRequest *rq, KFileName *file) override;
	bool load();
	void setFile(std::string file);
	bool load(std::string file);
	KUpstream* GetUpstream(KHttpRequest* rq) override;	

	const char *getType() override {
		return "api";
	}
	bool createProcess(KVirtualHost *vh, KPipeStream *st);
	bool isChanged(KExtendProgram *rd)
	{
		//todo:当前api不支持重新加载。
		return false;
	}
public:
	void buildXML(std::stringstream &s) override;
public:
	std::string apiFile;
	KApiDso dso;
private:
	KMutex lock;
};
#endif /* KAPIREDIRECT_H_ */
