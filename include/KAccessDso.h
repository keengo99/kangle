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
	KAccessDso *newInstance()
	{
		return new KAccessDso(access,dso,notify_type);
	}
	const char *getName()
	{
		return access->name;
	}
	std::string getHtml(KModel *model);
	std::string getDisplay();
	void editHtml(std::map<std::string, std::string> &attribute,bool html);
	bool startCharacter(KXmlContext *context, char *character, int len);
	void buildXML(std::stringstream &s);
	bool endElement(KXmlContext *context);
	friend class KAccessDsoMark;
	friend class KAccessDsoAcl;
	KDsoExtend *dso;
private:
	std::string build(KF_ACCESS_BUILD_TYPE type);
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
	int32_t shutdown()
	{
		return ad->shutdown();
	}
	void Init()
	{
		ad->Init();
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj,const int chainJumpType, int &jumpType)
	{
		KAccessContext cn;
		uint32_t result = ad->process(rq, &cn, obj ? KF_NOTIFY_RESPONSE_MARK : KF_NOTIFY_REQUEST_MARK);
		if (KBIT_TEST(result, KF_STATUS_REQ_FINISHED)) {
			if (cn.buffer) {
				kassert(KBIT_TEST(ad->notify_type, KF_NOTIFY_RESPONSE_MARK | KF_NOTIFY_RESPONSE_ACL) == 0);
				rq->CloseFetchObject();
				rq->responseConnection();
				rq->responseHeader(kgl_expand_string("Content-Length"), cn.buffer->getLen());
				rq->AppendFetchObject(new KBufferFetchObject(cn.buffer));
				if (rq->sink->data.status_code == 0) {
					rq->responseStatus(STATUS_OK);
				}
			}
			jumpType = JUMP_FINISHED;
			return true;
		}
		return KBIT_TEST(result, KF_STATUS_REQ_TRUE);
	}
	KMark *newInstance()
	{
		return new KAccessDsoMark(ad->newInstance());
	}
	const char *getName()
	{
		return ad->getName();
	}
	std::string getHtml(KModel *model)
	{
		return ad->getHtml(model);
	}
	std::string getDisplay()
	{
		return ad->getDisplay();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html);
	void buildXML(std::stringstream &s)
	{
		ad->buildXML(s);
	}
	bool startCharacter(KXmlContext *context, char *character, int len)
	{
		return ad->startCharacter(context, character, len);
	}
	bool endElement(KXmlContext *context)
	{
		return ad->endElement(context);
	}
private:
	KAccessDso *ad;
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
	bool match(KHttpRequest *rq, KHttpObject *obj)
	{
		KAccessContext cn;
		uint32_t ret = ad->process(rq, &cn, obj ? KF_NOTIFY_RESPONSE_ACL : KF_NOTIFY_REQUEST_ACL);
		return KBIT_TEST(ret, KF_STATUS_REQ_TRUE);
	}
	KAcl *newInstance()
	{
		return new KAccessDsoAcl(ad->newInstance());
	}
	const char *getName()
	{
		return ad->getName();
	}
	std::string getHtml(KModel *model)
	{
		return ad->getHtml(model);
	}
	std::string getDisplay()
	{
		return ad->getDisplay();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html)
	{
		ad->editHtml(attribute,html);
	}
	void buildXML(std::stringstream &s)
	{
		ad->buildXML(s);
	}
	bool startCharacter(KXmlContext *context, char *character, int len)
	{
		return ad->startCharacter(context, character, len);
	}
	bool endElement(KXmlContext *context)
	{
		return ad->endElement(context);
	}
private:
	KAccessDso *ad;
};
#endif
#endif
