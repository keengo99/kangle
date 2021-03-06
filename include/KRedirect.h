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
class KFetchObject;
class KUpstream;
class KRedirect: public KJump {
public:
	KRedirect();
	virtual ~KRedirect();
	virtual KFetchObject *makeFetchObject(KHttpRequest *rq, KFileName *file) = 0;
	//virtual kev_result connect(KHttpRequest *rq, KAsyncFetchObject *fo) = 0;
	virtual KUpstream *GetUpstream(KHttpRequest *rq)
	{
		return NULL;
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
