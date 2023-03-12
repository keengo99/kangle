#ifndef KSRCSACL_H
#define KSRCSACL_H
#include "KMultiAcl.h"
#include "KXml.h"
#include "utils.h"
#include "KIpMap.h"
class KSrcsAcl: public KAcl {
public:
	KSrcsAcl() {
		split = '|';
	}
	virtual ~KSrcsAcl() {
	}
	KAcl *new_instance() override {
		return new KSrcsAcl();
	}
	const char *getName() override {
		return "srcs";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) override {
		return im.find(rq->getClientIp()) != NULL;
	}
	void get_html(KModel *model,KWStream &s) override {
		s << "<input placeholder='format: 127.0.0.1|127.0.0.0/24|127.0.0.1-127.0.0.255' name=v size=50 value='";
		KSrcsAcl *acl = (KSrcsAcl *)(model);
		if (acl) {
			acl->getValList(s);
		}
		s << "'>";		
		s << "split:<input name=split max=1 size=1 value='";
		if (acl == NULL) {
			s << "|";
		} else {
			s << acl->split;
		}
		s << "'>";
	}
	void getValList(KWStream &s) {
		im.dump_addr(s, this->split);
	}
	void get_display(KWStream& s) override {
		getValList(s);
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		im.clear();
		if (attribute["split"].size() > 0) {
			split = attribute["split"][0];
		}
		else {
			split = '|';
		}
		if (attribute["v"].size() > 0) {
			im.add_multi_addr(attribute["v"].c_str(), split, (void *)1);
		}
	}
protected:
	char split;
	KIpMap im;
};
#endif
