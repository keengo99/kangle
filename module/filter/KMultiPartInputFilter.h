#ifndef KMULTIPARTINPUTFILTER_H
#define KMULTIPARTINPUTFILTER_H
#include <limits.h>
#include <list>
#include "KInputFilter.h"
#include "KHttpHeaderManager.h"
#include "KHttpParser.h"

#define FILLUNIT (1024 * 5)
#define MULTIPART_BOUNDARY_MODEL 0
#define MULTIPART_HEAD_MODEL     1
#define MULTIPART_BODY_MODEL     2
/* most bytes of an unfinished part header kept while waiting for its end */
#define MULTIPART_MAX_HEAD       65536

class multipart_buffer {
public:
	multipart_buffer() : buffer(NULL), buf_begin(NULL), bufsize(0),
		bytes_in_buffer(0), boundary(NULL), boundary_next(NULL),
		boundary_next_len(0), model(0) {}
	~multipart_buffer() {
		if (buffer) {
			free(buffer);
		}
		if (boundary) {
			free(boundary);
		}
		if (boundary_next) {
			free(boundary_next);
		}
	}
	bool add(const char* str, int len) {
		if (len < 0 || bytes_in_buffer < 0 || len > INT_MAX - 1 - bytes_in_buffer) {
			return false;
		}
		/* Move the kept bytes to the front and grow by doubling. Copying
		 * all kept data into a new buffer on every read made a long field
		 * cost quadratic time. */
		if (buffer && buf_begin != buffer) {
			if (bytes_in_buffer > 0) {
				memmove(buffer, buf_begin, bytes_in_buffer);
			}
			buf_begin = buffer;
		}
		int need = bytes_in_buffer + len + 1;
		if (need > capacity) {
			int size = capacity > 0 ? capacity : FILLUNIT;
			while (size < need) {
				size = (size > INT_MAX / 2) ? need : size * 2;
			}
			char* t = (char*)realloc(buffer, size);
			if (t == NULL) {
				return false;
			}
			buffer = t;
			capacity = size;
		}
		kgl_memcpy(buffer + bytes_in_buffer, str, len);
		bytes_in_buffer += len;
		buffer[bytes_in_buffer] = '\0';
		bufsize = bytes_in_buffer;
		buf_begin = buffer;
		return true;
	}
	/* read buffer */
	char* buffer;
	char* buf_begin;
	int  bufsize;
	int  bytes_in_buffer;
	int  capacity = 0;
	/* bytes from buf_begin already searched for boundary_next */
	int  searched = 0;

	/* boundary info */
	char* boundary;
	char* boundary_next;
	int  boundary_next_len;
	int  model;
};
class KMultiPartInputFilter : public KParamInputFilter
{
public:
	KMultiPartInputFilter(const char* val, size_t len);
	~KMultiPartInputFilter() {
		if (hm.header) {
			free_header_list(hm.header);
		}
		if (param) {
			free(param);
		}
		if (filename) {
			free(filename);
		}
		if (mb) {
			delete mb;
		}
	}
	bool match(KInputFilterContext* rq, const char* str, int len, bool isLast) override;
	bool register_file(KFileFilterHook* filter) override {
		file_list.emplace_back(filter->add_ref());
		return true;
	}
private:
	bool match_body(bool& success, bool isLast);
	char* parse_body(int* len, bool& all, bool isLast);
	kgl_parse_result parse_header();
	bool match_file_content(const char* buf, int len);
	KHttpHeaderManager hm;
	char* param;
	int param_len;
	char* filename;
	int filename_len;
	std::list<KFileFilter> file_list;
	multipart_buffer* mb = nullptr;
};
#endif
