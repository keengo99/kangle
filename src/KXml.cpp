/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 *
 * kangle web server              http://www.kangleweb.net/
 * ---------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *  See COPYING file for detail.
 *
 *  Author: KangHongjiu <keengo99@gmail.com>
 */
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <sstream>
#include <iostream>
#include <assert.h>
#include "KXml.h"
#include "klog.h"
#include "kforwin32.h"
//#include "kfiber.h"
//#include "KFileName.h"
#include "kmalloc.h"
using namespace std;
kxml_fopen KXml::fopen = NULL;
kxml_fclose KXml::fclose = NULL;
kxml_fsize KXml::fsize = NULL;
kxml_fread KXml::fread = NULL;

std::string replace(const char* str, map<string, string>& replaceMap,
	const char* start, const char* end) {
	stringstream s;
	if (str == NULL)
		return "";
	int startLen = 0;
	int endLen = 0;
	if (start) {
		startLen = (int)strlen(start);
	}
	if (end) {
		endLen = (int)strlen(end);
	}
	while (*str) {
		if (start) {
			if (strncmp(str, start, startLen) != 0) {
				s << *str;
				str++;
				continue;
			}
			str += startLen;
		}
		map<string, string>::iterator it;
		bool find = false;
		for (it = replaceMap.begin(); it != replaceMap.end(); it++) {
			if (strncmp(str, (*it).first.c_str(), (*it).first.size()) == 0) {
				if (end) {
					if (strncmp(str + (*it).first.size(), end, endLen) != 0) {
						continue;
					}
				}
				s << (*it).second;
				str += ((*it).first.size() + endLen);
				find = true;
				break;
			}
		}
		if (!find) {
			if (start) {
				s << start;
			}
			s << *str;
			str++;
		}
	}
	return s.str();
}
/*
 * return the next string
 *
 */
