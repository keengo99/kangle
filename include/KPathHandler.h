#ifndef KHTTPD_PATH_HANDLER_H
#define KHTTPD_PATH_HANDLER_H
#include "KMap.h"
#include "kmalloc.h"
#include "kstring.h"
#include "ksapi.h"
#include "KHttpLib.h"
template <class _Ty = void>
struct kgl_map_cmp
{
	_NODISCARD constexpr int operator()(const _Ty* _Left, const _Ty* _Right) const {
		return kgl_file_cmp(_Left->data, _Left->len, _Right->data, _Right->len);
	}
};
template<typename T,typename CMP = kgl_map_cmp<kgl_ref_str_t>>
class KPathHandler
{
public:
	KPathHandler(const char* name, size_t len) {
		handler = nullptr;
		//wide_handler = nullptr;
		this->name = kstring_from2(name, len);
	}
	KPathHandler(kgl_ref_str_t* name) {
		this->name = kstring_refs(name);
		handler = nullptr;
		//wide_handler = nullptr;
	}
	~KPathHandler() {
		if (child) {
			child->iterator([](void* data, void* arg) {
				delete (KPathHandler*)data;
				return iterator_remove_continue;
			}, NULL);
		}
		kstring_release(name);
	}
	int cmp(kgl_ref_str_t* key) {
		CMP c;
		//printf("sizeof(c)=[%d]\n", sizeof(c));
		return c(name, key);
		//return CMP(name->data, name->len, key->data, key->len);
		//return CMP(name, key);
	}
	bool add(const char* name, size_t size, T handler) {
		size_t name_len;
		while (size > 1 && *name == '/') {
			name++;
			size--;
		}
		if (size <= 0 || *name=='/') {
			if (this->handler) {
				return false;
			}
			this->handler = handler;
			return true;
		}
		auto path = (const char *)memchr(name, '/', size);
		if (path) {
			name_len = path - name;
		} else {
			name_len = size;
		}
		if (size <= 0) {
			if (this->handler) {
				return false;
			}
			this->handler = handler;
			return true;
		}
#if 0
		if (*name == '*') {
			if (this->wide_handler) {
				return false;
			}
			this->wide_handler = handler;
			return true;
		}
#endif
		kgl_ref_str_t key;
		key.data = name;
		key.len = name_len;
		int new_flag = 0;
		if (!child) {
			child = new KMap<kgl_ref_str_t, KPathHandler>();
		}
		auto node = child->insert(&key, &new_flag);
		if (new_flag) {
			node->value(new KPathHandler<T,CMP>(name,name_len));
		}
		return node->value()->add(name + name_len, size - name_len, handler);
	}
	T find(const char** name, size_t *size) {
		size_t name_len;
		while (*size > 1 && **name == '/') {
			(*name)++;
			(*size)--;
		}
		if (*size <= 0 || **name == '/') {
			return this->handler;
		}
		auto path = (const char*)memchr(*name, '/', *size);
		if (path) {
			name_len = path - (*name);
		} else {
			name_len = *size;
		}
		if (!child) {
			return this->handler;
		}
		kgl_ref_str_t key;
		key.data = *name;
		key.len = name_len;
		auto node = child->find(&key);
		if (!node) {
			return this->handler;
		}
		*name += name_len;
		*size -= name_len;
		auto value = node->value()->find(name, size);
		if (value) {
			return value;
		}
		return this->handler;
	}
	T remove(const char* name, size_t size) {
		size_t name_len;
		while (size>1 && *name == '/') {
			name++;
			size--;
		}
		if (size <= 0 || *name == '/') {
			auto handler = this->handler;
			this->handler = nullptr;
			return handler;
		}
		auto path = (const char*)memchr(name, '/', size);
		if (path) {
			name_len = path - name;
		} else {
			name_len = size;
		}
#if 0
		if (*name == '*') {
			auto handler = this->wide_handler;
			this->wide_handler = nullptr;
			return handler;
		}
#endif
		if (!child) {
			return nullptr;
		}
		kgl_ref_str_t key;
		key.data = name;
		key.len = name_len;
		auto node = child->find(&key);
		if (!node) {
			return nullptr;
		}
		auto child_handler = node->value();
		auto handler = child_handler->remove(name + name_len, size - name_len);
		if (handler) {
			if (child_node->empty()) {
				delete child_handler;
				child->erase(node);
			}
			if (child->empty()) {
				delete child;
				child = nullptr;
			}
		}
		return handler;
	}
	bool empty() {
		return !handler && !child;
	}
private:
	kgl_ref_str_t* name;
	T handler;
	//T wide_handler;
	KMap<kgl_ref_str_t, KPathHandler> *child = nullptr;
};
#endif
