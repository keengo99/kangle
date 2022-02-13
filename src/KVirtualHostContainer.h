#ifndef KVIRTUALHOSTCONTAINER_H
#define KVIRTUALHOSTCONTAINER_H
#include "krbtree.h"
#include <string.h>
#include "KHttpHeader.h"

typedef unsigned char * domain_t;
class KSubVirtualHost;
class KHttpRequest;
class KVirtualHost;
#define kgl_tolower(c)      (unsigned char) ((c >= 'A' && c <= 'Z') ? (c | 0x20) : c)
#define kgl_toupper(c)      (unsigned char) ((c >= 'a' && c <= 'z') ? (c & ~0x20) : c)
inline int memn2cmp(unsigned char *s1, unsigned char *s2)
{
	unsigned char n;
    int  m, z;
    if (*s1 <= *s2) {
        n = *s1;
        z = -1;
    } else {
        n = *s2;
        z = 1;
    }
    m = memcmp(s1 + 1, s2 + 1, n);
    if (m || *s1 == *s2) {
		return m;
    }
    return z;
}
inline bool revert_hostname(const char *src,int len,domain_t dst,int dst_len)
{
	kgl_str_t stack[256];
	int stack_size = 0;
	kgl_str_t *hot = stack;
	while (len>0) {
		hot->data = (char *)src;
		const char *p = (const char *)memchr(src,'.',len);
		if (p) {
			hot->len = p - src;
			len -= (int)(hot->len + 1);
			src = (p+1);
		} else {
			hot->len = len;
			len = 0;
		}
		if (hot->len>63 || stack_size>250) {
			*dst = '\0';
			return false;
		}
		if (hot->len == 0) {
			continue;
		}
		hot++;
		stack_size++;
	}
	while (stack_size-->0) {
		hot--;
		dst_len -= (int)(hot->len + 1);
		if (dst_len<=0) {
			*dst = '\0';
			return false;
		}
		*dst++ = (unsigned char)(hot->len);
		while (hot->len-->0) {
			*dst++ = kgl_tolower(*hot->data);
			hot->data++;
		}
	}
	*dst = '\0';
	return true;
}
enum kgl_bind_level {
	kgl_bind_high,
	kgl_bind_low,
	kgl_bind_unique
};
enum query_vh_result
{
	query_vh_success,
	query_vh_connect_limit,
	query_vh_host_not_found,
	query_vh_resolve,
	query_vh_unknow
};
enum kgl_del_result {
	kgl_del_success,
	kgl_del_failed,
	kgl_del_empty
};
class KBindVirtualHost
{
public:
	KBindVirtualHost(void *svh)
	{
		this->svh = svh;
		next = NULL;
	}
	KBindVirtualHost()
	{
		this->svh = NULL;
	}
	void *svh;
	KBindVirtualHost *next;
};
typedef void (*domain_iterator)(void *arg,const char *domain,void *vh);
struct inter_domain_iterator_arg {
	domain_iterator it;
	void *arg;
	domain_t domain;
	int domain_len;
};

class KVirtualHostContainer
{
public:
	KVirtualHostContainer();
	~KVirtualHostContainer();
	void iterator(domain_iterator it,void *arg);
	void iterator(inter_domain_iterator_arg *it);
	bool bind(const char *domain,void *vh,kgl_bind_level high);
	kgl_del_result unbind(const char *domain,void *vh);
	void *find(const char *domain);

	bool add(domain_t name, bool wide, kgl_bind_level high, void *vh);
	kgl_del_result del(domain_t name, bool wide, void *vh);
	void *find(domain_t name,bool wide=false);
	void clear(kgl_cleanup_f handler=NULL);
	void Destroy(kgl_cleanup_f handler);
	bool isEmpty() {
		if (wide_list==NULL && list==NULL && tree==NULL) {
			return true;
		}
		return false;
	}
	domain_t name;
private:
	krb_tree *tree;
	KBindVirtualHost *wide_list;
	KBindVirtualHost *list;
};
#endif
