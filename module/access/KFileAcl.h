#ifndef KFILEACL_H_
#define KFILEACL_H_

/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */
#include "KMultiAcl.h"
#include "KReg.h"
#include "KXml.h"
#include "utils.h"
class KFileAcl: public KMultiAcl {
public:
	KFileAcl() {
		icase_can_change = false;
#ifndef _WIN32
		seticase(false);
#endif
	}
	virtual ~KFileAcl() {
	}
	KAcl *newInstance() {
		return new KFileAcl();
	}
	const char *getName() {
		return "file";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		if (rq->file==NULL) {
			return false;
		}
		return KMultiAcl::match(rq->file->getName());
	}
protected:
	char *transferItem(char *file)
	{
		KFileName::tripDir3(file,PATH_SPLIT_CHAR);
		return file;
	}
};

#endif /*KFILEEXEACL_H_*/
