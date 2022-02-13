#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "kmalloc.h"
#include "global.h"
#include "KHttpParser.h"

kgl_parse_result khttp_parse_header(khttp_parser *parser, char *header, char *end, khttp_parse_result *rs)
{
	char *val;
	if (rs->is_first && !parser->first_same) {
		val = strchr(header, ' ');
		rs->request_line = 1;
	} else {
		val = strchr(header, ':');
	}
	
	if (val == NULL) {
		return kgl_parse_continue;
		/*
		if (isFirst) {
			//对于fastcgi协议，有可能会发送第一行HTTP/1.1 xxx xxx过来。
			//为了兼容性，要忽略第一行错误
			return true;
		}
		return false;
		*/
	}
	*val = '\0';
	val++;
	while (*val && IS_SPACE(*val)) {
		val++;
	}
	char *header_end = header;
	while (*header_end && !IS_SPACE(*header_end)) {
		header_end++;
	}
	rs->attr = header;
	rs->attr_len = (int)(header_end - header);
	*header_end = '\0';
	rs->val_len = (int)(end - val);
	rs->val = val;
	return kgl_parse_success;
}
kgl_parse_result khttp_parse(khttp_parser *parser, char **start, int *len, khttp_parse_result *rs)
{
restart:
	kassert(*len >= 0);
	if (*len <= 0) {
		kassert(*len == 0);
		return kgl_parse_continue;
	}
	char *pn = (char *)memchr(*start, '\n', *len);
	if (pn == NULL) {
		return kgl_parse_continue;
	}
	if (*start[0] == '\n' || *start[0] == '\r') {
		int checked = (int)(pn + 1 - *start);
		parser->header_len += checked;
		*start += checked;
		*len -= checked;
		if (!parser->started) {			
			goto restart;
		}
		parser->finished = 1;
		//printf("body[0]=%d,bodyLen=%d\n",body[0],bodyLen);
		return kgl_parse_finished;
	}
	if (parser->started) {
		/*
		 * 我们还要看看这个http域有没有换行，据rfc2616.
		 *        LWS            = [CRLF] 1*( SP | HT )
		 *        我们还要看看下一行第一个字符是否是空行。
		 */
		if (pn - *start == *len - 1) {
			/*
			 * 如果\n是最后的字符,则要continue.
			 */
			return kgl_parse_continue;
		}
		/*
		 * 如果下一行开头字符是SP或HT，则要并行处理。把\r和\n都换成SP
		 */
		while (pn[1]==' ' || pn[1]=='\t') {
			*pn = ' ';
			int checked = (int)(pn + 1 - *start);
			char *pr = (char *)memchr(*start, '\r', checked);
			if (pr) {
				*pr = ' ';
			}
			pn = (char *)memchr(pn,'\n', *len - checked);
			if (pn == NULL) {
				return kgl_parse_continue;
			}
		}
	}
	int checked = (int)(pn + 1 - *start);
	parser->header_len += checked;
	char *hot = *start;
	int hot_len = pn - *start;
	*start += checked;
	*len -= checked;
	if (hot_len > 2 && *(pn-1)=='\r') {
		pn--;
	}
	*pn = '\0';
	rs->is_first = !parser->started;
	parser->started = 1;
	return khttp_parse_header(parser, hot, pn, rs);
}
