/*
 * KSingleProgram.h
 *
 *  Created on: 2010-4-30
 *      Author: keengo
 */

#ifndef KSINGLEPROGRAM_H_
#define KSINGLEPROGRAM_H_
#include "kfile.h"
class KSingleProgram {
public:
	KSingleProgram();
	virtual ~KSingleProgram();
	int checkRunning(const char *pidFile);
	bool lock(const char *pidFile);
	void unlock();
	bool deletePid();
	bool savePid(int savePid);
private:
	FILE_HANDLE fd;
#ifdef _WIN32
	OVERLAPPED ov;
#endif
};
extern KSingleProgram singleProgram;
#endif /* KSINGLEPROGRAM_H_ */
