#ifndef KCLOUDIPACL_H
#define KCLOUDIPACL_H
#include <string>
#include "KAcl.h"
#include "KIpMap.h"
#include "utils.h"
#ifdef ENABLE_SIMULATE_HTTP
class KCloudIpAcl: public KAcl {
public:
	KCloudIpAcl();
	virtual ~KCloudIpAcl();
	std::string getHtml(KModel *model);
	KAcl *newInstance();
	const char *getName();
	bool match(KHttpRequest *rq, KHttpObject *obj);
	std::string getDisplay();
	void editHtml(std::map<std::string, std::string> &attribute,bool html);
	void buildXML(std::stringstream &s);
	void start();
	void start_http();
	void http_body_hook(const char *data, int len);
private:
	std::string url;
	int flush_time;
	bool started;
	KStringBuf data;
	char *sign;
	KIpMap *im;
	KMutex lock;
	bool parse_data();
};
#endif
#endif
