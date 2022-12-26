#ifndef KFASTCGIFETCHOBJECT_H_
#define KFASTCGIFETCHOBJECT_H_

#include "KFetchObject.h"
#include "KFastcgiUtils.h"
#include "KFileName.h"
#include "ksocket.h"
#include "KAsyncFetchObject.h"

class KFastcgiFetchObject : public KAsyncFetchObject
{
public:
	KFastcgiFetchObject();
	virtual ~KFastcgiFetchObject();
protected:
	KGL_RESULT buildHead(KHttpRequest* rq) override;
	void buildPost(KHttpRequest* rq) override;
	virtual bool is_extend()
	{
		return false;
	}
	bool NeedTempFile(bool upload, KHttpRequest* rq) override
	{
		return true;
	}
	bool checkContinueReadBody(KHttpRequest* rq) override
	{
		return !pop_header.recved_end_request;
	}
	KGL_RESULT read_body_end(KHttpRequest* rq, KGL_RESULT result) override;
	kgl_parse_result parse_unknow_header(KHttpRequest* rq, char** data, char* end) override;
	KGL_RESULT ParseBody(KHttpRequest* rq, char** data, char* end) override;
private:
	void appendPostEnd();
	//return NULL for need more data
	char *parse_fcgi_header(char** str, char* end, bool full);
	//body_len = 0 ±±Ì æ∂¡head
	union
	{
		struct
		{
			uint16_t body_len;
			uint8_t fcgi_header_type;
			uint8_t fcgi_pad_length;
		};
		uint32_t flags;
	};
	ks_buffer* split_header;
protected:
};
#endif /* KFASTCGIFETCHOBJECT_H_ */
