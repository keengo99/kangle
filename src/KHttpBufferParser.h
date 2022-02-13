#ifndef KHTTPBUFFERPARSER_H
#define KHTTPBUFFERPARSER_H
#include "global.h"
#include "KHttpHeaderManager.h"
#include "KHttpParser.h"
#define KWriteBackParser KHttpBufferParser

class KHttpBufferParser : public KHttpHeaderManager
{
public:
	KHttpBufferParser()
	{
		memset(this, 0, sizeof(*this));
		status_code = STATUS_OK;
		keep_alive = false;

	}
	~KHttpBufferParser()
	{
		if (header) {
			free_header(header);
		}
	}
	void Parse(char **str, int *len)
	{
		khttp_parser parser;
		memset(&parser, 0, sizeof(parser));
		while (*len > 0) {
			khttp_parse_result rs;
			memset(&rs, 0, sizeof(rs));
			switch (khttp_parse(&parser, str, len, &rs)) {
			case kgl_parse_finished:
			case kgl_parse_error:
			case kgl_parse_continue:
				return;
			default:
				parseHeader(rs.attr, rs.attr_len, rs.val, rs.val_len, rs.request_line);
			}
		}
	}
	void parseHeader(const char *attr, int attr_len, char *val, int val_len, bool isFirst)
	{
		if (isFirst) {
			status_code = atoi(val);
			return;
		}
		if (strcasecmp(attr, "Status") == 0) {
			status_code = atoi(val);
			return;
		}
		if (strcasecmp(attr, "Connection") == 0) {
			if (strcasecmp(val, "keep-alive") == 0) {
				keep_alive = true;
			}
			return;
		}
		if (strcasecmp(attr, "Content-Length") == 0
			|| strcasecmp(attr, "Transfer-Encoding") == 0) {
			return;
		}
		KHttpHeader *header = new_http_header(attr, attr_len, val, val_len);
		Append(header);
		return;
	}
	bool keep_alive;
	int status_code;
};
#endif
