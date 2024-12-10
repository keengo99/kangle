#include "KVirtualHostContainer.h"
#include "KSubVirtualHost.h"
#include "KVirtualHost.h"
#include "klist.h"

static int vh_container_find_cmp(const void* k, const void* k2) {
	domain_t s1 = (domain_t)k;
	KDomainMap* s2 = (KDomainMap*)k2;
	return kgl_domain_cmp(s1, s2->name);
}
KVirtualHostContainer::KVirtualHostContainer() {

}
KVirtualHostContainer::~KVirtualHostContainer() {

}
static iterator_ret clear_iterator(void* data, void* argv) {
	KDomainMap* vhc = (KDomainMap*)data;
	vhc->clear((kgl_cleanup_f)argv);
	delete vhc;
	return iterator_remove_continue;
}
void KDomainMap::clear(kgl_cleanup_f handler) {
	KBindVirtualHost* next;
	while (list) {
		next = list->next;
		if (handler) {
			handler(list->svh);
		}
		delete list;
		list = next;
	}
	while (wide_list) {
		next = wide_list->next;
		if (handler) {
			handler(wide_list->svh);
		}
		delete wide_list;
		wide_list = next;
	}
	rbtree_iterator(&tree, clear_iterator, (void*)handler);
}
static iterator_ret list_domain_iterator(void* data, void* argv) {
	KDomainMap* vhc = (KDomainMap*)data;
	inter_domain_iterator_arg* it = (inter_domain_iterator_arg*)argv;
	vhc->iterator(it);
	return iterator_continue;
}
char* build_domain_name(domain_t src, int len, bool wide) {
	const unsigned char* stack[256];
	int stack_size = 0;
	const unsigned char** hot = stack;
	char* buf = (char*)malloc(len + 4);
	char* dst = buf;
	if (wide) {
		*dst++ = '*';
	}
	while (len > 0) {
		unsigned char lable_len = src[0];
		if (lable_len == 0) {
			break;
		}
		len -= (lable_len + 1);
		if (len < 0) {
			xfree(buf);
			return NULL;
		}
		(*hot) = src;
		hot++;
		stack_size++;
		src += lable_len + 1;
	}
	while (stack_size-- > 0) {
		hot--;
		if (dst != buf) {
			*dst++ = '.';
		}
		const unsigned char* lable = *hot;
		unsigned char lable_len = lable[0];
		lable++;
		while (lable_len-- > 0) {
			*dst++ = *lable++;
		}
	}
	*dst = '\0';
	return buf;
}
void iterator_call_back(inter_domain_iterator_arg* it, bool wide, void* vh) {
	char* domain = build_domain_name(it->domain, it->domain_len, wide);
	it->it(it->arg, domain, vh);
	free(domain);
}
void KDomainMap::iterator(inter_domain_iterator_arg* it) {
	domain_t domain = NULL;
	domain_t orig_domain = it->domain;
	int orig_domain_len = it->domain_len;
	if (name) {
		int new_len = it->domain_len + *name + 1;
		domain = (domain_t)malloc(new_len + 1);
		if (it->domain) {
			kgl_memcpy(domain, it->domain, it->domain_len);
		}
		kgl_memcpy(domain + it->domain_len, name, *name + 1);
		it->domain_len = new_len;
		it->domain = domain;
	}
	KBindVirtualHost* next = list;
	while (next) {
		iterator_call_back(it, false, next->svh);
		next = next->next;
	}
	next = wide_list;
	while (next) {
		iterator_call_back(it, true, next->svh);
		next = next->next;
	}
	rbtree_iterator(&tree, list_domain_iterator, it);
	if (domain) {
		free(domain);
	}
	it->domain = orig_domain;
	it->domain_len = orig_domain_len;
}
void KDomainMap::iterator(domain_iterator it, void* arg) {
	inter_domain_iterator_arg it_arg;
	memset(&it_arg, 0, sizeof(inter_domain_iterator_arg));
	it_arg.it = it;
	it_arg.arg = arg;
	this->iterator(&it_arg);
}
bool convert(const char* domain, bool& wide, domain_t buf, int buf_size) {
	if (*domain == '*') {
		wide = true;
		domain++;
	} else {
		wide = false;
	}
	return revert_hostname(domain, (int)strlen(domain), buf, buf_size);
}
void KVirtualHostContainer::bind_vh(KVirtualHost* vh, bool high) {
	auto lock = get_locker();
	for (auto it2 = vh->hosts.begin(); it2 != vh->hosts.end(); ++it2) {
		if (root.add((*it2)->bind_host, (*it2)->wide, high ? kgl_bind_high : kgl_bind_low, (*it2))) {
			++count;
			++total_count;
		}
	}
}
void KVirtualHostContainer::unbind_vh(KVirtualHost* vh) {
	auto lock = get_locker();
	for (auto it2 = vh->hosts.begin(); it2 != vh->hosts.end(); ++it2) {
		if (kgl_del_failed != root.del((*it2)->bind_host, (*it2)->wide, (*it2))) {
			--count;
		}
	}
}
bool KDomainMap::bind(const char* domain, void* vh, kgl_bind_level level) {
	unsigned char hostname[256];
	bool wide;
	if (!convert(domain, wide, hostname, sizeof(hostname))) {
		return false;
	}
	return add(hostname, wide, level, vh);
}
void* KDomainMap::unbind(const char* domain) {
	unsigned char hostname[256];
	bool wide;
	if (!convert(domain, wide, hostname, sizeof(hostname))) {
		return nullptr;
	}
	return del(hostname, wide);
}
kgl_del_result KDomainMap::unbind(const char* domain, void* vh) {
	unsigned char hostname[256];
	bool wide;
	if (!convert(domain, wide, hostname, sizeof(hostname))) {
		return kgl_del_failed;
	}
	return del(hostname, wide, vh);
}
void* KDomainMap::find(const char* domain) {
	unsigned char hostname[256];
	bool wide;
	if (!convert(domain, wide, hostname, sizeof(hostname))) {
		return nullptr;
	}
	return find(hostname, wide);
}
void* KDomainMap::find(domain_t name, bool wide) {
	if (!*name) {
		//@解析了
		if (unlikely(wide)) {
			return wide_list ? wide_list->svh : nullptr;
		}
		return list ? list->svh : nullptr;
	}

	krb_node* node = rbtree_find(&tree, name, vh_container_find_cmp);
	if (node) {
		//精确解析
		name = name + *name + 1;
		KDomainMap* rn = (KDomainMap*)node->data;
		void* ret = rn->find(name, wide);
		if (ret) {
			return ret;
		}
	}
	if (wide_list) {
		//泛解析
		return wide_list->svh;
	}
	return nullptr;
}
bool KDomainMap::add(domain_t name, bool wide, kgl_bind_level level, void* svh) {
	if (!*name) {
		KBindVirtualHost* bl = new KBindVirtualHost(svh);
		if (level == kgl_bind_high) {
			if (wide) {
				bl->next = wide_list;
				wide_list = bl;
			} else {
				bl->next = list;
				list = bl;
			}
		} else {
			KBindVirtualHost** l;
			if (wide) {
				l = &wide_list;
			} else {
				l = &list;
			}
			if (level == kgl_bind_unique && *l != NULL) {
				delete bl;
				return false;
			}
			KBindVirtualHost* last = *l;
			while (last && last->next) {
				//assert no double bind
				assert(last->svh != svh);
				last = last->next;
			}
			if (last) {
				//assert no double bind
				assert(last->svh != svh);
				last->next = bl;
			} else {
				*l = bl;
			}
		}
		return true;
	}
	KDomainMap* rn;
	int new_flag = 0;
	krb_node* node = rbtree_insert(&tree, name, &new_flag, vh_container_find_cmp);
	assert(node);
	if (new_flag) {
		rn = new KDomainMap;
		rn->name = (domain_t)malloc(*name + 1);
		kgl_memcpy(rn->name, name, *name + 1);
		node->data = rn;
	} else {
		rn = (KDomainMap*)node->data;
	}
	return rn->add(name + *name + 1, wide, level, svh);
}
void* KDomainMap::del(domain_t name, bool wide) {
	if (!*name) {
		KBindVirtualHost** bl;
		if (wide) {
			bl = &wide_list;
		} else {
			bl = &list;
		}
		if (!(*bl)) {
			return nullptr;
		}
		KBindVirtualHost* cur = *bl;
		*bl = (*bl)->next;
		auto ret = (*bl)->svh;
		delete cur;
		return ret;
	}
	krb_node* node = rbtree_find(&tree, name, vh_container_find_cmp);
	if (node == NULL) {
		return nullptr;
	}
	KDomainMap* rn = (KDomainMap*)node->data;
	auto ret = rn->del(name + *name + 1, wide);
	if (rn->is_empty()) {
		delete rn;
		rbtree_remove(&tree, node);
	}
	return ret;
}
kgl_del_result KDomainMap::del(domain_t name, bool wide, void* svh) {
	if (!*name) {
		KBindVirtualHost** bl;
		if (wide) {
			bl = &wide_list;
		} else {
			bl = &list;
		}
		KBindVirtualHost* last = NULL;
		KBindVirtualHost* cur = *bl;
		while (cur) {
			if (cur->svh == svh) {
				if (last) {
					last->next = cur->next;
				} else {
					*bl = cur->next;
				}
				delete cur;
				if (this->is_empty()) {
					return kgl_del_empty;
				}
				return kgl_del_success;
			}
			last = cur;
			cur = cur->next;
		}
		return kgl_del_failed;
	}
	krb_node* node = rbtree_find(&tree, name, vh_container_find_cmp);
	if (node == NULL) {
		return kgl_del_failed;
	}
	KDomainMap* rn = (KDomainMap*)node->data;
	kgl_del_result ret = rn->del(name + *name + 1, wide, svh);
	if (ret == kgl_del_empty) {
		delete rn;
		rbtree_remove(&tree, node);
		if (!is_empty()) {
			ret = kgl_del_success;
		}
	}
	return ret;
}
