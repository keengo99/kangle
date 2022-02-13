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
	KAcl *newInstance() {
		return new KSrcsAcl();
	}
	const char *getName() {
		return "srcs";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		return im.find(rq->getClientIp()) != NULL;
	}
	std::string getHtml(KModel *model) {
		std::stringstream s;
		s << "<input placeholder='format: 127.0.0.1|127.0.0.0/24|127.0.0.1-127.0.0.255' name=v size=50 value='";
		KSrcsAcl *acl = (KSrcsAcl *)(model);
		if (acl) {
			s << acl->getValList();
		}
		s << "'>";		
		s << "split:<input name=split max=1 size=1 value='";
		if (acl == NULL) {
			s << "|";
		} else {
			s << acl->split;
		}
		s << "'>";
		return s.str();
	}
	std::string getValList() {
		std::stringstream s;
		im.dump_addr(s, this->split);
		return s.str();
	}
	std::string getDisplay() {
		std::stringstream s;
		s << getValList();
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html){
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
	bool startCharacter(KXmlContext *context, char *character, int len) {
		if (len > 0) {
			im.clear();
			im.add_multi_addr(character, split, (void *)1);
		}
		return true;
	}
	void buildXML(std::stringstream &s) {
		s << " split='" << split << "'>" << getValList();
	}
protected:
	char split;
	KIpMap im;
};
#endif
