#ifndef KHttpManageInclude_randomsdflisf97sd9f09s8df
#define KHttpManageInclude_randomsdflisf97sd9f09s8df

#include <map>
#include <string>
#include "global.h"
#include "do_config.h"
#include "log.h"
#include "http.h"
#include "KLastModify.h"
#include "KUrlValue.h"
#include "KPathHandler.h"
#include "HttpFiber.h"
#include "KHttpLib.h"

enum {
	USER_TYPE_UNAUTH, USER_TYPE_ADMIN, USER_TYPE_VIRTUALHOST, USER_TYPE_NORMAL
};
class KConnectionInfoContext {
public:
	KConnectionInfoContext()
	{
		debug = 0;
		total_count = 0;
		vh = NULL;
		translate = false;
	}
	int debug;
	int total_count;
	const char *vh;
	bool translate;
	std::stringstream s;
};

bool kgl_connection_iterator(void* arg, KSink* rq);
class KHttpManage : public KFetchObject {
public:
	bool sendHttpContent();
	bool sendHttpHeader();
	KHttpManage();
	~KHttpManage();
	static KPathHandler<kgl_request_handler> handler;
	KGL_RESULT Open(KHttpRequest *rq, kgl_input_stream* in, kgl_output_stream* out);
	//void process(KHttpRequest *rq);
	bool start(bool &hit);
	bool start_vhs(bool &hit);
	bool start_listen(bool &hit);
	//bool start_obj(bool &hit);
	bool start_access(bool &hit);

	std::string getUrlValue(std::string name);
	bool sendHttp(const char *msg, INT64 content_length,
			const char * content_type = NULL, const char *add_header = NULL,
			int max_age = 0);
	bool sendMainPage();
	KHttpRequest* rq;
	kgl_output_stream* out;
private:
	bool save_access(KVirtualHost *vh,std::string redirect_url);
	KXmlAttribute urlParam;
	KUrlValue urlValue;
	char *postData;
	int postLen;
	int userType;
	bool sendHttp(const std::string &msg);
	void sendTest();
	bool parseUrlParam(char *param,size_t len);
	bool parseUrl(char *url);
	void parsePostData();
	char *parsePostFile(int &len, std::string &fileName);
#if 0
	bool exportConfig();
	bool importConfig();
	bool importexport();
#endif
	bool runCommand();
	bool sendErrorSaveConfig(int file=0);
	bool sendErrPage(const char *err_msg, int closed_flag = 0);

	bool sendLeftMenu();
	bool sendMainFrame();
	bool sendProcessInfo();
	bool send_css();
	bool config();
	bool configsubmit();
	bool extends(unsigned item=0);
	bool sendRedirect(const char *newUrl);
	bool reboot();
	bool sendXML(const char *buf,bool addXml=false);
	//bool start_fetchobj();
	bool xml;
};
bool changeAdminPassword(KUrlValue *url,std::string &errMsg);
bool checkManageLogin(KHttpRequest *rq) ;
void init_manager_handler();
#endif

