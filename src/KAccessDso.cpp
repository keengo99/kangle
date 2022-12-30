#include "KAccessDso.h"
#include "KXml.h"
#include "ksapi.h"
#include "extern.h"
#include "KHttpRequest.h"
#include "KHttpFilterContext.h"
#include "KHttpFilterHook.h"
#include "KHttpObject.h"
#include "KAccessDsoSupport.h"
#include "kselector_manager.h"
#include "KSimulateRequest.h"

#ifdef ENABLE_KSAPI_FILTER
#if 0
int WINAPI access_simulate_shutdown_body(void* arg, const char* data, int len)
{
	if (data) {
		fwrite(data, 1, len, stderr);
	} else {
		katom_dec((void*)&mark_module_shutdown);
	}
	return len;
}
int WINAPI access_simulate_init_body(void* arg, const char* data, int len)
{
	if (data) {
		fwrite(data, 1, len, stderr);
	}
	return len;
}
static bool access_simulate(KCONN cn, kgl_async_upstream* us, void* context, const char* url, bool init)
{
	KAccessDso* ctx = (KAccessDso*)cn;
	kgl_async_http async_ctx;
	memset(&async_ctx, 0, sizeof(kgl_async_http));
	async_ctx.meth = (char*)"GET";
	async_ctx.url = (char*)url;
	async_ctx.body = init ? access_simulate_init_body : access_simulate_shutdown_body;

	KHttpRequest* rq = kgl_create_simulate_request(&async_ctx);
	if (rq == NULL) {
		return false;
	}
	//目前还不支持同步模式
	KBIT_CLR(us->flags, KF_UPSTREAM_SYNC);
	KDsoRedirect* rd = new KDsoRedirect("", (kgl_upstream*)us);
	KFetchObject* fo = rd->makeFetchObject(rq, context);
	fo->bindRedirect(rd, KGL_CONFIRM_FILE_NEVER);
	fo->filter = 0;
	rq->InsertFetchObject(fo);
	if (!init) {
		katom_inc((void*)&mark_module_shutdown);
	}
	kgl_start_simulate_request(rq);
	return true;
}
#endif
kev_result next_dso_init(KOPAQUE data, void* arg, int got)
{
	KAccessDsoMark* mark = (KAccessDsoMark*)arg;
	mark->Init();
	mark->release();
	return kev_ok;
}
static int write_string(void* serverCtx, const char* str, int len, int build_flags)
{
	std::stringstream* s = (std::stringstream*)serverCtx;
	if (KBIT_TEST(build_flags, KGL_BUILD_HTML_ENCODE)) {
		char* buf = KXml::htmlEncode(str, len, NULL);
		s->write(buf, len);
		free(buf);
		return 0;
	}
	s->write(str, len);
	return 0;
}
static const char* get_character(void* server_ctx, const char* name)
{
	kgl_str_t* s = (kgl_str_t*)server_ctx;
	return s->data;
}
static int64_t get_int(void* server_ctx, const char* name)
{
	std::map<std::string, std::string>* attribute = (std::map<std::string, std::string>*)server_ctx;
	std::map<std::string, std::string>::iterator it = attribute->find(name);
	if (it == attribute->end()) {
		return 0;
	}
	return string2int((*it).second.c_str());
}
static const char* get_value(void* server_ctx, const char* name)
{
	std::map<std::string, std::string>* attribute = (std::map<std::string, std::string>*)server_ctx;
	std::map<std::string, std::string>::iterator it = attribute->find(name);
	if (it == attribute->end()) {
		return NULL;
	}
	return (*it).second.c_str();
}
void KAccessDso::Init()
{
	if (access->init_shutdown == NULL) {
		return;
	}
	access->init_shutdown(ctx.module, false);
	return;
}
int32_t KAccessDso::shutdown()
{
	if (access->init_shutdown == NULL) {
		return 0;
	}
	access->init_shutdown(ctx.module, true);
	return 0;
}
uint32_t KAccessDso::process(KHttpRequest* rq, KAccessContext* cn, DWORD notify)
{
	kgl_access_context ar_ctx;
	kgl_memcpy(&ar_ctx, &ctx, sizeof(ar_ctx));
	ar_ctx.cn = cn;
	return access->process(rq, &ar_ctx, notify);
}
std::string KAccessDso::getHtml(KModel* model)
{
	return build(KF_ACCESS_BUILD_HTML);
}
std::string KAccessDso::getDisplay()
{
	return build(KF_ACCESS_BUILD_SHORT);
}
void KAccessDso::editHtml(std::map<std::string, std::string>& attribute, bool html)
{
	kgl_access_parse parser;
	memset(&parser, 0, sizeof(parser));
	parser.get_value = get_value;
	parser.get_int = get_int;
	parser.module = ctx.module;
	parser.cn = &attribute;
	access->parse(&parser, KF_ACCESS_PARSE_KV);
	if (html) {
		access->parse(&parser, KF_ACCESS_PARSE_END);
	}

}
bool KAccessDso::startCharacter(KXmlContext* context, char* character, int len)
{
	kgl_access_parse parser;
	memset(&parser, 0, sizeof(parser));
	parser.get_value = get_character;
	parser.module = ctx.module;
	kgl_str_t str;
	str.data = character;
	str.len = len;
	parser.cn = &str;
	access->parse(&parser, KF_ACCESS_PARSE_XML_CHARACTER);
	return true;
}
bool KAccessDso::endElement(KXmlContext* context)
{
	kgl_access_parse parser;
	memset(&parser, 0, sizeof(parser));
	parser.module = ctx.module;
	access->parse(&parser, KF_ACCESS_PARSE_END);
	return true;
}
void KAccessDso::buildXML(std::stringstream& s)
{
	s << build(KF_ACCESS_BUILD_XML) << ">";
	s << build(KF_ACCESS_BUILD_XML_CHARACTER);
}
std::string KAccessDso::build(KF_ACCESS_BUILD_TYPE type)
{
	std::stringstream s;
	kgl_access_build builder;
	memset(&builder, 0, sizeof(builder));
	builder.cn = &s;
	builder.module = ctx.module;
	builder.write_string = write_string;
	access->build(&builder, type);
	return s.str();
}
void KAccessDsoMark::editHtml(std::map<std::string, std::string>& attribute, bool html)
{
	ad->editHtml(attribute, html);
	if (html) {
		init_event();
	}
}
bool KAccessDsoMark::endElement(KXmlContext* context)
{
	bool result = ad->endElement(context);
	if (result) {
		init_event();
	}
	return result;
}
void KAccessDsoMark::init_event()
{
	if (ad->access->init_shutdown) {
		kgl_selector_module.next(get_selector_by_index(0), NULL, next_dso_init, this, 0);
		this->addRef();
	}
}
#endif
