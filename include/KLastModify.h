#ifndef KLASTMODIFY_H_
#define KLASTMODIFY_H_
#include "lib.h"
class KLastModify {
public:
	void setTime(time_t lastTime) {
		m_lastTime=lastTime;
		char buf[41];
		memset(buf, 0, sizeof(buf));
		mk1123time(lastTime, buf, sizeof(buf));
		m_lastHttpString=buf;
	}
	;
	time_t m_lastTime;
	std::string m_lastHttpString;
};

#endif /*KLASTMODIFY_H_*/
