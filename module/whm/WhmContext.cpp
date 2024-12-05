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
void WhmContext::add(const char* name, INT64 value) {
	std::stringstream s;
	s << value;
	return add(name, s.str().c_str());
}
void WhmContext::add(const char* name, KString& value) {
	add(name, value.c_str());
}
WhmDataValue::WhmDataValue() {
	is_child = true;
	encode = false;
	this->attr = new WhmDataAttribute();
}
WhmDataValue::~WhmDataValue() {
	if (is_child) {
		delete this->attr;
	} else {
		delete this->value;
	}
}
static void build_json_string(KWStream& s, const KString& v) {
	s << "\""_CS;
	for (int i = 0; i < v.size(); ++i) {
		if (v[i] == '\\' || v[i] == '"') {
			s << "\\";
		}
		s << v[i];
	}
	s << "\""_CS;
}
void WhmDataValue::build(const KString& name, KWStream& s, int format) {
	if (format == OUTPUT_FORMAT_XML) {
		if (is_child) {
			s << "<"_CS << name << ">"_CS;
			attr->build(s, format);
			s << "</"_CS << name << ">"_CS;
			return;
		}
		for (auto it = value->begin(); it != value->end(); ++it) {
			s << "<"_CS << name << ">"_CS;
			if (encode) {
				s << CDATA_START << (*it) << CDATA_END;
			} else {
				s << *it;
			}
			s << "</"_CS << name << ">"_CS;
		}
	} else if (format == OUTPUT_FORMAT_JSON) {
		if (is_child) {
			s << "\""_CS << name << "\":{"_CS;
			attr->build(s, format);
			s << "}"_CS;
			return;
		}
		if (value->size() == 1) {
			s << "\""_CS << name << "\":"_CS;
			build_json_string(s, *value->begin());
			return;
		}
		s << "\""_CS << name << "\":["_CS;
		for (auto it = value->begin(); it != value->end(); ++it) {
			if (it != value->begin()) {
				s << ","_CS;
			}
			build_json_string(s, *it);
		}
		s << "]";

	}
}

void WhmContext::add(const char* name, const char* value, bool encode) {
	return data()->add(name, value, encode);
}
void WhmContext::add(KString& name, KString& value) {
	add(name.c_str(), value.c_str());
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
bool WhmContext::flush(int status, int format) {
	KWStream* out = getOutputStream();
	if (status > 0) {
		if (format == OUTPUT_FORMAT_XML) {
			*out << "<result status=\"" << status;
			if (statusMsg.size() > 0) {
				*out << " " << statusMsg;
			}
			*out << "\">\n";
		} else if (format == OUTPUT_FORMAT_JSON) {
			*out << "\"status\":\"" << status;
			if (statusMsg.size() > 0) {
				*out << " " << statusMsg;
			}
			*out << "\",\"result\":{\n";
		}
	}
	dv.build(*out, format);
	if (status > 0) {
		if (format == OUTPUT_FORMAT_XML) {
			*out << "</result>\n";
		} else {
			*out << "}";
		}
	}
	return true;
}
