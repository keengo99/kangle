/*
 * KHttpHeadPull.h
 *
 *  Created on: 2010-6-11
 *      Author: keengo
 */

#ifndef KHTTPHEADPULL_H_
#define KHTTPHEADPULL_H_
#include "KStringBuf.h"
#include "KHttpParser.h"
#include "KHttpResponseParser.h"

class KHttpPushParser {
public:
	KHttpPushParser()
	{
		memset(this, 0, sizeof(*this));
	}
	~KHttpPushParser()
	{
		if (buf) {
			xfree(buf);
		}
	}
	kgl_parse_result Parse(KHttpRequest *rq,const char *str, int len)
	{
		Push(str, len);
		return InternalParse(rq);	
	}
	const char *GetBody(int *len)
	{
		*len = this->len;
		return hot;
	}
	khttp_parser parser;
	KHttpResponseParser parser_ctx;
private:
	kgl_parse_result InternalParse(KHttpRequest *rq)
	{
		khttp_parse_result rs;
		for (;;) {
			memset(&rs, 0, sizeof(rs));
			kgl_parse_result result = khttp_parse(&parser, &hot, &len, &rs);
			switch (result) {
			case kgl_parse_want_read:
			case kgl_parse_error:
				return kgl_parse_error;
			case kgl_parse_continue:
				SavePoint();
				return kgl_parse_continue;
			case kgl_parse_success:
#if 0
				if (rs.is_first) {
					parser_ctx.StartParse(rq);
				}
#endif
				if (!parser_ctx.ParseHeader(rq, rs.attr, rs.attr_len, rs.val, rs.val_len, rs.request_line)) {
					return kgl_parse_error;
				}
				//printf("attr=[%s] val=[%s]\n", rs.attr, rs.val);
				break;
			case kgl_parse_finished:
				parser_ctx.EndParse(rq);
				return kgl_parse_finished;
			}
		}
	}
	void SavePoint()
	{
		if (buf) {
			xfree(buf);
			buf = NULL;
			buf_len = 0;
		}
		if (len > 0) {
			buf = (char *)xmalloc(len);
			kgl_memcpy(buf, hot, len);
			buf_len = len;
		}
	}
	void Push(const char *str, int len)
	{
		if (buf != NULL) {
			char *nb = (char *)malloc(buf_len + len);
			kgl_memcpy(nb, buf, buf_len);
			kgl_memcpy(nb + buf_len, str, len);
			xfree(buf);
			buf = nb;
			this->len = buf_len + len;
			hot = buf;
		} else {
			hot = (char *)str;
			this->len = len;
		}
	}
	char *hot;
	int len;
	char *buf;
	int buf_len;
};
#endif /* KHTTPHEADPULL_H_ */
