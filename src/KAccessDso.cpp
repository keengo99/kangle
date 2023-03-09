#include "KAccessDso.h"
#include "KXml.h"
#include "ksapi.h"
#include "extern.h"
#include "KHttpRequest.h"
#include "KHttpObject.h"
#include "KAccessDsoSupport.h"
#include "kselector_manager.h"
#include "KSimulateRequest.h"

#ifdef ENABLE_KSAPI_FILTER
kev_result next_dso_init(KOPAQUE data, void* arg, int got)
{
	KAccessDsoMark* mark = (KAccessDsoMark*)arg;
	mark->Init();
	mark->release();
	return kev_ok;
}
static int write_int(void* server_ctx, int value)
{
	std::stringstream* s = (std::stringstream*)server_ctx;
	*s << value;
	return 0;
}
static int write_string(void* server_ctx, const char* str, int len, int build_flags)
{
	std::stringstream* s = (std::stringstream*)server_ctx;
	if (KBIT_TEST(build_flags, KGL_BUILD_HTML_ENCODE)) {
		char* buf = KXml::htmlEncode(str, len, NULL);
		s->write(buf, len);
		free(buf);
		return 0;
	}
	s->write(str, len);
	return 0;
}
static const char* get_text(KCONN_CFG cn)
{
	const khttpd::KXmlNodeBody* xml = (const khttpd::KXmlNodeBody*)cn;
	return xml->get_text();
}
static int64_t get_int(KCONN_CFG cn, const char* name)
{
	const khttpd::KXmlNodeBody* xml = (const khttpd::KXmlNodeBody*)cn;
	return xml->attributes.get_int64(name);
}
static const char* get_value(KCONN_CFG cn, const char* name)
{
	const khttpd::KXmlNodeBody* xml = (const khttpd::KXmlNodeBody*)cn;
	return xml->attributes.get_string(name);
}
static KCONN_CFG_NODE find_child(KCONN_CFG cn, const char* name) {
	const khttpd::KXmlNodeBody* xml = (const khttpd::KXmlNodeBody*)cn;
	return (KCONN_CFG_NODE)kconfig::find_child(xml, name, strlen(name));
}
static kgl_access_config_f access_config_func = {
	get_value,
	get_int,
	get_text,
	find_child
};
static const char* get_tag(KCONN_CFG_NODE cn) {
	const khttpd::KXmlNode* node = (const khttpd::KXmlNode*)cn;
	return node->key.tag->data;
}
static uint32_t get_count(KCONN_CFG_NODE cn) {
	const khttpd::KXmlNode* node = (const khttpd::KXmlNode*)cn;
	return node->get_body_count();
}
static KCONN_CFG get_data(KCONN_CFG_NODE cn, uint32_t index) {
	const khttpd::KXmlNode* node = (const khttpd::KXmlNode*)cn;
	return (KCONN_CFG)node->get_body(index);
}
static kgl_access_node_config_f access_node_config_func = {
	get_tag,
	get_count,
	get_data,
};
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
void KAccessDso::parse_config(const khttpd::KXmlNodeBody* xml) {
	kgl_access_parse_config cfg{ 0 };	
	cfg.module = ctx.module;
	cfg.cfg = access_config_func;
	cfg.node = access_node_config_func;
	cfg.cn = xml;	
	auto result = access->parse_config(&cfg);
	if (result != KGL_OK) {
		throw KXmlException("parse child error");
	}
}
void KAccessDso::parse_child(const kconfig::KXmlChanged* changed) {
	if (access->parse_child==NULL) {
		return;
	}
	kgl_access_parse_child cfg{ 0 };
	cfg.o = changed->old_xml;
	cfg.n = changed->new_xml;
	cfg.diff = changed->diff;
	cfg.module = ctx.module;
	cfg.cfg = access_config_func;
	cfg.node = access_node_config_func;
	auto result = access->parse_child(&cfg);
	if (result != KGL_OK) {
		throw KXmlException("parse child error");
	}
}
std::string KAccessDso::getHtml(KModel* model)
{
	return "";
}
std::string KAccessDso::getDisplay()
{
	return build(0);
}
std::string KAccessDso::build(uint32_t type)
{
	std::stringstream s;
	kgl_access_build builder;
	memset(&builder, 0, sizeof(builder));
	builder.cn = &s;
	builder.module = ctx.module;
	builder.write_string = write_string;
	builder.write_int = write_int;
	access->build(&builder, type);
	return s.str();
}
void KAccessDsoMark::init_event()
{
	if (ad->access->init_shutdown) {
		kgl_selector_module.next(get_selector_by_index(0), NULL, next_dso_init, this, 0);
		this->add_ref();
	}
}
#endif
