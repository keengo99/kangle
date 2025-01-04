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

class KApiRedirect final: public KRedirect,public KExtendProgram {
public:
	KApiRedirect(const KString &name);
	const char *getName()
	{
		return name.c_str();
	}
	bool parse_config(const khttpd::KXmlNode* node) override;
	KRedirectSource*makeFetchObject(KHttpRequest *rq, KFileName *file) override;
	bool unload();	
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
	bool load();
	void setFile(const KString& file);
	void dump(kgl::serializable* sl);
public:
	KString apiFile;
	KApiDso dso;
protected:
	~KApiRedirect();
private:
	KMutex lock;
};
#endif /* KAPIREDIRECT_H_ */
