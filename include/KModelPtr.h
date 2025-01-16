#ifndef KMODELPTR_H_INCLUDED
#define KMODELPTR_H_INCLUDED
#include "serializable.h"
template<typename T>
class KModelPtr {
public:
	KModelPtr(T* m) {
		revers = false;
		is_or = false;
		this->m = m;
	}
	KModelPtr(const KString& named, T* m) : named{ named } {
		revers = false;
		is_or = false;
		if (!m) {
			throw std::bad_exception();
		}
		this->m = m;
	}
	KModelPtr(const KModelPtr& ptr) :named{ ptr.named } {
		if (ptr.m) {
			this->m = static_cast<T*>(ptr.m->add_ref());
		} else {
			this->m = nullptr;
		}
		this->revers = ptr.revers;
		this->is_or = ptr.is_or;
	}
	~KModelPtr() {
		if (m) {
			m->release();
		}
	}
	explicit operator bool() const noexcept {
		return m != nullptr;
	}
	KModelPtr& operator =(const KModelPtr& ptr) {
		if (this == *ptr) {
			return *this;
		}
		if (m) {
			m->release();
			m = nullptr;
		}
		if (ptr.m) {
			this->m = static_cast<T*>(ptr.m->add_ref());
		}
		this->revers = ptr.revers;
		this->is_or = ptr.is_or;
		this->named = ptr.named;
		return *this;
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) {
		revers = (xml->attributes["revers"] == "1");
		is_or = (xml->attributes["or"] == "1");
		//m->parse_config(xml);
	}
	void dump(kgl::serializable* m, bool is_short) {
		m->add("revers", revers);
		m->add("is_or", is_or);
		if (!named.empty()) {
			m->add("ref", named);
		}
		if (this->m) {
			this->m->dump(m, is_short);
		}
	}
	bool revers;
	bool is_or;
	/* 命名模块的名字 */
	KString named;
	T* m;
};
#endif // !KMODELPTR_H_INCLUDED
