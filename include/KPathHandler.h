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
class KPathHandler
{
public:
	using iterator_callback = void (*)(KStackName*, T, void*);
	KPathHandler(const char* name, size_t len) {
		handler = nullptr;
		this->name = kstring_from2(name, len);
	}
	KPathHandler(kgl_ref_str_t* name) {
		this->name = kstring_refs(name);
		handler = nullptr;
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
		return c(name, key);
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
			this->handler = handler;
			if (size == 0) {
				this->name->id = FILE_KEY;
			}
			return true;
		}
		if (*name == '*') {
			if (this->handler) {
				return false;
			}
			this->name->id = WIDE_KEY;
			this->handler = handler;
			return true;
		}
		auto path = (const char*)memchr(name, '/', size);
		if (path) {
			name_len = path - name;
		} else {
			name_len = size;
		}
		kgl_ref_str_t key;
		key.data = name;
		key.len = name_len;
		int new_flag = 0;
		if (!child) {
			child = new KMap<kgl_ref_str_t, KPathHandler>();
		}
		auto node = child->insert(&key, &new_flag);
		if (new_flag) {
			node->value(new KPathHandler<T, CMP>(name, name_len));
		}
		return node->value()->add(name + name_len, size - name_len, handler);
	}
	T find(const char** name, size_t* size) {
		size_t name_len;
		while (*size > 1 && **name == '/') {
			(*name)++;
			(*size)--;
		}
		if (*size <= 0) {
			return this->handler;
		}
		if (**name == '/') {
			if (this->name->id == FILE_KEY) {
				return nullptr;
			}
			return this->handler;
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
		if (!child) {
			goto done;
		}
		key.data = *name;
		key.len = name_len;
		node = child->find(&key);
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
		if (this->name->id == WIDE_KEY) {
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
			this->name->id = 0;
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
		auto node = child->find(&key);
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
			if (child->empty()) {
				delete child;
				child = nullptr;
			}
		}
		return handler;
	}
	void iterator(KStackName* name, iterator_callback cb, void* arg) {
		if (!name->push(this->name)) {
			return;
		}
		if (handler) {
			cb(name, handler, arg);
		}
		if (child) {
			for (auto it = child->first(); it; it = it->next()) {
				it->value()->iterator(name, cb, arg);
			}
		}
		name->pop();
	}
	bool empty() {
		return !handler && !child;
	}

private:
	static constexpr uint16_t WIDE_KEY = 1;
	static constexpr uint16_t FILE_KEY = 2;
	kgl_ref_str_t* name;
	T handler;
	KMap<kgl_ref_str_t, KPathHandler>* child = nullptr;
};
#endif
