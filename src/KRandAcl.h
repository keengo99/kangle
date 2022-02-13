#ifndef KRANDACL_H
#define KRANDACL_H
#include "KAcl.h"
class KRandAcl : public KAcl
{
public:
	KRandAcl()
	{
		count = 0;
		total = 1;
	}
	~KRandAcl()
	{
	}
	std::string getHtml(KModel *model) {
		std::stringstream s;
		s << "<input name='rand' value='";
		KRandAcl *urlAcl = (KRandAcl *) (model);
		if (urlAcl) {
			s << urlAcl->getDisplay();
		}
		s << "'>";
		return s.str();
	}
	KAcl *newInstance() {
		return new KRandAcl();
	}
	const char *getName() {
		return "rand";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		return (rand() % total) < count;
	}
	std::string getDisplay() {
		std::stringstream s;
		s << count << "/" << total;
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html){
		setRand(attribute["rand"].c_str());
	}
	void buildXML(std::stringstream &s) {
		s << " rand='" << getDisplay() << "'>";
	}
private:
	void setRand(const char *str)
	{
		count = atoi(str);
		total = 0;
		const char *p = strchr(str,'/');
		if (p) {
			total = atoi(p+1);
		}
		if (total==0) {
			total = 1;
		}
	}
	int count;
	int total;
};
#endif
