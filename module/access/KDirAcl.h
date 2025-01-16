#ifndef KDIRACL_H
#define KDIRACL_H
#include "KMultinAcl.h"
#include "KXml.h"
#include "utils.h"
class KDirAcl: public KMultinAcl {
public:
	KDirAcl() {
		icase_can_change = false;
#ifdef _WIN32
		seticase(true);
#else
		seticase(false);
#endif
	}
	virtual ~KDirAcl() {
	}
	KAcl *new_instance() override {
		return new KDirAcl();
	}
	const char *get_module() const override {
		return "dir";
	}
	bool match(KHttpRequest* rq, KHttpObject* obj) override {
		if (rq->file==NULL) {
			return false;
		}
		return KMultinAcl::match(rq->file->getName());
	}
protected:
	char *transferItem(char *file) override
	{
		KFileName::tripDir3(file,PATH_SPLIT_CHAR);
		return file;
	}
};
#endif
