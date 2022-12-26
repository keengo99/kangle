#include "KMultiPartInputFilter.h"
#include "http.h"
#include "KUrlParser.h"
#ifdef ENABLE_INPUT_FILTER
char* my_strndup(const char* s, int length)
{
	char* p = (char*)malloc(length + 1);
	if (p == NULL) {
		return p;
	}
	kgl_memcpy(p, s, length);
	p[length] = 0;
	return p;
}
void* kgl_memstr(char* haystack, int haystacklen, char* needle, int needlen)
{
	int len = haystacklen;
	char* ptr = haystack;

	/* iterate through first character matches */
	while ((ptr = (char*)memchr(ptr, needle[0], len))) {

		/* calculate length after match */
		len = haystacklen - (int)(ptr - (char*)haystack);

		/* done if matches up to capacity of buffer */
		if (memcmp(needle, ptr, needlen < len ? needlen : len) == 0) {
			return ptr;
		}

		/* next character */
		ptr++; len--;
	}

	return NULL;
}
/*
 * Gets the next CRLF terminated line from the input buffer.
 * If it doesn't find a CRLF, and the buffer isn't completely full, returns
 * NULL; otherwise, returns the beginning of the null-terminated line,
 * minus the CRLF.
 *
 * Note that we really just look for LF terminated lines. This works
 * around a bug in internet explorer for the macintosh which sends mime
 * boundaries that are only LF terminated when you use an image submit
 * button in a multipart/form-data form.
 */
static char* next_line(multipart_buffer* self)
{
	/* look for LF in the data */
	char* line = self->buf_begin;
	char* ptr = (char*)memchr(self->buf_begin, '\n', self->bytes_in_buffer);

	if (ptr) {	/* LF found */

		/* terminate the string, remove CRLF */
		if ((ptr - line) > 0 && *(ptr - 1) == '\r') {
			*(ptr - 1) = 0;
		} else {
			*ptr = 0;
		}

		/* bump the pointer */
		self->buf_begin = ptr + 1;
		self->bytes_in_buffer -= (int)(self->buf_begin - line);

	} else {	/* no LF found */

		/* buffer isn't completely full, fail */
		if (self->bytes_in_buffer < self->bufsize) {
			return NULL;
		}
		/* return entire buffer as a partial line */
		line[self->bufsize] = 0;
		self->buf_begin = ptr;
		self->bytes_in_buffer = 0;
	}

	return line;
}
/* finds a boundary */
static bool find_boundary(multipart_buffer* self)
{
	char* line;

	/* loop thru lines */
	while ((line = next_line(self))) {
		/* finished if we found the boundary */
		if (!strcmp(line, self->boundary)) {
			return true;
		}
	}

	/* didn't find the boundary */
	return false;
}
static char* substring_conf(char* start, int len, char quote)
{
	char* result = (char*)malloc(len + 2);
	char* resp = result;
	int i;

	for (i = 0; i < len; ++i) {
		if (start[i] == '\\' && (start[i + 1] == '\\' || (quote && start[i + 1] == quote))) {
			*resp++ = start[++i];
		} else {
			*resp++ = start[i];
		}
	}

	*resp = '\0';
	return result;
}
static char* php_ap_getword_conf(char** line)
{
	char* str = *line, * strend, * res, quote;


	while (*str && isspace(*str)) {
		++str;
	}

	if (!*str) {
		*line = str;
		return strdup("");
	}

	if ((quote = *str) == '"' || quote == '\'') {
		strend = str + 1;
	look_for_quote:
		while (*strend && *strend != quote) {
			if (*strend == '\\' && strend[1] && strend[1] == quote) {
				strend += 2;
			} else {
				++strend;
			}
		}
		if (*strend && *strend == quote) {
			char p = *(strend + 1);
			if (p != '\r' && p != '\n' && p != '\0') {
				strend++;
				goto look_for_quote;
			}
		}

		res = substring_conf(str + 1, (int)(strend - str - 1), quote);

		if (*strend == quote) {
			++strend;
		}

	} else {

		strend = str;
		while (*strend && !isspace(*strend)) {
			++strend;
		}
		res = substring_conf(str, (int)(strend - str), 0);
	}

	while (*strend && isspace(*strend)) {
		++strend;
	}

	*line = strend;
	return res;
}
static char* php_ap_getword(char** line, char stop)
{
	char* pos = *line, quote;
	char* res;

	while (*pos && *pos != stop) {
		if ((quote = *pos) == '"' || quote == '\'') {
			++pos;
			while (*pos && *pos != quote) {
				if (*pos == '\\' && pos[1] && pos[1] == quote) {
					pos += 2;
				} else {
					++pos;
				}
			}
			if (*pos) {
				++pos;
			}
		} else ++pos;
	}
	if (*pos == '\0') {
		res = strdup(*line);
		*line += strlen(*line);
		return res;
	}

	res = my_strndup(*line, (int)(pos - *line));

	while (*pos == stop) {
		++pos;
	}

	*line = pos;
	return res;
}
bool multipart_eof(multipart_buffer* mb, bool isLast)
{
	if (isLast) {
		return mb->bytes_in_buffer <= 0;
	}
	return  mb->bytes_in_buffer <= mb->boundary_next_len + 1;
}
char* get_hdr_value(KHttpHeader* header, const char* attr,size_t attr_len)
{
	while (header) {
		if (kgl_is_attr(header,attr,attr_len)) {
			return header->buf + header->val_offset;
		}
		header = header->next;
	}
	return NULL;
}

