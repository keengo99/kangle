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
	KWStream * s = (KWStream *)server_ctx;
	*s << value;
	return 0;
}
static int write_string(void* server_ctx, const char* str, int len, int build_flags)
{
	KWStream* s = (KWStream *)server_ctx;
	if (KBIT_TEST(build_flags, KGL_BUILD_HTML_ENCODE)) {
		char* buf = KXml::htmlEncode(str, len, NULL);
		s->write_all(buf, len);
		free(buf);
		return 0;
	}
	s->write_all(str, len);
	return 0;
}
static const char* get_text(const kgl_config_body*cn)
{
	const khttpd::KXmlNodeBody* xml = (const khttpd::KXmlNodeBody*)cn;
	return xml->get_text();
}
static int64_t get_int(const kgl_config_body* cn, const char* name)
{
	const khttpd::KXmlNodeBody* xml = (const khttpd::KXmlNodeBody*)cn;
	return xml->attributes.get_int64(name);
}
static const char* get_value(const kgl_config_body* cn, const char* name)
{
	const khttpd::KXmlNodeBody* xml = (const khttpd::KXmlNodeBody*)cn;
	return xml->attributes.get_string(name);
}
static kgl_config_node* find_child(const kgl_config_body* cn, const char* name) {
	khttpd::KXmlNodeBody* xml = (khttpd::KXmlNodeBody*)cn;
	return (kgl_config_node *)kconfig::find_child(xml, name, strlen(name));
}
static void set_value(kgl_config_body* cn, const char* name, const char* value) {
	khttpd::KXmlNodeBody* body = (khttpd::KXmlNodeBody*)cn;
	body->attributes.emplace(name, value);
}
static void set_int(kgl_config_body* cn, const char* name, int64_t value) {
	KStringBuf s;
	s << value;
	khttpd::KXmlNodeBody* body = (khttpd::KXmlNodeBody*)cn;
	body->attributes.emplace(name, s.c_str());
}
static void set_text(kgl_config_body* cn, const char* value) {
	khttpd::KXmlNodeBody* body = (khttpd::KXmlNodeBody*)cn;
	body->set_text(value);
}
static kgl_config_body *new_child(kgl_config_body* cn, const char* name) {
	khttpd::KXmlNodeBody* body = (khttpd::KXmlNodeBody*)cn;
	return (kgl_config_body*)kconfig::new_child(body, name, strlen(name));
}

kgl_config_body_f kgl_config_body_func = {
	get_value,
	get_int,
	get_text,
	set_value,
	set_int,
	set_text,
	find_child,
	new_child
};
static const char* get_tag(const kgl_config_node *cn) {
	const khttpd::KXmlNode* node = (const khttpd::KXmlNode*)cn;
	return node->key.tag->data;
}
static uint32_t get_count(const kgl_config_node* cn) {
	const khttpd::KXmlNode* node = (const khttpd::KXmlNode*)cn;
	return node->get_body_count();
}
static kgl_config_body* get_body(const kgl_config_node* cn, uint32_t index) {
	const khttpd::KXmlNode* node = (const khttpd::KXmlNode*)cn;
	return (kgl_config_body*)node->get_body(index);
}
static kgl_config_body* new_body(kgl_config_node* cn, uint32_t index) {
	return NULL;
}
kgl_config_node_f kgl_config_node_func = {
	get_tag,
	get_count,
	get_body,
	new_body
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
	kgl_access_parse_config cfg;
	cfg.module = ctx.module;
	cfg.body = &kgl_config_body_func;
	cfg.node = &kgl_config_node_func;
	cfg.cn = (kgl_config_body *)xml;
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
	cfg.o = (const kgl_config_node *)changed->old_xml;
	cfg.n = (const kgl_config_node *)changed->new_xml;
	cfg.diff = changed->diff;
	cfg.module = ctx.module;
	cfg.body = &kgl_config_body_func;
	cfg.node = &kgl_config_node_func;
	auto result = access->parse_child(&cfg);
	if (result != KGL_OK) {
		throw KXmlException("parse child error");
	}
}
void KAccessDso::getHtml(KModel* model, KWStream& s)
{
	
}
void KAccessDso::getDisplay(KWStream& s)
{
	return build(0,s);
}
void KAccessDso::build(uint32_t type, KWStream& s)
{
	kgl_access_build builder;
	memset(&builder, 0, sizeof(builder));
	builder.cn = &s;
	builder.module = ctx.module;
	builder.write_string = write_string;
	builder.write_int = write_int;
	access->build(&builder, type);
	return;
}
void KAccessDsoMark::init_event()
{
	if (ad->access->init_shutdown) {
		kgl_selector_module.next(get_selector_by_index(0), NULL, next_dso_init, this, 0);
		this->add_ref();
	}
}
#endif