char* getString(char* str, char** nextstr, const char* ended_chars,
	bool end_no_quota_value, bool skip_slash) {
	while (*str && isspace((unsigned char)*str)) {
		str++;
	}
	bool slash = false;
	char endChar = *str;
	char* start;
	if (endChar != '\'' && endChar != '"') {
		//没有引号引起来的字符串
		start = str;
		while (*str && !isspace((unsigned char)*str)) {
			if (ended_chars && strchr(ended_chars, *str) != NULL) {
				break;
			}
			str++;
		}
		if (end_no_quota_value) {
			if (*str != '\0') {
				*str = '\0';
				str++;
			}
		}
		*nextstr = str;
		return start;
	}
	str++;
	start = str;
	char* hot = str;
	while (*str) {
		if (slash) {
			if (*str == '\\' || *str == '\'' || *str == '"') {
				*hot = *str;
				hot++;
			} else {
				*hot = '\\';
				hot++;
				*hot = *str;
				hot++;
			}
			slash = false;
		} else {
			if (!skip_slash && *str == '\\') {
				slash = true;
				str++;
				continue;
			}
			slash = false;
			if (*str == endChar) {
				*str = '\0';
				*hot = '\0';
				*nextstr = str + 1;
				return start;
			}
			*hot = *str;
			hot++;
		}
		str++;
	}
	return NULL;
}
KXml::KXml() {
	file = NULL;
	origBuf = NULL;
	line = 0;
	hot = NULL;
	data = NULL;
}
KXml::~KXml()
{
	clear();
}
void KXml::clear()
{
	std::list<KXmlContext*>::iterator it;
	for (it = contexts.begin(); it != contexts.end(); it++) {
		delete (*it);
	}
	contexts.clear();
}
void KXml::setEvent(KXmlEvent* event) {
	//this->event = event;
	events.clear();
	events.push_back(event);
}
void KXml::addEvent(KXmlEvent* event) {
	events.push_back(event);
}
char* KXml::htmlEncode(const char* str, int& len, char* buf)
{
	if (buf == NULL) {
		buf = (char*)malloc(5 * len + 1);
	}
	char* dst = buf;
	const char* src = str;
	while (*src) {
		switch (*src) {
		case '\'':
			*dst++ = '&';
			*dst++ = '#';
			*dst++ = '3';
			*dst++ = '9';
			*dst++ = ';';
			break;
		case '"':
			*dst++ = '&';
			*dst++ = '#';
			*dst++ = '3';
			*dst++ = '4';
			*dst++ = ';';
			break;
		case '&':
			*dst++ = '&';
			*dst++ = 'a';
			*dst++ = 'm';
			*dst++ = 'p';
			*dst++ = ';';
			break;
		case '>':
			*dst++ = '&';
			*dst++ = 'g';
			*dst++ = 't';
			*dst++ = ';';
			break;
		case '<':
			*dst++ = '&';
			*dst++ = 'l';
			*dst++ = 't';
			*dst++ = ';';
			break;
		default:
			*dst++ = *src;
		}
		src++;
	}
	*dst = '\0';
	len = dst - buf;
	return buf;
}
char* KXml::htmlDecode(char* str, int& len)
{
	char* dst = str;
	char* src = str;
	while (*src) {
		if ((*src) == '&' && *(src + 1)) {
			if (strncasecmp(src + 1, "lt;", 3) == 0) {
				*dst++ = '<';
				src += 4;
				continue;
			}
			if (strncasecmp(src + 1, "gt;", 3) == 0) {
				*dst++ = '>';
				src += 4;
				continue;
			}
			if (strncasecmp(src + 1, "quot;", 5) == 0) {
				*dst++ = '"';
				src += 6;
				continue;
			}
			if (strncasecmp(src + 1, "apos;", 5) == 0) {
				*dst++ = '\'';
				src += 6;
				continue;
			}
			if (strncasecmp(src + 1, "amp;", 4) == 0) {
				*dst++ = '&';
				src += 5;
				continue;
			}
			if (*(src + 1) == '#') {
				char* e = strchr(src + 1, ';');
				if (e) {
					*dst++ = atoi(src + 2);
					src = e + 1;
					continue;
				}
			}
		}
		*dst++ = *src++;
	}
	*dst = '\0';
	len = dst - str;
	return str;
}
std::string KXml::param(const char* str)
{
	return encode(str);
}
std::string KXml::encode(std::string str) {
	map<string, string> transfer;
	transfer["&"] = "&amp;";
	transfer["'"] = "&#39;";
	transfer["\""] = "&#34;";
	transfer[">"] = "&gt;";
	transfer["<"] = "&lt;";
	return replace(str.c_str(), transfer);
}
std::string KXml::decode(std::string str) {
	map<string, string> transfer;
	transfer["&#39;"] = "'";
	transfer["&#34;"] = "\"";
	transfer["&amp;"] = "&";
	transfer["&gt;"] = ">";
	transfer["&lt;"] = "<";
	return replace(str.c_str(), transfer);
}
long KXml::getFileSize(FILE* fp) {
	long begin, end, current;
	assert(fp);
	if (fp == NULL)
		return -1;
	current = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	begin = ftell(fp);
	fseek(fp, 0, SEEK_END);
	end = ftell(fp);
	fseek(fp, current, SEEK_SET);
	return end - begin;
}
int KXml::getLine() {
	char* buf = origBuf;
	//int l = line;
	int len = hot - buf;
	//printf("len=%d\n",len);
	while (len-- > 0) {
		if (*buf == '\n') {
			line++;
		}
		buf++;
	}
	origBuf = hot;
	//line = l;
	return line;
}
bool KXml::startParse(char* buf) {
	origBuf = buf;
	line = 1;
	if (events.size() == 0) {
		fprintf(stderr, "not set event\n");
		return false;
	}
	bool result = false;
	hot = strchr(buf, '<');
	if (hot == NULL) {
		fprintf(stderr, "file is not a xml format\n");
		return false;
	}
	if (hot[1] == '?') {
		//first
		hot[1] = '\0';
		buf = hot + 2;
		hot = strchr(buf, '?');
		if (hot == NULL || hot[1] != '>') {
			fprintf(stderr, "file is not a xml format\n");
			return false;
		}
		*hot = 0;
		std::map<string, string> attribute;
		buildAttribute(buf, attribute);
		encoding = attribute["encoding"];
		buf = hot + 2;
	}
	try {
		startXml(encoding);
		result = internelParseString(buf);
	} catch (KXmlException& e) {
		endXml(result);
		throw e;
	}
	endXml(result);
	return result;

}
bool KXml::parseString(const char* buf) {

	char* str = strdup(buf);
	bool result = false;
	try {
		result = startParse(str);
	} catch (KXmlException& e) {
		free(str);
		throw e;
	}
	free(str);
	return result;
}

