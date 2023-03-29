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
	void get_display(KWStream& s) override;
	void get_html(KModel* model, KWStream& s) override;
	KAcl *new_instance() override;
	const char *getName() override;
	bool match(KHttpRequest* rq, KHttpObject* obj) override;
	void parse_config(const khttpd::KXmlNodeBody* xml) override;
	void start();
	void start_http();
	void http_body_hook(const char *data, int len);
private:
	KString url;
	int flush_time;
	bool started;
	KStringStream data;
	kgl_auto_cstr sign;
	KIpMap *im;
	KMutex lock;
	bool parse_data();
};
#endif
#endif
