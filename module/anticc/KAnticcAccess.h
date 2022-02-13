#ifndef KMANTICCACCESS_H
#define KMANTICCACCESS_H
#include "anticc.h"
struct KAntiRefreshBuffer
{
	unsigned short type;
	unsigned short len;
	char *buf;
	KAntiRefreshBuffer *next;
};
class KAnticcAccess
{
public:
	KAnticcAccess()
	{
		buffer = NULL;
	}
	~KAnticcAccess()
	{
		destroy();
	}
	void destroy()
	{
		while (buffer) {
			KAntiRefreshBuffer *tmp = buffer->next;
			if (buffer->buf) {
				free(buffer->buf);
			}
			delete buffer;
			buffer = tmp;
		}
	}
	static void build_html(kgl_access_build *build_ctx,KAnticcAccess *m)
	{
		std::stringstream s;
		s << "msg:<textarea name='msg' rows='6' cols='50'>";
		if (m) {
			s << m->msg;
		} else {
			s << anti_cc_models[0].content;
		}
		s << "</textarea>";
		s << "<script language='javascript'>\r\n\
			 var cc_model=new Array(";
		int total_cc_model = sizeof(anti_cc_models)/sizeof(anti_cc_model);
		for(int i=0;i<total_cc_model;i++){
			if (i>0) {
				s << ",";
			}
			s << "'" << anti_cc_models[i].content << "'";
		}
		s << ");\
			 function set_cc_model(m){\r\n\
			 if(m==0) return;\
			 accessaddform.msg.value=cc_model[m-1];\r\n\
			}\r\n\
			</script>\r\n<br>\
			preset msg:<select onchange='set_cc_model(this.options[this.selectedIndex].value)'>";
		s << "<option value='0'>--select preset msg--</option>";
		for(int i=0;i<total_cc_model;i++){
			s << "<option value='" << i+1 << "'>" << anti_cc_models[i].name << "</option>";
		}
		s << "</select>";
		build_ctx->write_string(build_ctx->server_ctx,s.str().c_str(),s.str().size(),0);
	}
	DWORD process(kgl_filter_context * ctx,kgl_filter_request *rq);
	void parse(kgl_access_parse *parse_ctx)
	{
		const char *msg = parse_ctx->get_value(parse_ctx->server_ctx,"msg");
		if (msg==NULL) {
			return;
		}
		this->msg = msg;
		destroy();
		char *buf = strdup(msg);
		char *hot = buf;
		for (;;) {
			char *start = strstr(hot,"{{");
			if (start==NULL) {
				break;
			}
			char *end = strstr(start,"}}");
			if (end==NULL) {
				break;
			}
			addBuffer(hot,start - hot);
			*end = '\0';
			addCommand(start + 2);
			hot = end + 2;		
		}
		addBuffer(hot,strlen(hot));
		free(buf);
	}
	void build_xml(kgl_access_build *build_ctx)
	{
		build_ctx->write_string(build_ctx->server_ctx,"msg='",sizeof("msg='")-1,0);
		build_ctx->write_string(build_ctx->server_ctx,msg.c_str(),msg.size(),KGL_BUILD_HTML_ENCODE);
		build_ctx->write_string(build_ctx->server_ctx,"'",1,0);
	}
private:
	void addCommand(char *cmd)
	{	
		KAntiRefreshBuffer *buf = new KAntiRefreshBuffer;
		buf->buf = NULL;
		buf->len = 0;
		if (strcasecmp(cmd,"url")==0) {
			buf->type = CC_BUFFER_URL;		
		} else if(strcasecmp(cmd,"murl")==0) {
			buf->type = CC_BUFFER_MURL;
		} else if(strncasecmp(cmd,"revert:",7)==0) {
			buf->type = CC_BUFFER_REVERT;
			buf->buf = strdup(cmd+7);
		} else if(strncasecmp(cmd,"sb:",3)==0) {
			buf->type = CC_BUFFER_SB;
			buf->buf = strdup(cmd+3);
		} else {
			delete buf;
			return;
		}
		addBuffer(buf);
	}
	void addBuffer(KAntiRefreshBuffer *buf)
	{		
		buf->next = NULL;
		if (buffer==NULL) {
			buffer = buf;
			return;
		}
		KAntiRefreshBuffer *last = buffer;
		while (last->next) {
			last = last->next;
		}
		last->next = buf;
	}
	void addBuffer(char *msg,int len)
	{
		KAntiRefreshBuffer *buf = new KAntiRefreshBuffer;
		buf->type = CC_BUFFER_STRING;
		buf->buf = (char *)malloc(len);
		memcpy(buf->buf,msg,len);
		buf->len = len;
		addBuffer(buf);
	}
	std::string msg;
	KAntiRefreshBuffer *buffer;
};
#endif

