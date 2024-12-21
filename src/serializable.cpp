#include "serializable.h"
#include "KXmlEvent.h"
namespace kgl {
	static void build_json_string(wstream& s, const string& v) {
		s << "\""_CS;
		for (size_t i = 0; i < v.size(); ++i) {
			switch (v[i]) {
			case '\r':
				s << "\\r";
				break;
			case '\n':
				s << "\\n";
				break;
			case '\t':
				s << "\\t";
				break;
			case '\\':
			case '"':
				s << "\\" << v[i];
				break;
			default:
				s << v[i];
				break;
			}
		}
		s << "\""_CS;
	}
	static void build_xml_text(wstream& s, const string& v) {
		if (v.find('<') != string::npos) {
			s << CDATA_START << v << CDATA_END;
			return;
		}
		s << v;
		return;
	}
	static inline void build_xml_attribute(wstream& s, const string& v) {
		build_json_string(s, v);
	}
	data_value::data_value(data_type type) :type{ type } {
		switch (type) {
		case data_type::INT_ARRAY:
			ints = new std::vector<int64_t>;
			break;
		case data_type::STR_ARRAY:
			strs = new std::vector<string>;
			break;
		case data_type::OBJ_ARRAY:
			objs = new std::vector<serializable>;
			break;
		case data_type::OBJ:
			obj = new serializable;
			break;
		default:
			throw std::bad_exception();
		}
	}
	data_value::~data_value() {
		switch (type) {
		case data_type::INT_ARRAY:
			delete ints;
			break;
		case data_type::STR_ARRAY:
			delete strs;
			break;
		case data_type::OBJ_ARRAY:
			delete objs;
			break;
		case data_type::OBJ:
			delete obj;
			break;
		case data_type::STR:
			delete str;
			break;
		default:
			break;
		}
	}
	void data_value::build(const string& name, wstream& s, format fmt) {
		switch (fmt) {
		case format::xml_text:
			switch (type) {
			case data_type::OBJ:
				s << "<"_CS << name << ">"_CS;
				obj->build(s, fmt);
				s << "</"_CS << name << ">"_CS;
				return;
			case data_type::OBJ_ARRAY:
				for (auto it = objs->begin(); it != objs->end(); ++it) {
					s << "<"_CS << name << ">"_CS;
					(*it).build(s, fmt);
					s << "</"_CS << name << ">"_CS;
				}
				return;
			case data_type::STR:
				s << "<"_CS << name << ">"_CS;
				build_xml_text(s, *str);
				s << "</"_CS << name << ">"_CS;
				return;
			case data_type::STR_ARRAY:
				for (auto it = strs->begin(); it != strs->end(); ++it) {
					s << "<"_CS << name << ">"_CS;
					build_xml_text(s, (*it));
					s << "</"_CS << name << ">"_CS;
				}
				return;
			case data_type::INT_ARRAY:
				for (auto it = ints->begin(); it != ints->end(); ++it) {
					s << "<"_CS << name << ">"_CS;
					s << *it;
					s << "</"_CS << name << ">"_CS;
				}
				return;
			case data_type::INT:
				s << "<"_CS << name << ">"_CS << this->i << "</"_CS << name << ">"_CS;
				return;
			}
			break;
		case format::json:
			switch (type) {
			case data_type::OBJ_ARRAY:
				s << "\""_CS << name << "\":["_CS;
				for (auto it = objs->begin(); it != objs->end(); ++it) {
					if (it != objs->begin()) {
						s << ","_CS;
					}
					s << "{"_CS;
					(*it).build(s, fmt);
					s << "}"_CS;
				}
				s << "]";
				return;
			case data_type::OBJ:
				s << "\""_CS << name << "\":{"_CS;
				obj->build(s, fmt);
				s << "}"_CS;
				return;
			case data_type::STR_ARRAY:
				s << "\""_CS << name << "\":["_CS;
				for (auto it = strs->begin(); it != strs->end(); ++it) {
					if (it != strs->begin()) {
						s << ","_CS;
					}
					build_json_string(s, *it);
				}
				s << "]";
				return;
			case data_type::STR:
				s << "\""_CS << name << "\":"_CS;
				build_json_string(s, *str);
				return;
			case data_type::INT:
				s << "\""_CS << name << "\":"_CS << this->i;
				return;
			case data_type::INT_ARRAY:
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
		default:
			break;
		}
	}
	serializable::~serializable() {
		for (auto it = data.begin(); it != data.end(); ++it) {
			(*it).second->release();
		}
	}
	bool serializable::add(const string& name, const string& value) {
		auto it = data.find(name);
		if (it == data.end()) {
			data_value* dv = new data_value(value);
			data.emplace(name, dv);
			return true;
		}
		auto dv = (*it).second;
		switch (dv->get_type()) {
		case data_type::STR:
			dv->change_to_array();
			//[[fallthrough]];
		case data_type::STR_ARRAY:
			dv->strs->push_back(value);
			return true;
		default:
			break;
		}
		return false;
	}
	bool serializable::add(const string& name, int64_t value) {
		auto it = data.find(name);
		if (it == data.end()) {
			data_value* dv = new data_value(value);
			data.emplace(name, dv);
			return true;
		}
		auto dv = (*it).second;
		switch (dv->get_type()) {
		case data_type::INT:
			dv->change_to_array();
			//[[fallthrough]];
		case data_type::INT_ARRAY:
			dv->ints->push_back(value);
			return true;
		default:
			break;
		}
		return false;
	}
	serializable* serializable::add(const string& name) {
		auto it = data.find(name);
		if (it == data.end()) {
			data_value* dv = new data_value(data_type::OBJ);
			data.emplace(name, dv);
			return dv->obj;
		}
		auto dv = (*it).second;
		switch (dv->get_type()) {
		case data_type::OBJ:
			dv->change_to_array();
			//[[fallthrough]];
		case data_type::OBJ_ARRAY:
			dv->objs->emplace_back();
			return &(*(dv->objs->rend()));
		default:
			break;
		}
		return nullptr;
	}
	std::vector<string>* serializable::add_string_array(const string& name) {
		auto it = data.find(name);
		if (it == data.end()) {
			data_value* dv = new data_value(data_type::STR_ARRAY);
			data.emplace(name, dv);
			return dv->strs;
		}
		return nullptr;
	}
	std::vector<int64_t>* serializable::add_int_array(const string& name) {
		auto it = data.find(name);
		if (it == data.end()) {
			data_value* dv = new data_value(data_type::INT_ARRAY);
			data.emplace(name, dv);
			return dv->ints;
		}
		return nullptr;
	}
	serializable* serializable::add_obj_array(const string& name) {
		auto it = data.find(name);
		if (it == data.end()) {
			data_value* dv = new data_value(data_type::OBJ_ARRAY);
			data.emplace(name, dv);
			dv->objs->emplace_back();
			return &(*(dv->objs->rbegin()));
		}
		if ((*it).second->get_type() != data_type::OBJ_ARRAY) {
			return nullptr;
		}
		data_value* dv = (*it).second;
		dv->objs->emplace_back();
		return &(*(dv->objs->rbegin()));
	}
	void serializable::build(wstream& s, format fmt) {
		for (auto it = data.begin(); it != data.end(); ++it) {
			if (fmt == format::json) {
				if (it != data.begin()) {
					s << ",";
				}
			}
			(*it).second->build((*it).first, s, fmt);
		}
	}
	bool serializable::get(const string& name, string& value) const {
		auto it = data.find(name);
		if (it == data.end() || (*it).second->get_type() != data_type::STR) {
			return false;
		}
		value = *(*it).second->str;
		return true;
	}
	const string& serializable::get(const string& name) const {
		auto it = data.find(name);
		if (it == data.end() || (*it).second->get_type() != data_type::STR) {
			return string::empty_string;
		}
		return *(*it).second->str;
	}
	const string* serializable::getx(const string& name) const {
		auto it = data.find(name);
		if (it == data.end() || (*it).second->get_type() != data_type::STR) {
			return nullptr;
		}
		return (*it).second->str;
	}
	const int64_t serializable::get_int(const string& name) const {
		auto it = data.find(name);
		if (it == data.end() || (*it).second->get_type() != data_type::INT) {
			return 0;
		}
		return (*it).second->i;
	}
	serializable& serializable::operator = (serializable&& a) noexcept {
		this->data.swap(a.data);
		return *this;
	}
	serializable& serializable::operator = (const serializable& a) {
		this->data = a.data;
		for (auto it = data.begin(); it != data.end(); ++it) {
			(*it).second->add_ref();
		}
		return *this;
	}
}