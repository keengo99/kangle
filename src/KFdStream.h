/*
 * KFdStream.h
 *
 *  Created on: 2010-6-11
 *      Author: keengo
 */

#ifndef KFDSTREAM_H_
#define KFDSTREAM_H_
#ifndef _WIN32
#include <unistd.h>
#endif
#include "kforwin32.h"
#include "KStream.h"

class KFdStream: public KStream {
public:
	KFdStream(int fd);
	virtual ~KFdStream();
	int write(const char *buf, int len) {
		return ::write(fd, buf, len);
	}
	int read(char *buf, int len) {
		return ::read(fd, buf, len);
	}
	int getfd() {
		return fd;
	}
private:
	int fd;
};

#endif /* KFDSTREAM_H_ */
