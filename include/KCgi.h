/*
 * KCgi.h
 *
 *  Created on: 2010-6-11
 *      Author: keengo
 * linuxœ¬cgi¿‡
 */

#ifndef KCGI_H_
#define KCGI_H_
#include "KStream.h"
#include "KCgiEnv.h"
class KCgi {
public:
	KCgi();
	virtual ~KCgi();
	bool redirectIO(int rin, int rout, int rerr = 2);
	bool run(const char *cmd, char ** arg, KCgiEnv *env);
	bool cmdModel;
};
#endif /* KCGI_H_ */
