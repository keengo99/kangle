#include "KMultiPartInputFilter.h"
#include "http.h"
#include "KUrlParser.h"
#if 0
char* my_strndup(const char* s, int length) {
	char* p = (char*)malloc(length + 1);
	if (p == NULL) {
		return p;
	}
	kgl_memcpy(p, s, length);
	p[length] = 0;
	return p;
}
void* kgl_memstr(char* haystack, int haystacklen, char* needle, int needlen) {
	int len = haystacklen;
	char* ptr = haystack;

	/* iterate through first character matches */
	while ((ptr = (char*)memchr(ptr, needle[0], len))) {

		/* calculate length after match */
		len = haystacklen - (ptr - (char*)haystack);

		/* done if matches up to capacity of buffer */
		if (memcmp(needle, ptr, needlen < len ? needlen : len) == 0) {
			return ptr;
		}

		/* next character */
		ptr++; len--;
	}

	return NULL;
}
#endif
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
static char* next_line(multipart_buffer* self) {
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
		self->bytes_in_buffer -= (self->buf_begin - line);

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
static bool find_boundary(multipart_buffer* self) {
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
static char* substring_conf(char* start, int len, char quote) {
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
static char* php_ap_getword_conf(char** line) {
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

		res = substring_conf(str + 1, strend - str - 1, quote);

		if (*strend == quote) {
			++strend;
		}

	} else {

		strend = str;
		while (*strend && !isspace(*strend)) {
			++strend;
		}
		res = substring_conf(str, strend - str, 0);
	}

	while (*strend && isspace(*strend)) {
		++strend;
	}

	*line = strend;
	return res;
}
static char* php_ap_getword(char** line, char stop) {
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

	res = kgl_strndup(*line, pos - *line);

	while (*pos == stop) {
		++pos;
	}

	*line = pos;
	return res;
}
static bool multipart_eof(multipart_buffer* mb, bool isLast) {
	if (isLast) {
		return mb->bytes_in_buffer <= 0;
	}
	return  mb->bytes_in_buffer <= mb->boundary_next_len + 1;
}

static char* get_hdr_value(KHttpHeader* header, const char* attr,size_t attr_len) {
	while (header) {
		if (kgl_is_attr(header, attr, attr_len)) {
			return header->buf + header->val_offset;
		}
		header = header->next;
	}
	return NULL;
}

KMultiPartInputFilter::KMultiPartInputFilter(const char* val, size_t len) {
	memset(&hm, 0, sizeof(hm));
	param = NULL;
	filename = NULL;
	const char* hot = (char*)kgl_memstr(val, len, _KS("boundary"));
	if (!hot) {
		char* content_type_lcase = kgl_strndup(val, len);
		string2lower2(content_type_lcase, len);
		hot = strstr(content_type_lcase, "boundary");
		if (hot) {
			hot = val + (hot - content_type_lcase);
		}
		free(content_type_lcase);
	}
	const char* end = val + len;
	if (!hot || !(hot = (char*)memchr(hot, '=', end - hot))) {
		//sapi_module.sapi_error(E_WARNING, "Missing boundary in multipart/form-data POST data");
		return;
	}
	hot++;
	size_t boundary_len = end - hot;

	const char* boundary_end;
	if (hot[0] == '"') {
		hot++;
		boundary_end = (char*)memchr(hot, '"', boundary_len);
		if (!boundary_end) {
			//  sapi_module.sapi_error(E_WARNING, "Invalid boundary in multipart/form-data POST data");
			return;
		}
	} else {
		/* search for the end of the boundary */
		boundary_end = kgl_mempbrk(hot, boundary_len, _KS(",;"));
	}
	if (boundary_end) {
		boundary_len = boundary_end - hot;
	}
	mb = new multipart_buffer;
	mb->boundary = (char*)malloc(boundary_len + 3);
	mb->boundary[0] = mb->boundary[1] = '-';
	kgl_memcpy(mb->boundary + 2, hot, boundary_len);
	mb->boundary[boundary_len + 2] = '\0';
	mb->boundary_next = (char*)malloc(boundary_len + 4);
	mb->boundary_next[0] = '\n';
	kgl_memcpy(mb->boundary_next + 1, mb->boundary, boundary_len + 2);
	mb->boundary_next_len = (int)(boundary_len + 3);
	mb->boundary_next[mb->boundary_next_len] = '\0';
}
bool KMultiPartInputFilter::match(KInputFilterContext* rq, const char* str, int len, bool isLast) {
	if (!mb) {
		return false;
	}
	if (str) {
		mb->add(str, len);
	}
	while (!multipart_eof(mb, isLast)) {
		if (mb->model == MULTIPART_BODY_MODEL) {
			bool success;
			if (match_body(success)) {
				return true;
			}
			if (!success) {
				return false;
			}
			continue;
		}
		if (mb->model == MULTIPART_BOUNDARY_MODEL) {
			if (!find_boundary(mb)) {
				return JUMP_ALLOW;
			}
			mb->model = MULTIPART_HEAD_MODEL;
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
		switch (parse_header()) {
		case kgl_parse_finished:
			break;
		case kgl_parse_error:
			delete mb;
			mb = NULL;
			return false;
		default:
			return false;
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
				for (auto it = file_list.begin(); it != file_list.end(); ++it) {
					if ((*it)->check_name(param, param_len, filename, filename_len, hm.header)) {
						return true;
					}
				}
			}
			if (hm.header) {
				xfree_header(hm.header);
				memset(&hm, 0, sizeof(hm));
			}

		}
	}
	return false;
}
kgl_parse_result KMultiPartInputFilter::parse_header() {
	khttp_parser parser;
	khttp_parse_result rs;
	memset(&parser, 0, sizeof(parser));
	parser.first_same = 1;
	while (mb->bytes_in_buffer > 0) {
		memset(&rs, 0, sizeof(rs));
		char* end = mb->buf_begin + mb->bytes_in_buffer;
		kgl_parse_result ret = khttp_parse(&parser, &mb->buf_begin, end, &rs);
		mb->bytes_in_buffer = end - mb->buf_begin;

		switch (ret) {
		case kgl_parse_continue:
		case kgl_parse_error:
			return ret;
		case kgl_parse_success:
			hm.add_header(rs.attr, rs.attr_len, rs.val, rs.val_len);
			continue;
		case kgl_parse_finished:
			mb->model = MULTIPART_BODY_MODEL;
			return ret;
		}
	}
	return kgl_parse_continue;
}

bool KMultiPartInputFilter::match_file_content(const char* buf, int len) {
	for (auto it = file_list.begin(); it != file_list.end(); ++it) {
		if ((*it)->check_content(buf, len)) {
			return true;
		}
	}
	return false;
}
bool KMultiPartInputFilter::match_body(bool& success) {
	mb->model = MULTIPART_BODY_MODEL;
	int len;
	bool all = !filename;
	char* buf = parse_body(&len, all);
	if (buf) {
		success = true;
		if (all) {
			mb->model = MULTIPART_BOUNDARY_MODEL;
		}
		if (filename) {
			if (match_file_content(buf, len)) {
				return true;
			}
			if (all) {
				if (match_file_content(buf, len)) {
					return true;
				}
				file_list.clear();
			}
		} else if (param) {
			len = url_decode(buf, len, NULL, true);
			if (match_param(param, param_len, buf, len)) {
				return true;
			}
		}
		free(buf);
	} else {
		success = false;
	}
	return false;
}
char* KMultiPartInputFilter::parse_body(int* len, bool& all) {
	int max;
	char* bound;
	if (mb->bytes_in_buffer <= 0) {
		return NULL;
	}

	if ((bound = (char*)kgl_memstr(mb->buf_begin, mb->bytes_in_buffer, mb->boundary_next, mb->boundary_next_len))) {
		max = bound - mb->buf_begin;
		all = true;
	} else {
		if (all) {
			return NULL;
		}
		max = mb->bytes_in_buffer;
	}
	*len = max;
	if (*len > 0) {
		char* buf = (char*)malloc(*len + 1);
		kgl_memcpy(buf, mb->buf_begin, *len);
		buf[*len] = '\0';
		if (bound && buf[*len - 1] == '\r') {
			buf[--(*len)] = 0;
		}
		mb->bytes_in_buffer -= *len;
		mb->buf_begin += *len;
		return buf;
	}
	return NULL;
}
