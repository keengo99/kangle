#ifndef KMULTIHOSTACL_H_
#define KMULTIHOSTACL_H_
#include <map>
#include "KAcl.h"
#include "KLineFile.h"
#include "utils.h"

class KMultiHostAcl: public KAcl {
public:
	KMultiHostAcl();
	virtual ~KMultiHostAcl();
	std::string getHtml(KModel *model) override;
	KAcl *new_instance() override;
	const char *getName() override;
	bool match(KHttpRequest *rq, KHttpObject *obj) override;
	std::string getDisplay() override;
	void editHtml(std::map<std::string, std::string> &attribute,bool html) override;
	void buildXML(std::stringstream &s) override;
private:
	bool loadFile(KHttpRequest *rq);
	void freeMap();
	std::string file;
	time_t lastModified;
	time_t lastCheck;
	std::map<char *, bool,lessp_icase > m;
	KMutex lock;
	OpenState lastState;
};

#endif /* KMULTIHOSTACL_H_ */
