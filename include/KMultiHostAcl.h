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
	std::string getHtml(KModel *model);
	KAcl *newInstance();
	const char *getName();
	bool match(KHttpRequest *rq, KHttpObject *obj);
	std::string getDisplay();
	void editHtml(std::map<std::string, std::string> &attribute,bool html);
	void buildXML(std::stringstream &s);
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
