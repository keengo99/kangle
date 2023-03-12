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
#include "extern.h"

class KHttpRequest;
class KAsyncFetchObject;
class KRedirectSource;
class KUpstream;
class KRedirect: public KJump {
public:
	KRedirect();
	KRedirect(const KString& name) : KJump(name) {
	
	}
	KRedirect(KString&& name) : KJump(name) {
	
	}
	
	virtual KRedirectSource*makeFetchObject(KHttpRequest *rq, KFileName *file) = 0;
	virtual KUpstream *GetUpstream(KHttpRequest *rq)
	{
		return nullptr;
	}
	void set_enable(bool enable) {
		
	}
	bool is_disable() {
		return false;
	}
	virtual const char *getType() = 0;
	/*
	   get the redirect infomation.
	*/
	virtual const char *getInfo()
	{
		return "";
	}
protected:
	virtual ~KRedirect();
};

#endif /* KREDIRECT_H_ */
