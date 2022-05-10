/*
 * KHtAccess.cpp
 *
 *  Created on: 2010-9-26
 *      Author: keengo
 */
#include <vector>
#include <string.h>
#include <string>
#include "KReg.h"
#include "KHtAccess.h"
#include "KLineFile.h"
#include "log.h"
#include "KXml.h"
#include "KHtRewriteModule.h"
#include "directory.h"
#include "kmalloc.h"
#include "KApacheVirtualHost.h"
#include "kfile.h"

struct KApacheConfigFileInclude
{
	char* dir;
	KReg file;
	KApacheConfig* configer;
};
int apache_config_include_handle(const char* file, void* param)
{
	KApacheConfigFileInclude* include = (KApacheConfigFileInclude*)param;
	if (include->file.match(file, (int)strlen(file), 0)) {
		std::stringstream s;
		s << include->dir << PATH_SPLIT_CHAR << file;
		include->configer->load(s.str().c_str());
	}
	return 0;
}
using namespace std;
/*
 * 最大htaccess文件大小,1M
 */
static const int max_htaccess_file_size = 1024 * 1024;
void split(char* buf, std::vector<char*>& item) {
	for (;;) {
		char* hot = buf;
		while (*hot && !isspace((unsigned char)*hot)) {
			hot++;
		}
		if (*hot == '\0') {
			break;
		}
		*hot = '\0';
		hot++;
		while (*hot && isspace((unsigned char)*hot)) {
			hot++;
		}
		char startChar = *hot;
		if (*hot == '\'' || *hot == '"') {
			hot++;
			buf = hot;
			item.push_back(buf);
			char* dst = hot;
			bool slash = false;
			for (;;) {
				if (*hot == '\0') {
					return;
				}
				if (!slash) {
					if (*hot == startChar) {
						*dst = '\0';
						buf = hot + 1;
						break;
					}
					if (*hot == '\\') {
						slash = true;
						hot++;
						continue;
					}
					*dst++ = *hot++;
				} else {
					if (*hot != startChar) {
						*dst++ = '\\';
					}
					*dst++ = *hot++;
					slash = false;
				}
			}
		} else {
			buf = hot;
			item.push_back(buf);
			if (*buf == '#' || *buf == '\0') {
				break;
			}
		}

	}
}
KApacheConfig::KApacheConfig(bool isHtaccess) {
	this->isHtaccess = isHtaccess;
	tableid = 1;
	includeLevel = 0;
	if (!isHtaccess) {
		modules.push_back(new KApacheVirtualHost);
	}
	modules.push_back(new KHtRewriteModule);
	prefix = "";
}
KApacheConfig::~KApacheConfig() {
	std::list<KHtModule*>::iterator it;
	for (it = modules.begin(); it != modules.end(); it++) {
		delete (*it);
	}
}
std::string KApacheConfig::getFullPath(std::string file)
{
	if (isAbsolutePath(file.c_str())) {
		return file;
	}
	return serverRoot + PATH_SPLIT_CHAR + file;
}
bool KApacheConfig::process(const char* cmd, std::vector<char*>& item)
{
	if (isHtaccess) {
		return false;
	}
	if (strcasecmp(cmd, "ServerRoot") == 0) {
		serverRoot = item[0];
		return true;
	}
	if (strcasecmp(cmd, "Listen") == 0) {
		char* v = item[0];
		KListenHost listen;
		char* p = strchr(v, ':');
		const char* ip = "*";
		int port = 0;
		if (p) {
			port = atoi(p + 1);
			*p = '\0';
			if (*v == '[') {
				v++;
				p = strchr(v, ']');
				if (*p) {
					*p = '\0';
				}
			}
			ip = v;
		} else {
			port = atoi(v);
		}
		listen.ip = ip;
		listen.port = port;
		listen.model = 0;
		if (item.size() > 1 && strcasecmp(item[1], "https") == 0) {
			listen.model = WORK_MODEL_SSL;
		}
		listens.push_back(listen);
		return true;
	}
	if (strcasecmp(cmd, "Include") == 0) {
		if (includeLevel > 128) {
			klog(KLOG_ERR, "Include level is too large,ignore Include\n");
			return true;
		}
		char* path = NULL;
		if (isAbsolutePath(item[0])) {
			path = strdup(item[0]);
		} else {
			std::stringstream p;
			p << serverRoot << PATH_SPLIT_CHAR << item[0];
			path = strdup(p.str().c_str());
		}
		if (strchr(path, '*') != NULL) {
			//目录
			KApacheConfigFileInclude include;
			char* e = path + strlen(path) - 1;
			bool efinded = false;
			while (e > path) {
				if (*e == '/'
#ifdef _WIN32
					|| *e == '\\'
#endif
					) {
					efinded = true;
					break;
				}
				e--;
			}
			if (efinded) {
				int nc = 0;
#ifdef _WIN32
				nc = PCRE_CASELESS;
#endif
				include.file.setModel(e + 1, nc);
				*e = '\0';
				include.dir = path;
				include.configer = this;
				list_dir(path, apache_config_include_handle, (void*)&include);
			}
		} else {
			//include one file
			load(path);
		}
		free(path);
	}
	return false;
}
bool KApacheConfig::load(const char* file)
{
	KFileName fileInfo;
	if (!fileInfo.setName(file)) {
		return false;
	}
	includeLevel++;
	bool result = load(&fileInfo);
	includeLevel--;
	return result;
}
bool KApacheConfig::parse(char* buf)
{
	list<KHtModule*>::iterator it;
	KLineFile lf;
	lf.give(buf);
	for (;;) {
		char* line = lf.readLine();
		if (line == NULL) {
			break;
		}
		while (*line && isspace((unsigned char)*line)) {
			line++;
		}
		if (*line == '#' || *line == '\0') {
			//it is a comment line or is a space line
			continue;
		}
		if (*line == '<') {
			//it is a block
			line++;
			while (*line && isspace((unsigned char)*line)) {
				line++;
			}
			char* end = strchr(line, '>');
			if (end == NULL) {
				//klog(KLOG_ERR,
				//		"parse file[%s] error want have end char >\n",
				//		file->getName());
				continue;
			}
			*end = '\0';
			char* p = line;
			while (*p && !isspace((unsigned char)*p)) {
				p++;
			}
			if (*p != '\0') {
				*p = '\0';
			}
			if (*line == '/') {
				for (it = modules.begin(); it != modules.end(); it++) {
					(*it)->endContext(this, line + 1);
				}
				context = "";
			} else {
				attribute.clear();
				buildAttribute(p + 1, attribute);
				context = line;
				for (it = modules.begin(); it != modules.end(); it++) {
					(*it)->startContext(this, line, attribute);
				}
			}
			continue;
		}
		std::vector<char*> items;
		split(line, items);
		if (!process(line, items)) {
			for (it = modules.begin(); it != modules.end(); it++) {
				if ((*it)->process(this, line, items)) {
					break;
				}
			}
		}
	}
	return true;
}
bool KApacheConfig::load(KFileName* file)
{
	if (file->get_file_size() > max_htaccess_file_size) {
		klog(KLOG_ERR, "the file [%s] size=[%d] is too big\n", file->getName(),
			file->get_file_size());
		return false;
	}
	KFile fp;
	if (!fp.open(file->getName(), fileRead)) {
		klog(KLOG_ERR, "cann't open file[%s] for read\n", file->getName());
		return false;
	}
	char* buf = (char*)xmalloc((int)file->get_file_size() + 1);
	if (buf == NULL) {
		klog(KLOG_ERR, "no memory to alloc %s:%d", __FILE__, __LINE__);
		return false;
	}
	int len = fp.read(buf, (int)file->get_file_size());
	buf[len] = '\0';
	return parse(buf);
}
bool KApacheConfig::load(KFileName* file, std::stringstream& s) {
	if (!load(file)) {
		return false;
	}
	getXml(s);
#ifndef NDEBUG
#if 0
	KFileName savefile;
	stringstream f1;
	f1 << file->getName() << ".xml";
	FILE* fp2 = fopen(f1.str().c_str(), "wb");
	if (fp2 != NULL) {
		fprintf(fp2, "%s", s.str().c_str());
		fclose(fp2);
	}
#endif
#endif
	return true;
}
void KApacheConfig::getXml(std::stringstream& s)
{
	s << "<!--converted by kangle " << time(NULL) << " -->\n";
	s << "<config version='" << VERSION << "'>\n";
	std::list<KListenHost>::iterator it2;
	for (it2 = listens.begin(); it2 != listens.end(); it2++) {
		s << "\t<listen ip='" << (*it2).ip << "' ";
		s << "port='" << (*it2).port << "' type='"
			<< getWorkModelName((*it2).model) << "' ";
		s << "/>\n";
	}
	list<KHtModule*>::iterator it;
	for (it = modules.begin(); it != modules.end(); it++) {
		(*it)->getXml(s);
	}
	s << "</config>";
	return;
}
