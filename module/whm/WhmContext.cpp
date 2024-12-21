/*
 * WhmContext.cpp
 *
 *  Created on: 2010-8-30
 *      Author: keengo
 */
#include "WhmContext.h"
using namespace std;

WhmContext::WhmContext(KServiceProvider* provider) {
	this->provider = provider;
	//status = WHM_OK;
	vh = NULL;
	ds = NULL;
}

WhmContext::~WhmContext() {
	if (vh) {
		vh->release();
	}
	if (ds) {
		delete ds;
	}
	list<char*>::iterator it2;
	for (it2 = memorys.begin(); it2 != memorys.end(); it2++) {
		xfree((*it2));
	}
}
char* WhmContext::save(char* p) {
	memorys.push_back(p);
	return p;
}
bool WhmContext::add(const char* name, INT64 value) {
	return data()->add(name, value);
}
void WhmContext::add(const char* name, KString& value) {
	add(name, value.c_str());
}
bool WhmContext::add(const char* name, const char* value, bool encode) {
	return data()->add(name, value);
}
bool WhmContext::add(const KString& name, const KString& value) {
	return data()->add(name, value);
}

void WhmContext::buildVh(KVirtualHost* vh) {
	if (this->vh) {
		this->vh->release();
	}
	if (ds) {
		delete ds;
		ds = NULL;
	}
	this->vh = vh;
	if (vh) {
		ds = new KExtendProgramString("whm", vh);
	}
}
bool WhmContext::buildVh() {
	const char* name = urlValue.getx("vh");
	if (name == NULL) {
		return false;
	}
	if (ds) {
		delete ds;
		ds = NULL;
	}
	if (this->vh) {
		this->vh->release();
	}
	vh = conf.gvm->refsVirtualHostByName(name);
	if (vh) {
		ds = new KExtendProgramString("whm", vh);
	}
	return true;
}
bool WhmContext::flush(int status, kgl::format fmt) {
	KWStream* out = getOutputStream();
	KReadWriteBuffer s;
	if (status > 0) {
		if (fmt != kgl::format::json) {
			s << "<result status=\"" << status;
			if (status_msg.size() > 0) {
				s << " " << status_msg;
			}
			s << "\">\n";
		} else {
			s << "\"status\":\"" << status;
			if (status_msg.size() > 0) {
				s << " " << status_msg;
			}
			s << "\",\"result\":{\n";
		}
	}
	dv.build(s, fmt);
	if (status > 0) {
		if (fmt != kgl::format::json) {
			s << "</result>\n";
		} else {
			s << "}";
		}
	}
	auto head = s.getHead();
	while (head) {
		auto result = out->write_all(head->data, head->used);
		if (result != KGL_OK) {
			return result;
		}
		head = head->next;
	}
	return KGL_OK;
}
