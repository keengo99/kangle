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
#ifndef KFILEEXEACL_H_
#define KFILEEXEACL_H_

#include "KMultiAcl.h"
#include "KReg.h"
#include "KXml.h"
#include "utils.h"
class KFileExeAcl : public KMultiAcl {
public:
	KFileExeAcl() {
		icase_can_change = true;
#ifndef _WIN32
		seticase(false);
#endif
	}
	virtual ~KFileExeAcl() {
	}
	KAcl *newInstance() {
		return new KFileExeAcl();
	}
	const char *getName() {
		return "file_ext";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		if (rq->file) {
			return KMultiAcl::match(rq->file->getExt());
		}
		return checkPath(rq->url->path);
	}
private:
	bool checkPath(const char *path) {
		bool result=false;
		assert(path);
		const char *p=strrchr(path, '.');
		if (p) {
			p++;
			if (strchr(p, '/')==NULL) {
				result=KMultiAcl::match(p);
			}
		}
		return result;
	}
};
class KFileNameAcl : public KMultiAcl {
public:
	KFileNameAcl() {
		icase_can_change = true;
#ifndef _WIN32
		seticase(false);
#endif
	}
	virtual ~KFileNameAcl() {
	}
	KAcl *newInstance() {
		return new KFileNameAcl();
	}
	const char *getName() {
		return "filename";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		if (rq->file==NULL) {
			return false;
		}
		const char *filename = rq->file->getName();
		const char *p = strrchr(filename,PATH_SPLIT_CHAR);
		if(p==NULL){
			p = filename;
		} else {
			p++;
		}
		return KMultiAcl::match(p);		
	}
};
#endif /*KFILEEXEACL_H_*/
