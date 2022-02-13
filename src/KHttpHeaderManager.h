#ifndef KHTTPHEADERMANAGER_H_99
#define KHTTPHEADERMANAGER_H_99
#include "KHttpHeader.h"

class KHttpHeaderManager {
public:
	void Append(KHttpHeader *new_t)
	{
		if (header == NULL) {
			header = last = new_t;
			return;
		}
		new_t->next = header;
		header = new_t;
	}
	void Insert(KHttpHeader *new_t)
	{
		if (header == NULL) {
			header = last = new_t;
			return;
		}
		kassert(last);
		last->next = new_t;
		last = new_t;
		return;
	}
	bool AddHeader(const char *attr, int attr_len, const char *val, int val_len, bool tail = true)
	{
		return AddHeader(kgl_header_unknow, attr, attr_len, val, val_len, tail);
	}
	bool AddHeader(kgl_header_type type, const char *attr, int attr_len, const char *val, int val_len, bool tail = true)
	{
		if (attr_len > MAX_HEADER_ATTR_VAL_SIZE || val_len > MAX_HEADER_ATTR_VAL_SIZE) {
			return false;
		}
		KHttpHeader *new_t = new KHttpHeader;
		if (new_t == NULL) {
			return false;
		}
		memset(new_t, 0, sizeof(KHttpHeader));
		new_t->type = type;
		if (attr) {
			new_t->attr = strlendup(attr, attr_len);
		}
		new_t->attr_len = attr_len;
		new_t->val = strlendup(val, val_len);
		new_t->val_len = val_len;
		new_t->next = NULL;
		if (tail) {
			Insert(new_t);
			return true;
		}
		Append(new_t);
		return true;
	}
	KHttpHeader *FindHeader(const char *attr, int len)
	{
		KHttpHeader *l = header;
		while (l) {
			if (is_attr(l, attr, len)) {
				return l;
			}
			l = l->next;
		}
		return NULL;
	}
	KHttpHeader *RemoveHeader(const char *attr)
	{
		KHttpHeader *l = header;
		KHttpHeader *prev = NULL;
		while (l) {
			if (strcasecmp(l->attr, attr) == 0) {
				if (prev) {
					prev->next = l->next;
				} else {
					header = l->next;
				}
				return l;
			}
			prev = l;
			l = l->next;
		}
		return NULL;
	}
	KHttpHeader *GetHeader()
	{
		return header;
	}
	KHttpHeader *StealHeader()
	{
		KHttpHeader *h = header;
		header = last = NULL;
		return h;
	}
	KHttpHeader *header;
	KHttpHeader *last;
};
#endif
