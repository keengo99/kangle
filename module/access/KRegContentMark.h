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
#ifndef KCONTENTMARK_H_
#define KCONTENTMARK_H_
#include <string>
#include <map>
#include "KMark.h"
#include "KReg.h"
#include "KContentMark.h"
#include "log.h"

#define WORK_SIZE		1024
#define BACK_SIZE		128
static std::map<std::string, std::string> replaceContentMap;
class KRegContentMark: public KContentMark {
public:
	KRegContentMark() {
		partial = true;
		//		logFlag = 1;
		if (replaceContentMap.size() == 0) {
			replaceContentMap["\""] = "\\\"";
		}
		useCharset = true;
		localReg = NULL;
		utf8Reg = NULL;
	}
	virtual ~KRegContentMark() {
		clear();
	}
	void clear() {
		if(localReg){
			delete localReg;
			localReg = NULL;
		}
		if(utf8Reg){
			delete utf8Reg;
			utf8Reg = NULL;
		}
	}
	std::string getDisplay() {
		std::stringstream s;
		s << "<a href=# title='" << KXml::encode(reg) << "'>charset:"
				<< charset << ",partial:"
				<< (partial ? "yes" : "no") << "</a>";
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html){
		charset = attribute["charset"];
		reg = attribute["content"];
		setCharsetReg();
	}
	std::string getHtml(KModel *model) {
		KRegContentMark *mark = (KRegContentMark *) model;
		std::stringstream s;
		s << "content:<input type=text size=80 name='content' value='";
		if (mark) {
			s << KXml::encode(mark->reg);
		}
		s << "'><br>";
		s << "charset:<input type=text size='6' name='charset' value='";
		if (mark) {
			s << KXml::encode(mark->charset);
		}
		s << "'>";
		return s.str();
	}
	KMark *newInstance() {
		return new KRegContentMark();
	}
	const char *getName() {
		return "content";
	}
public:
	bool startElement(KXmlContext *context,
			std::map<std::string, std::string> &attribute) {
		charset = attribute["charset"];
		return true;
	}
	bool startCharacter(KXmlContext *context, char *character, int len) {
		reg = KXml::decode(character);
		setCharsetReg();
		return true;
	}
	void buildXML(std::stringstream &s) {
		s << " charset='" << charset << "'>" ;
		s << CDATA_START << KXml::encode(reg) << CDATA_END;
	}
	int check(const char *data, int data_len, const char *charset,
			KFilterKey **keys) {
		int ovector[3];
		//		int worksize[WORK_SIZE];
		memset(ovector, 0, sizeof(ovector));
		KReg *charsetReg = getReg(charset);
		int ret = -1;
		int regFlag = 0;
		if (charsetReg == NULL) {
			return false;
		}
		if (partial) {
			if (charsetReg->isPartialModel()) {
				regFlag |= PCRE_PARTIAL;
			} else {
				partial = false;
			}
		}
		for (int i = 0; i < 2; i++) {
			ret = charsetReg->match(data, data_len, regFlag, ovector, 3);
			if (ret == PCRE_ERROR_PARTIAL) {
				return FILTER_PARTIALMATCH;
			}
			if (ret == PCRE_ERROR_BADPARTIAL) {
				assert(KBIT_TEST(regFlag,PCRE_PARTIAL));
				KBIT_CLR(regFlag, PCRE_PARTIAL);
				partial = false;
				continue;
			}
			if (ret >= 0 || ret == PCRE_ERROR_MATCHLIMIT) {
				int key_len = ovector[1] - ovector[0];
				if (key_len <= 0) {
					klog(KLOG_WARNING, "key_len is NULL\n");
					return FILTER_MATCH;
				}
				assert(*keys==NULL);
				*keys = new KFilterKey;
				(*keys)->key = (char *) xmalloc(key_len+1);
				(*keys)->len = key_len;
				kgl_memcpy((*keys)->key, data + ovector[0], ovector[1] - ovector[0]);
				(*keys)->key[key_len] = 0;
				return FILTER_MATCH;
			}
			break;
		}
		return FILTER_NOMATCH;
	}
private:

	void setCharsetReg() {
		clear();
		utf8Reg = new KReg;
		utf8Reg->setModel(reg.c_str(), 0, 0);
		if(charset.size()==0 || strcasecmp(charset.c_str(),"utf-8")==0){
			return;
		}	
		char *str = NULL;
		str = utf82charset(reg.c_str(), reg.size(), charset.c_str());
		if (str == NULL) {
			return;
		}
		localReg = new KReg;
		if (!localReg->setModel(str, 0, 0)) {
			//	printf("cann't compile regstr=[%s]\n", str);
			goto err;
		}
		if (str) {
			free(str);
		}
		return;
		err: if (localReg) {
			delete localReg;
			localReg = NULL;
		}
		if (str) {
			free(str);
		}
		return;
	}
	KReg *getReg(const char *charset) {
		if(localReg==NULL){
			return utf8Reg;
		}
		if(charset && strcasecmp(charset,"utf-8")==0){
			return utf8Reg;
		}
		return localReg;
	}
private:
	bool partial;
	std::string charset;
	bool useCharset;
	std::string reg;
	KReg *localReg;
	KReg *utf8Reg;
};

#endif /*KCONTENTMARK_H_*/
