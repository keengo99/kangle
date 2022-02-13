//{{ent
#ifndef KLICENSE_H
#define KLICENSE_H
#include <string>
#include <string.h>
#include <sstream>

#include "global.h"
#include "kmd5.h"
#include "utils.h"
#ifndef KANGLE_FREE
#define LICENSE_VERSION    2
#define SIGN1   "lddds_ERhd11dsss"
#define SIGN2   "ke%66lS)("
struct license_t {
	std::string id;
	std::string name;
	std::string ips;
	INT64 expireTime;
	int cpu;
	std::string passwd;
	char sn[2][33];
	std::vector<std::string> ip;
};
class KSimpleSign {
public:
	KSimpleSign() {
		index = 0;
		memset(buf, 0, sizeof(buf));
	}
	char *sign(const char *str) {
		const unsigned char *hot = (const unsigned char*) str;
		while (*hot) {
			buf[index] = buf[index] ^ *hot;
			hot++;
			index++;
			if (index == 8) {
				index = 0;
			}
		}
		return (char *) buf;
	}
	std::string getSign() {
		return b64encode(buf, 8);
	}
private:
	unsigned char buf[8];
	int index;
};
class KLicense {
public:
	KLicense() {
		loaded = false;
	}
	~KLicense() {

	}
	//bool download();
	bool LoadOrDownload();
	bool Load();
	bool load(const char *file);
	bool CheckOrDownload() {
		if (!loaded) {
			return false;
		}
		if (license.expireTime == 0) {
			return true;
		}
		INT64 nowTime = time(NULL);
		if (nowTime > license.expireTime) {
			return false;
		}
		return true;
	}
	bool checkIp() {
		if (!loaded) {
			return false;
		}
		return true;
	}
	license_t license;
private:
	bool loaded;
};
extern KLicense license;
enum vl_result {
	vl_success,
	vl_failed,
	vl_unknow,
};
KTHREAD_FUNCTION remote_verify_license(void* arg);
extern volatile	vl_result license_result;
void kgl_check_license(void *arg,time_t nowTime);
#endif
#endif
//}}
