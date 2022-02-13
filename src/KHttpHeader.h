#ifndef KHTTPHEADER_H_
#define KHTTPHEADER_H_
#include "ksapi.h"
#include "kmalloc.h"
#include "kstring.h"
KBEGIN_DECLS

struct kgl_keyval_t {
	kgl_str_t   key;
	kgl_str_t   value;
};
extern kgl_str_t kgl_header_type_string[];
void *kgl_memstr(char *haystack, int haystacklen, char *needle, int needlen);
#define kgl_cpymem(dst, src, n)   (((u_char *) kgl_memcpy(dst, src, n)) + (n))
typedef void(*kgl_header_callback)(KOPAQUE data, void *arg, const char *attr, int attr_len, const char *val, int val_len);
inline KHttpHeader *new_pool_http_header(kgl_pool_t *pool,const char *attr, int attr_len, const char *val, int val_len) {
	if (attr_len > MAX_HEADER_ATTR_VAL_SIZE || val_len > MAX_HEADER_ATTR_VAL_SIZE) {
		return NULL;
	}
	KHttpHeader *header = (KHttpHeader *)kgl_pnalloc(pool,sizeof(KHttpHeader));
	header->next = NULL;
	header->attr = (char *)kgl_pnalloc(pool,attr_len + 1);
	kgl_memcpy(header->attr, attr, attr_len);
	header->attr[attr_len] = '\0';
	header->attr_len = attr_len;
	header->val = (char *)kgl_pnalloc(pool, val_len + 1);
	kgl_memcpy(header->val, val, val_len);
	header->val[val_len] = '\0';
	header->val_len = val_len;
	return header;
}
inline KHttpHeader *new_http_header(const char *attr,int attr_len,const char *val,int val_len) {
	if (attr_len > MAX_HEADER_ATTR_VAL_SIZE || val_len>MAX_HEADER_ATTR_VAL_SIZE) {
		return NULL;
	}
	KHttpHeader *header = (KHttpHeader *)malloc(sizeof(KHttpHeader));
	header->next = NULL;
	header->attr = (char *)malloc(attr_len+1);
	kgl_memcpy(header->attr,attr,attr_len);
	header->attr[attr_len] = '\0';
	header->attr_len = attr_len;
	header->val = (char *)malloc(val_len+1);
	kgl_memcpy(header->val,val,val_len);
	header->val[val_len] = '\0';
	header->val_len = val_len;
	return header;
}
inline void free_header(KHttpHeader *av) {
	KHttpHeader *next;
	while (av) {
		next = av->next;
		free(av->attr);
		free(av->val);
		free(av);
		av = next;
	}
}
bool is_attr(KHttpHeader *av, const char *attr,int attr_len);
inline char *strlendup(const char *str, int len)
{
	char *buf = (char *)malloc(len + 1);
	memcpy(buf, str, len);
	buf[len] = '\0';
	return buf;
}
KEND_DECLS
#endif /*KHTTPHEADER_H_*/
