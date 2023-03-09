#ifndef KACCESSDSO_H
#define KACCESSDSO_H
#include "KMark.h"
#include "KAcl.h"
#include "ksapi.h"
#include "KDsoExtend.h"
#include "KAccessDsoSupport.h"
#include "KBufferFetchObject.h"

#ifdef ENABLE_KSAPI_FILTER

class KAccessDso
{
public:
	KAccessDso(kgl_access *access, KDsoExtend *dso,int notify_type)
	{
		this->access = access;
		this->dso = dso;
		init_access_dso_support(&ctx,notify_type);
		ctx.module = access->create_ctx();
		this->notify_type = notify_type;
	}
	~KAccessDso()
	{
		if  (ctx.module)  {
			access->free_ctx(ctx.module);
		}
	}
	int32_t shutdown();
	void Init();
	uint32_t process(KHttpRequest *rq, KAccessContext *cn,DWORD notify);
	KAccessDso *new_instance()
	{
		return new KAccessDso(access,dso,notify_type);
	}
	const char *getName()
	{
		return access->name;
	}
	std::string getHtml(KModel *model);
	std::string getDisplay();
	void parse_config(const khttpd::KXmlNodeBody* xml);
	void parse_child(const kconfig::KXmlChanged* changed);
#if 0
	void editHtml(std::map<std::string, std::string> &attribute,bool html);
	bool startCharacter(KXmlContext *context, char *character, int len);
	void buildXML(std::stringstream &s);
	bool endElement(KXmlContext *context);
#endif
	friend class KAccessDsoMark;
	friend class KAccessDsoAcl;
	KDsoExtend *dso;
private:
	std::string build(uint32_t type);
	kgl_access *access;
	kgl_access_context ctx;
	int notify_type;
};
class KAccessDsoMark : public KMark
{
public:
	KAccessDsoMark(KAccessDso *ad)
	{
		this->ad = ad;
	}
	~KAccessDsoMark()
	{
		delete ad;
	}
	int32_t shutdown() override
	{
		return ad->shutdown();
	}
	void Init()
	{
		ad->Init();
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj, KFetchObject** fo) override
	{
		KAccessContext cn;
		uint32_t result = ad->process(rq, &cn, obj ? KF_NOTIFY_RESPONSE_MARK : KF_NOTIFY_REQUEST_MARK);
		if (KBIT_TEST(result, KF_STATUS_REQ_FINISHED)) {
			if (cn.buffer) {
				kassert(KBIT_TEST(ad->notify_type, KF_NOTIFY_RESPONSE_MARK | KF_NOTIFY_RESPONSE_ACL) == 0);
				*fo = new KBufferFetchObject(cn.buffer->getHead(),cn.buffer->getLen());
				if (rq->sink->data.status_code == 0) {
					rq->response_status(STATUS_OK);
				}
			}
			//jump_type = JUMP_FINISHED;
			return true;
		}
		return KBIT_TEST(result, KF_STATUS_REQ_TRUE);
	}
	KMark * new_instance() override
	{
		return new KAccessDsoMark(ad->new_instance());
	}
	const char *getName() override
	{
		return ad->getName();
	}
	std::string getHtml(KModel *model) override
	{
		return ad->getHtml(model);
	}
	std::string getDisplay() override
	{
		return ad->getDisplay();
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		ad->parse_config(xml);
		init_event();
		return;
	}
	void parse_child(const kconfig::KXmlChanged* changed) override {
		ad->parse_child(changed);
	}
private:
	KAccessDso *ad;
	void init_event();
};
class KAccessDsoAcl : public KAcl
{
public:
	KAccessDsoAcl(KAccessDso *ad)
	{
		this->ad = ad;
	}
	~KAccessDsoAcl()
	{
		delete ad;
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) override
	{
		KAccessContext cn;
		uint32_t ret = ad->process(rq, &cn, obj ? KF_NOTIFY_RESPONSE_ACL : KF_NOTIFY_REQUEST_ACL);
		return KBIT_TEST(ret, KF_STATUS_REQ_TRUE);
	}
	KAcl *new_instance() override
	{
		return new KAccessDsoAcl(ad->new_instance());
	}
	const char *getName() override
	{
		return ad->getName();
	}
	std::string getHtml(KModel *model) override
	{
		return ad->getHtml(model);
	}
	std::string getDisplay() override
	{
		return ad->getDisplay();
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		ad->parse_config(xml);
	}
	void parse_child(const kconfig::KXmlChanged* changed) override {
		ad->parse_child(changed);
	}
private:
	KAccessDso *ad;
};
#endif
#endif
