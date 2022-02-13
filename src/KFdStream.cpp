/*
 * KFdStream.cpp
 *
 *  Created on: 2010-6-11
 *      Author: keengo
 */

#include "KFdStream.h"

KFdStream::KFdStream(int fd) {
	this->fd = fd;
}

KFdStream::~KFdStream() {
	::close(fd);
}