int KMultiPartInputFilter::check(KInputFilterContext* rq, const char* str, int len, bool isLast)
{
	int ret = InternalCheck(rq, str, len, isLast);
	return ret;
}
int KMultiPartInputFilter::InternalCheck(KInputFilterContext* rq, const char* str, int len, bool isLast)
{
	if (rq->mb == NULL) {
		return JUMP_ALLOW;
	}
	if (str) {
		rq->mb->add(str, len);
	}
	while (!multipart_eof(rq->mb, isLast)) {
		if (rq->mb->model == MULTIPART_BODY_MODEL) {
			bool success;
			if (JUMP_DENY == handleBody(rq, success)) {
				return JUMP_DENY;
			}
			if (!success) {
				return JUMP_ALLOW;
			}
			continue;
		}
		if (rq->mb->model == MULTIPART_BOUNDARY_MODEL) {
			if (!find_boundary(rq->mb)) {
				return JUMP_ALLOW;
			}
			rq->mb->model = MULTIPART_HEAD_MODEL;
			continue;
		}

		char* cd = NULL;
		if (param) {
			free(param);
			param = NULL;
		}
		if (filename) {
			free(filename);
			filename = NULL;
		}
		switch (parseHeader(rq)) {
		case kgl_parse_finished:
			break;
		case kgl_parse_error:
			delete rq->mb;
			rq->mb = NULL;
			return JUMP_ALLOW;
		default:
			return JUMP_ALLOW;
		}
		if ((cd = get_hdr_value(hm.header, _KS("Content-Disposition")))) {
			char* pair = NULL;
			while (isspace(*cd)) {
				++cd;
			}
			while (*cd && (pair = php_ap_getword(&cd, ';'))) {
				char* key = NULL, * word = pair;
				while (isspace(*cd)) {
					++cd;
				}
				if (strchr(pair, '=')) {
					key = php_ap_getword(&pair, '=');
					if (!strcasecmp(key, "name")) {
						if (param) {
							free(param);
						}
						param = php_ap_getword_conf(&pair);
						if (param) {
							param_len = url_decode(param, 0, NULL, true);
						}
					} else if (!strcasecmp(key, "filename")) {
						if (filename) {
							free(filename);
						}
						filename = php_ap_getword_conf(&pair);
						if (filename) {
							filename_len = url_decode(filename, 0, NULL, true);
						}
					}
				}
				if (key) {
					free(key);
				}
				free(word);
			}
			if (filename) {
				KInputFilterHelper<KFileFilterHook>* file = file_head;
				while (file) {
					if (JUMP_DENY == file->t->matchFilter(rq, param, param_len, filename, filename_len, hm.header)) {
						return JUMP_DENY;
					}
					file = file->next;
				}
			}
			if (hm.header) {
				free_header_list2(hm.header);
				memset(&hm, 0, sizeof(hm));
			}

		}
	}
	return JUMP_ALLOW;
}
kgl_parse_result KMultiPartInputFilter::parseHeader(KInputFilterContext* rq)
{
	khttp_parser parser;
	khttp_parse_result rs;
	memset(&parser, 0, sizeof(parser));
	parser.first_same = 1;
	char* end = rq->mb->buf_begin + rq->mb->bytes_in_buffer;
	while (rq->mb->buf_begin < end) {
		memset(&rs, 0, sizeof(rs));
		kgl_parse_result ret = khttp_parse(&parser, &rq->mb->buf_begin, end, &rs);
		rq->mb->bytes_in_buffer = (int)(end - rq->mb->buf_begin);
		switch (ret) {
		case kgl_parse_continue:
		case kgl_parse_error:
			return ret;
		case kgl_parse_success:
			hm.add_header(rs.attr, rs.attr_len, rs.val, rs.val_len);
			continue;
		case kgl_parse_finished:
			rq->mb->model = MULTIPART_BODY_MODEL;
			return ret;
		default:
			return ret;
		}
	}
	return kgl_parse_continue;
}
int KMultiPartInputFilter::hookFileContent(KInputFilterContext* fc, const char* buf, int len)
{
	KFileContentFilterHelper* hook = file_content_head;
	while (hook) {
		if (JUMP_DENY == hook->matchContent(fc, buf, len)) {
			return JUMP_DENY;
		}
		hook = hook->next;
	}
	return JUMP_ALLOW;
}
int KMultiPartInputFilter::handleBody(KInputFilterContext* fc, bool& success)
{
	fc->mb->model = MULTIPART_BODY_MODEL;
	int len;
	bool all = !filename;
	char* buf = parseBody(fc, &len, all);
	if (buf) {
		success = true;
		if (all) {
			fc->mb->model = MULTIPART_BOUNDARY_MODEL;
		}
		if (filename) {
			if (JUMP_DENY == hookFileContent(fc, buf, len)) {
				return JUMP_DENY;
			}
			if (all) {
				if (JUMP_DENY == hookFileContent(fc, buf, len)) {
					return JUMP_DENY;
				}
				cleanFileContentHook();
			}
		} else if (param) {
			len = url_decode(buf, len, NULL, true);
			if (JUMP_DENY == hookParamFilter(fc, param, param_len, buf, len)) {
				return JUMP_DENY;
			}
		}
		free(buf);
	} else {
		success = false;
	}
	return JUMP_ALLOW;
}
char* KMultiPartInputFilter::parseBody(KInputFilterContext* fc, int* len, bool& all)
{
	int max;
	char* bound;
	if (fc->mb->bytes_in_buffer <= 0) {
		return NULL;
	}

	if ((bound = (char*)kgl_memstr(fc->mb->buf_begin, fc->mb->bytes_in_buffer, fc->mb->boundary_next, fc->mb->boundary_next_len))) {
		max = (int)(bound - fc->mb->buf_begin);
		all = true;
	} else {
		if (all) {
			return NULL;
		}
		max = fc->mb->bytes_in_buffer;
	}
	*len = max;
	if (*len > 0) {
		char* buf = (char*)malloc(*len + 1);
		kgl_memcpy(buf, fc->mb->buf_begin, *len);
		buf[*len] = '\0';
		if (bound && buf[*len - 1] == '\r') {
			buf[--(*len)] = 0;
		}
		fc->mb->bytes_in_buffer -= *len;
		fc->mb->buf_begin += *len;
		return buf;
	}
	return NULL;
}
#endif
