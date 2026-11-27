#ifndef KFILEMODIFIED_H_INCLUDED
#define KFILEMODIFIED_H_INCLUDED
#include "kfile.h"
#include "KStringBuf.h"
class KFileModified
{
public:
	KFileModified() :KFileModified(0, 0) {

	}
	KFileModified(time_t mt_time, INT64 size) {
		this->mt_time = mt_time;
		this->size = size;
	}
	KFileModified(const char* filename) :mt_time{ 0 }, size{ 0 } {
		kassert(filename != nullptr);
		struct _stati64 sbuf;
#ifdef _WIN32
		int ret = lstat(filename, &sbuf);
#else
		//跟随符号链接,证书/配置文件常以符号链接部署(如letsencrypt)
		int ret = stat(filename, &sbuf);
#endif
		if (ret != 0 || !S_ISREG(sbuf.st_mode)) {
			return;
		}
		mt_time = sbuf.st_mtime;
		size = sbuf.st_size;
	}
	KFileModified(const KFileModified& a) = default;
	bool operator == (const KFileModified& a) const {
		return memcmp(this, &a, sizeof(KFileModified)) == 0;
	}
	bool operator != (const KFileModified& a) const {
		return !this->operator==(a);
	}
	bool empty() {
		return mt_time == 0;
	}
	KString to_string() {
		KStringBuf s;
		s << mt_time << "|" << size;
		return s.str();
	}
	time_t mt_time;
	INT64 size;
};
#endif
