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
WhmDataValue::WhmDataValue() {
	type = WhmDataType::OBJ;
	encode = false;
	this->objs = new std::vector<WhmDataAttribute*>();
}
WhmDataValue::~WhmDataValue() {
	switch (type) {
	case WhmDataType::OBJ:
		for (auto it = objs->begin(); it != objs->end(); ++it) {
			delete (*it);
		}
		delete this->objs;
		return;
	case WhmDataType::STR:
		delete this->strs;
		break;
	case WhmDataType::INT:
		delete this->ints;
		break;
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
		switch (type) {
		case WhmDataType::OBJ:
			for (auto it = objs->begin(); it != objs->end(); ++it) {
				s << "<"_CS << name << ">"_CS;
				(*it)->build(s, format);
				s << "</"_CS << name << ">"_CS;
			}
			return;
		case WhmDataType::STR:
			for (auto it = strs->begin(); it != strs->end(); ++it) {
				s << "<"_CS << name << ">"_CS;
				if (encode) {
					s << CDATA_START << (*it) << CDATA_END;
				} else {
					s << *it;
				}
				s << "</"_CS << name << ">"_CS;
			}
			return;
		case WhmDataType::INT:
			for (auto it = ints->begin(); it != ints->end(); ++it) {
				s << "<"_CS << name << ">"_CS;
				if (encode) {
					s << CDATA_START << (*it) << CDATA_END;
				} else {
					s << *it;
				}
				s << "</"_CS << name << ">"_CS;
			}
			return;
		}
	} else if (format == OUTPUT_FORMAT_JSON) {
		switch (type) {
		case WhmDataType::OBJ:
			if (objs->size() == 1) {
				s << "\""_CS << name << "\":{"_CS;
				(*(objs->begin()))->build(s, format);
				s << "}"_CS;
				return;
			}
			s << "\""_CS << name << "\":["_CS;
			for (auto it = objs->begin(); it != objs->end(); ++it) {
				if (it != objs->begin()) {
					s << ","_CS;
				}
				s << "{"_CS;
				(*it)->build(s, format);
				s << "}"_CS;
			}
			s << "]";
			return;
		case WhmDataType::STR:
			if (strs->size() == 1) {
				s << "\""_CS << name << "\":"_CS;
				build_json_string(s, *strs->begin());
				return;
			}
			s << "\""_CS << name << "\":["_CS;
			for (auto it = strs->begin(); it != strs->end(); ++it) {
				if (it != strs->begin()) {
					s << ","_CS;
				}
				build_json_string(s, *it);
			}
			s << "]";
			return;
		case WhmDataType::INT:
			if (ints->size() == 1) {
				s << "\""_CS << name << "\":"_CS << *ints->begin();
				return;
			}
			s << "\""_CS << name << "\":["_CS;
			for (auto it = ints->begin(); it != ints->end(); ++it) {
				if (it != ints->begin()) {
					s << ","_CS;
				}
				s << (*it);
			}
			s << "]";
			return;
		}
	}
}

bool WhmContext::add(const char* name, const char* value, bool encode) {
	return data()->add(name, value, encode);
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
