/*
 * KRedirect.h
 *
 *  Created on: 2010-6-12
 *      Author: keengo
 */

#ifndef KREDIRECT_H_
#define KREDIRECT_H_
#include "KFileName.h"
#include "KJump.h"
#include "ksocket.h"
#include "kselector.h"

class KHttpRequest;
class KAsyncFetchObject;
class KRedirectSource;
class KUpstream;
class KRedirect: public KJump {
public:
	KRedirect();
	virtual ~KRedirect();
	virtual KRedirectSource*makeFetchObject(KHttpRequest *rq, KFileName *file) = 0;
	virtual KUpstream *GetUpstream(KHttpRequest *rq)
	{
		return nullptr;
	}
	void setEnable() {
		enable = true;
	}
	void setDisable() {
		enable = false;
	}
	virtual const char *getType() = 0;
	/*
	   get the redirect infomation.
	*/
	virtual const char *getInfo()
	{
		return "";
	}
	bool enable;
	/*
	 * 是否外来扩展，如果是就不用保存在config.xml
	 */
	bool ext;
};

#endif /* KREDIRECT_H_ */
