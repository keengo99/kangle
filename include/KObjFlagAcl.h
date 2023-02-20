#ifndef KOBJFLAGACL_H
#define KOBJFLAGACL_H
#include "KAcl.h"
class KObjFlagAcl : public KAcl {
public:
	KObjFlagAcl() {
		flag=0;
	}
	virtual ~KObjFlagAcl() {
	}

	bool match(KHttpRequest *rq,KHttpObject *obj) override
	{
		return KBIT_TEST(obj->index.flags,flag)>0;
	}
	std::string getDisplay() override {
		std::stringstream s;
		if (KBIT_TEST(flag,ANSW_NO_CACHE)) {
			s << "nocache,";
		}
		return s.str();
	}
	void editHtml(std::map<std::string,std::string> &attribute,bool html) override {
		flag=0;
		const char *flagStr=attribute["flag"].c_str();
		char *buf = strdup(flagStr);
		char *hot = buf;
		for (;;) {
			char *p = strchr(hot,',');
			if (p) {
				*p = '\0';
			}
			if (strcasecmp(hot,"nocache")==0) {
				KBIT_SET(flag,ANSW_NO_CACHE);
			}
			if (p==NULL) {
				break;
			}
			hot = p+1;
		}
		free(buf);
	}
	std::string getHtml(KModel *model) override {
		std::stringstream s;
		s << "<input type=text name=flag value='";
		if (model) {
			s << model->getDisplay();
		}

		s << "'>(available:nocache)";
		return s.str();
	}
	KAcl *new_instance() override {
		return new KObjFlagAcl();
	}
	const char *getName() override {
		return "obj_flag";
	}
public:
	void buildXML(std::stringstream &s) override {
		s << " flag='" << getDisplay() << "'>";
	}
private:
	int flag;
};
#endif
