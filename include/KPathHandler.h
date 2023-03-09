#ifndef KHTTPD_PATH_HANDLER_H
#define KHTTPD_PATH_HANDLER_H
#include "KMap.h"
#include "kmalloc.h"
#include "kstring.h"
#include "ksapi.h"
#include "KHttpLib.h"
#include "KStackName.h"


struct kgl_map_cmp
{
	int operator()(const kgl_ref_str_t* _Left, const kgl_ref_str_t* _Right) const {
		return kgl_file_cmp(_Left->data, _Left->len, _Right->data, _Right->len);
	}
};
template<typename T, typename CMP = kgl_map_cmp>
class KPathHandler : private CMP
{
public:
	using iterator_callback = void (*)(KStackName*, T, bool, void*);
	KPathHandler(const char* name, size_t len) {
		handler = nullptr;
		this->key = (uintptr_t)kstring_from2(name, len);
	}
	KPathHandler(kgl_ref_str_t* name) {
		this->key = (uintptr_t)kstring_refs(name);
		handler = nullptr;
	}
	~KPathHandler() noexcept {
		child.iterator([](void* data, void* arg) {
			delete (KPathHandler*)data;
			return iterator_remove_continue;
			}, NULL);
		kstring_release(get_name());
	}
	int cmp(const kgl_ref_str_t* key) const {
		return this->operator()(get_name(), key);
	}
	bool add(const char* name, size_t size, T handler) {
		size_t name_len;
		while (size > 1 && *name == '/') {
			name++;
			size--;
		}
		if (size <= 0 || *name == '/') {
			if (this->handler) {
				return false;
			}
			bind_handler(handler, false);
			return true;
		}
		if (*name == '*') {
			if (this->handler) {
				return false;
			}
			bind_handler(handler, true);
			return true;
		}
		auto path = (const char*)memchr(name, '/', size);
		if (path) {
			name_len = path - name;
		} else {
			name_len = size;
		}
		kgl_ref_str_t key;
		key.data = (char *)name;
		key.len = name_len;
		int new_flag = 0;
		auto node = child.insert(&key, &new_flag);
		if (new_flag) {
			node->value(new KPathHandler<T, CMP>(name, name_len));
		}
		return node->value()->add(name + name_len, size - name_len, handler);
	}
	T find(const char** name, size_t* size) const {
		size_t name_len;
		while (*size > 1 && **name == '/') {
			(*name)++;
			(*size)--;
		}
		if (*size <= 0) {
			return this->handler;
		}
		if (**name == '/') {
			goto done;
		}
		auto path = (const char*)memchr(*name, '/', *size);
		if (path) {
			name_len = path - (*name);
		} else {
			name_len = *size;
		}
		kgl_ref_str_t key;
		KMapNode<KPathHandler>* node;
		T value;
		key.data = (char *)*name;
		key.len = name_len;
		node = child.find(&key);
		if (!node) {
			goto done;
		}
		*name += name_len;
		*size -= name_len;
		value = node->value()->find(name, size);
		if (value) {
			return value;
		}
	done:
		if (is_wide_handler()) {
			//is wide
			return this->handler;
		}
		return nullptr;
	}
	T remove(const char* name, size_t size) {
		size_t name_len;
		while (size > 1 && *name == '/') {
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
		if (!child) {
			return nullptr;
		}
		kgl_ref_str_t key;
		key.data = name;
		key.len = name_len;
		auto node = child.find(&key);
		if (!node) {
			return nullptr;
		}
		auto child_handler = node->value();
		auto handler = child_handler->remove(name + name_len, size - name_len);
		if (handler) {
			if (child_handler->empty()) {
				delete child_handler;
				child->erase(node);
			}			
		}
		return handler;
	}
	void iterator(KStackName* name, iterator_callback cb, void* arg) {
		if (!name->push(this->name)) {
			return;
		}
		if (handler) {
			cb(name, handler, is_wide_handler(), arg);
		}		
		for (auto it = child.first(); it; it = it.next()) {
			it->value()->iterator(name, cb, arg);
		}		
		name->pop();
	}
	bool empty() const {
		return !handler && child.empty();
	}
	kgl_ref_str_t* get_name() const {
		return ((kgl_ref_str_t*)(key & ~1));
	}
private:
	void bind_handler(T handler, bool is_wide) {
		if (is_wide) {
			key |= 1;
		} else {
			key &= ~1;
		}
		this->handler = handler;
	}
	bool is_wide_handler() const {
		return (key & 1);
	}
	uintptr_t key;
	T handler;
	KMap<kgl_ref_str_t, KPathHandler> child;
};
#endif