bool KXml::internelParseString(char* buf) {

	//std::map<std::string, std::string> attibute;
	std::list<KXmlEvent*>::iterator it;
	bool single = false;
	KXmlContext* curContext = NULL;
	std::string name;
	int state = 0;
	hot = buf;
	char* p;
	KXmlException e;
	clear();
	//	hot=buf;
	while (*hot) {
		bool cdata = false;
		while (*hot && isspace((unsigned char)*hot))
			hot++;
		if (*hot == '<') {
			//	printf("buf=[%s]",buf);
			if (strncmp(hot, "<![CDATA[", 9) == 0) {
				cdata = true;
				hot += 9;
				state = START_CHAR;
			} else if (strncmp(hot, "<!--", 4) == 0) {
				//注释
				hot = strstr(hot + 4, "-->");
				hot += 3;
				continue;
			} else {
				hot++;
				while (*hot && isspace((unsigned char)*hot))
					hot++;
				if (*hot == '/') {
					state = END_ELEMENT;
					hot++;
				} else {
					state = START_ELEMENT;
				}
			}
		} else {
			if (state == START_CHAR) {
				//				printf("xml end\n");
				break;
			}
			state = START_CHAR;
		}
		if (state == START_ELEMENT) {
			single = false;
			while (*hot && isspace((unsigned char)*hot))
				hot++;
			p = strchr(hot, '>');
			if (!p) {
				e << "Cann't get start element end,tag=" << name << "\n";
				throw e;
				//				printf("cann't get start element end,qName=%s,buf=%s\n",
				//						name.c_str(), buf);
				//				return false;
			}
			*p = 0;
			char* end = p + 1;
			int end_pos = p - hot;
			for (int i = end_pos - 1; i >= 0; i--) {
				if (isspace((unsigned char)hot[i]))
					continue;
				if (hot[i] == '/') {
					single = true;
					hot[i] = '\0';
					break;
				} else {
					break;
				}
			}
			p = hot;
			while (*p && !isspace((unsigned char)*p))
				p++;

			if (*p != 0) {
				*p = 0;
				name = hot;
				curContext = newContext(name);
				hot = p + 1;
				buildAttribute(hot, curContext->attribute);
			} else {
				name = hot;
				curContext = newContext(name);
			}
			try {
				for (it = events.begin(); it != events.end(); it++) {
					(*it)->startElement(curContext, curContext->attribute);
					(*it)->startElement(curContext->path, curContext->qName,
						curContext->attribute);
				}
				hot = end;
				if (single) {
					for (it = events.begin(); it != events.end(); it++) {
						(*it)->endElement(curContext);
						(*it)->endElement(curContext->path, curContext->qName);
					}
					delete curContext;
					curContext = NULL;
					if (contexts.size() == 0) {
						//printf("xml end\n");
						break;
					}
					continue;
				}
				contexts.push_back(curContext);
			} catch (KXmlException& e2) {
				delete curContext;
				throw e2;
			}
		} else if (state == START_CHAR) {

			int char_len;
			if (cdata) {
				p = strstr(hot, "]]>");
				if (p == NULL) {
					e << "Cann't read cdata end\n";
					throw e;
					//					printf("cann't read cdata end\n");
					//					return false;
				}
				char_len = p - hot;
				p += 3;
			} else {
				p = strchr(hot, '<');
				if (p == NULL) {
					return true;
					//e << "cann't get charater end\n";
					//throw e;
					//					cout << "不能得到charater end" << endl;
					//					return false;
				}
				char_len = p - hot;
			}
			if (curContext) {
				//assert(char_len>0);
				char* charBuf = (char*)malloc(char_len + 1);
				kgl_memcpy(charBuf, hot, char_len);
				charBuf[char_len] = '\0';
				if (!cdata) {
					htmlDecode(charBuf, char_len);
				}
				for (it = events.begin(); it != events.end(); it++) {
					(*it)->startCharacter(curContext, charBuf, char_len);
					(*it)->startCharacter(curContext->path, curContext->qName,
						charBuf, char_len);
				}
				free(charBuf);
			}
			hot = p;
		} else if (state == END_ELEMENT) {
			while (*hot && isspace((unsigned char)*hot))
				hot++;
			p = strchr(hot, '>');
			if (p == NULL) {
				e << "cann't get charater end\n";
				throw e;
				//				cout << "不能得到charater end" << endl;
				//				return false;
			}
			*p = 0;
			char* end = p + 1;
			p = hot;
			while (*p && !isspace((unsigned char)*p))
				p++;
			*p = 0;
			if (contexts.size() <= 0) {
				e << "contexts not enoungh\n";
				throw e;
				//				printf("contexts 不够\n");
				//				return false;
			}
			name = hot;
			list<KXmlContext*>::iterator it2 = contexts.end();
			it2--;
			if (name != (*it2)->qName) {
				e << "end tag [" << name << "] is not equal start tag ["
					<< (*it2)->qName << "]\n";
				throw e;
				//				printf("end Element =[%s] 和 start Element[%s] 不对\n",
				//						name.c_str(), (*it)->qName.c_str());
				//				return false;
			}
			curContext = (*it2);
			contexts.pop_back();

			//getContext(context);
			for (it = events.begin(); it != events.end(); it++) {
				(*it)->endElement(curContext);
				(*it)->endElement(curContext->path, curContext->qName);
			}
			delete curContext;
			curContext = NULL;
			hot = end;
			if (contexts.size() == 0) {
				//	printf("xml end\n");
				//break;
			}

		}
	}
	/*
	 if (contexts.size() > 0) {
	 e << "xml not complete\n";
	 throw e;
	 }
	 if (*hot) {
	 goto retry;
	 }
	 */
	return true;
}
bool KXml::parseFile(std::string file) {
	KXmlException e;
	stringstream s;
	bool result;
	this->file = file.c_str();
	char* content = getContent(file);
	if (content == NULL || *content == '\0') {
		s << "cann't read file:[" << file << "]\n";
		e.setMsg(s.str());
		if (content) {
			free(content);
		}
		throw e;
	}
	try {
		result = startParse(content);
	} catch (KXmlException& e2) {
		fprintf(stderr, "Error happen in %s:%d\n", file.c_str(), getLine());
		free(content);
		throw e2;
	}
	free(content);
	return result;
}
KXmlContext* KXml::newContext(std::string qName) {
	KXmlContext* context = new KXmlContext(this);
	//context->file = file;
	//context->line = getLine();
	context->qName = qName;
	stringstream s;
	bool begin = true;
	for (list<KXmlContext*>::iterator it = contexts.begin(); it
		!= contexts.end(); it++) {
		if (!begin) {
			s << "/";
		}
		begin = false;
		s << (*it)->qName;
		context->parent = (*it);
		context->level++;
	}
	s.str().swap(context->path);
	return context;
}
char* KXml::getContent(const std::string& file) {
	void *fp = KXml::fopen(file.c_str(), fileRead, 0);
	if (fp == NULL) {
		return NULL;
	}
	char* buf = NULL;
	INT64 fileSize = KXml::fsize(fp);
	if (fileSize > 1048576) {
		goto clean;
	}
	buf = (char*)malloc((int)fileSize + 1);
	if (buf == NULL) {
		goto clean;
	}
	if (KXml::fread(fp, buf, (int)fileSize) != (int)fileSize) {
		free(buf);
		buf = NULL;
		goto clean;
	}
	buf[fileSize] = '\0';
clean:
	KXml::fclose(fp);
	return buf;

}
void buildAttribute(char* buf, std::map<std::string, std::string>& attribute) {
	attribute.clear();
	//	printf("buf=[%s]\n",buf);
	while (*buf) {
		while (*buf && isspace((unsigned char)*buf))
			buf++;
		char* p = strchr(buf, '=');
		if (p == NULL)
			return;
		int name_len = p - buf;
		for (int i = name_len - 1; i >= 0; i--) {
			if (isspace((unsigned char)buf[i]))
				buf[i] = 0;
			else
				break;
		}
		*p = 0;
		p++;
		std::string name = buf;
		buf = p;
		buf = getString(buf, &p, NULL, false, true);
		if (buf == NULL) {
			return;
		}
		int len;
		std::string value = KXml::htmlDecode(buf, len);
		buf = p;
		//cout << "name=" << name << ",value=" << value << endl;
		attribute.insert(pair<std::string, std::string>(name, value));
	}
}



void buildAttribute(char* buf, std::map<char*, char*, lessp_icase>& attibute) {
	while (*buf) {
		while (*buf && isspace((unsigned char)*buf))
			buf++;
		char* p = strchr(buf, '=');
		if (p == NULL) {
			attibute.insert(std::pair<char*, char*>(buf, NULL));
			return;
		}
		int name_len = p - buf;
		for (int i = name_len - 1; i >= 0; i--) {
			if (isspace((unsigned char)buf[i]))
				buf[i] = 0;
			else
				break;
		}
		*p = 0;
		p++;
		char* name = buf;
		buf = p;
		buf = getString(buf, &p, NULL, true, true);
		if (buf == NULL) {
			return;
		}
		char* value = buf;
		buf = p;
		attibute.insert(std::pair<char*, char*>(name, value));
	}
}
