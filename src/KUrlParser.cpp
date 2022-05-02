#include "KUrlParser.h"

bool KUrlParser::parse(const char *param)
{
	if (param == NULL || buf) {
		return false;
	}
	buf = strdup(param);
	char *name = buf;
	char *value;
	for (;;) {
		char *next_item = strchr(name,'&');
		if (next_item) {
			*next_item = '\0';
			next_item++;
		}
		value = strchr(name,'=');
		url_decode(name);
		if (value) {
			*value = '\0';
			value++;
			url_decode(value);
			add(name,value);
		} else {
			add(name,"");
		}
		if (next_item==NULL) {
			break;
		}
		name = next_item;
	}
	return true;
}
