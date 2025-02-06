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
	kgl::serializable* sl = nullptr;

	bool translate;
	KStringBuf s;
};

bool kgl_connection_iterator(void* arg, KSink* rq);
bool kgl_connection_iterator2(void* arg, KSink* rq);
class KHttpManage : public KFetchObject {
public:
	KHttpManage();
	~KHttpManage();
	static KPathHandler<kgl_request_handler> handler;
	KGL_RESULT Open(KHttpRequest *rq, kgl_input_stream* in, kgl_output_stream* out);
	bool start(bool &hit);
	bool start_vhs(bool &hit);
	bool start_listen(bool &hit);
	bool start_access(bool &hit);
	KString removeUrlValue(const KString& name);
	const KString&getUrlValue(const KString&name);
	bool sendHttp(const char *msg, INT64 content_length,
			const char * content_type = NULL, const char *add_header = NULL,
			int max_age = 0);
	bool sendMainPage();
	KHttpRequest* rq;
	kgl_output_stream* out;
private:
	KUrlValue urlValue;
	char *postData;
	int postLen;
	int userType;
	bool sendHttp(const KString &msg);
	void sendTest();
	bool parseUrl(const char *url);
	void parsePostData();
	char *parsePostFile(int &len, KString &fileName);
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
};
bool changeAdminPassword(KUrlValue *url,KString &errMsg);
bool checkManageLogin(KHttpRequest *rq) ;
void init_manager_handler();
bool console_config_submit(size_t item, KUrlValue& uv, KString& err_msg);
#endif

