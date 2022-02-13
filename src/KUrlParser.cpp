#include "KUrlParser.h"

static int my_htoi(char *s) {
	int value;
	int c;

	c = ((unsigned char *) s)[0];
	if (isupper(c))
		c = tolower(c);
	value = (c >= '0' && c <= '9' ? c - '0' : c - 'a' + 10) * 16;

	c = ((unsigned char *) s)[1];
	if (isupper(c))
		c = tolower(c);
	value += c >= '0' && c <= '9' ? c - '0' : c - 'a' + 10;

	return (value);
}
int url_decode(char *str, int len, KUrl *url,bool space2plus) {
	char *dest = str;
	char *data = str;
	bool mem_availble = false;
	if (len == 0) {
		len = (int)strlen(str);
	}
	while (len--) {
		if (space2plus && *data == '+') {
			*dest = ' ';
			if (url) {
				SET(url->flags,KGL_URL_ENCODE);
			}
		} else if (
			*data == '%' &&
			len >= 2 &&
			isxdigit((unsigned char) *(data + 1)) &&
			isxdigit((unsigned char) *(data + 2))) {
			mem_availble = true;
			*dest = (char) my_htoi(data + 1);
			data += 2;
			len -= 2;
			if (url) {
				SET(url->flags,KGL_URL_ENCODE);
			}
		} else {
			*dest = *data;
		}
		data++;
		dest++;
	}
	if (mem_availble) {
		*dest = '\0';
	}
	return (int)(dest - str);
}
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
