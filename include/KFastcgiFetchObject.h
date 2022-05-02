#ifndef KFASTCGIFETCHOBJECT_H_
#define KFASTCGIFETCHOBJECT_H_

#include "KFetchObject.h"
#include "KFastcgiUtils.h"
#include "KFileName.h"
#include "ksocket.h"
#include "KAsyncFetchObject.h"

class KFastcgiFetchObject: public KAsyncFetchObject {
public:
	KFastcgiFetchObject();
	virtual ~KFastcgiFetchObject();
protected:
	KGL_RESULT buildHead(KHttpRequest *rq);
	void buildPost(KHttpRequest *rq);
	virtual bool isExtend()
	{
		return false;
	}
	/*
	void expectDone(KHttpRequest *rq)
	{
		parser_ctx.keep_alive_time_out = 0;
		KAsyncFetchObject::expectDone(rq);
	}
	*/
	bool NeedTempFile(bool upload, KHttpRequest *rq)
	{
		return true;
	}
	bool checkContinueReadBody(KHttpRequest *rq)
	{
		return !bodyEnd;
	}
	kgl_parse_result ParseHeader(KHttpRequest *rq, char **data, int *len);
	KGL_RESULT ParseBody(KHttpRequest *rq, char **data, int *len);
private:
	void appendPostEnd();
	char *parse(KHttpRequest *rq,char **str,int *len,int *packet_len);
	void readBodyEnd(KHttpRequest *rq,char **data,int *len)
	{
		//要保持长连接，必须读完此连接全部数据，在读完body后还有可能有两个包要解析。
		//一个是空的数据包，一个是END_REQUEST包
		int packet_len;
		for (int i = 0; i < 2; i++) {
			if (bodyEnd) {
				*len = 0;
				break;
			}
			if (*len <= 0) {
				break;
			}
			parse(rq, data, len,&packet_len);
		}
	}
	char *pad_buf;
	int pad_len;
	//body_len = -1时表示在读head
	int body_len;
	bool bodyEnd;
	FCGI_Header buf;
protected:
	void RestorePacket(char **packet, int *packet_len)
	{
		if (pad_buf) {
			char *nb = (char *)xmalloc(pad_len + *packet_len);
			kgl_memcpy(nb, pad_buf, pad_len);
			kgl_memcpy(nb + pad_len, *packet, *packet_len);
			xfree(pad_buf);
			pad_buf = nb;
			pad_len += *packet_len;
			*packet = pad_buf;
			*packet_len = pad_len;
		}
	}
	void SavePacket(char *packet, int packet_len)
	{
		char *save_pad_buf = NULL;
		if (packet_len > 0) {
			save_pad_buf = (char *)xmalloc(packet_len);
			kgl_memcpy(save_pad_buf, packet, packet_len);
		}
		if (pad_buf) {
			xfree(pad_buf);
		}
		pad_buf = save_pad_buf;
		pad_len = packet_len;
		return;
	}
};
#endif /* KFASTCGIFETCHOBJECT_H_ */
