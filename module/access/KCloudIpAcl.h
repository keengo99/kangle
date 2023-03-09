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
	std::string getHtml(KModel *model) override;
	KAcl *new_instance() override;
	const char *getName() override;
	bool match(KHttpRequest *rq, KHttpObject *obj) override;
	std::string getDisplay() override;
	void parse_config(const khttpd::KXmlNodeBody* xml) override;
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
