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
	void get_html(KWStream& s) override;
	void get_display(KWStream& s) override;
	KAcl *new_instance() override;
	const char *get_module() const override;
	bool match(KHttpRequest* rq, KHttpObject* obj) override;
	void parse_config(const khttpd::KXmlNodeBody* xml) override;
private:
	bool loadFile(KHttpRequest *rq);
	void freeMap();
	KString file;
	time_t lastModified;
	time_t lastCheck;
	std::map<char *, bool,lessp_icase > m;
	KMutex lock;
	OpenState lastState;
};

#endif /* KMULTIHOSTACL_H_ */
