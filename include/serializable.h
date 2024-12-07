#ifndef SERIALIZABLE_H
#define SERIALIZABLE_H
#include "KStream.h"
#include "KStringBuf.h"
#include "KXmlDocument.h"
#include <vector>
#include <map>
namespace kgl {
	using string = KString;
	using wstream = KWStream;
	enum class data_type {
		STR,
		OBJ,
		INT,
		STR_ARRAY,
		OBJ_ARRAY,
		INT_ARRAY
	};
	enum class format {
		xml_attribute,
		xml_text,
		json
	};
	class data_value;
	class serializable {
	public:
		~serializable();
		serializable() {

		}
		serializable(serializable&& a) noexcept {
			data.swap(a.data);
		}
		serializable(const serializable& a) = delete;

		bool add(const string& name, const string& value);
		bool add(const string& name, int64_t v);
		serializable* add(const string& name);

		std::vector<string>* add_string_array(const string& name);
		std::vector<int64_t>* add_int_array(const string& name);
		serializable* add_obj_array(const string& name);
		serializable& operator = (serializable&& a) noexcept;
		serializable& operator = (const serializable& a);

		bool get(const string& name, string& value) const;
		const string& get(const string& name) const;
		const string* getx(const string& name) const;
		const int64_t get_int(const string& name) const;
		void build(wstream& s, format fmt);
	private:
		std::map<string, data_value*> data;
	};

	class data_value {
	public:
		data_value(const string& value) {
			type = data_type::STR;
			this->str = new string(value);
		}
		data_value(int64_t v) :i{ v } {
			type = data_type::INT;
		}
		data_value(data_type type);
		data_value* add_ref() {
			katom_inc((void*)&refs);
			return this;
		}
		void release() {
			if (katom_dec((void*)&refs) == 0) {
				delete this;
			}
		}
		data_type change_to_array() {
			switch (type) {
			case data_type::STR:
			{
				auto v = str;
				strs = new std::vector<string>{ *v };
				delete v;
				type = data_type::STR_ARRAY;
				break;
			}
			case data_type::INT:
			{
				auto v = i;
				ints = new std::vector<int64_t>{ v };
				type = data_type::INT_ARRAY;
				break;
			}
			case data_type::OBJ:
			{
				auto v = obj;
				objs = new std::vector< serializable>();
				objs->emplace_back(std::move(*v));
				delete v;
				type = data_type::OBJ_ARRAY;
				break;
			}
			default:
				break;
			}
			return type;
		}
		void build(const string& name, wstream& s, format fmt);
		union {
			std::vector<string>* strs;       //STR_ARRAY
			std::vector<serializable>* objs; //OBJ_ARRAY
			std::vector<int64_t>* ints;      //INT_ARRAY
			string* str;                     //STR
			serializable* obj;               //OBJ
			int64_t i;                       //INT
		};
		data_type get_type() const {
			return type;
		}
	private:
		data_type type;
		volatile int32_t refs = 1;
		~data_value();
	};

}
#endif
