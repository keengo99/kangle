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
	std::string getHtml(KModel *model) override {
		std::stringstream s;
		s << "<input name='rand' value='";
		KRandAcl *urlAcl = (KRandAcl *) (model);
		if (urlAcl) {
			s << urlAcl->getDisplay();
		}
		s << "'>";
		return s.str();
	}
	KAcl *new_instance() override {
		return new KRandAcl();
	}
	const char *getName() override {
		return "rand";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) override {
		return (rand() % total) < count;
	}
	std::string getDisplay() override {
		std::stringstream s;
		s << count << "/" << total;
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html) override {
		setRand(attribute["rand"].c_str());
	}
	void buildXML(std::stringstream &s) override {
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
