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
		ks_buffer_init(&buf, 8192);
	}
	~KHttpPushParser()
	{
		ks_buffer_clean(&buf);
	}
	kgl_parse_result Parse(KHttpRequest *rq,const char *str, int len)
	{
		Push(str, len);
		return InternalParse(rq);	
	}
	const char *GetBody(int *len)
	{
		*len = buf.used;
		return buf.buf;
	}
	khttp_parser parser;
	KHttpResponseParser parser_ctx;
private:
	kgl_parse_result InternalParse(KHttpRequest *rq)
	{
		khttp_parse_result rs;
		char* hot = buf.buf;
		char* end = buf.buf + buf.used;
		for (;;) {
			memset(&rs, 0, sizeof(rs));
			kgl_parse_result result = khttp_parse(&parser, &hot, end, &rs);
			switch (result) {
			case kgl_parse_want_read:
			case kgl_parse_error:
				return kgl_parse_error;
			case kgl_parse_continue:
				ks_save_point(&buf, hot);
				return kgl_parse_continue;
			case kgl_parse_success:
				if (!parser_ctx.parse_unknow_header(rq, rs.attr, rs.attr_len, rs.val, rs.val_len, rs.request_line)) {
					return kgl_parse_error;
				}
				//printf("attr=[%s] val=[%s]\n", rs.attr, rs.val);
				break;
			case kgl_parse_finished:
				parser_ctx.end_parse(rq, -1);
				return kgl_parse_finished;
			}
		}
	}
	void Push(const char *str, int len)
	{
		ks_write_str(&buf, str, len);
	}
	ks_buffer buf;
};
#endif /* KHTTPHEADPULL_H_ */
