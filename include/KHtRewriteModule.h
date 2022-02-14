/*
 * KHtRewriteModule.h
 *
 *  Created on: 2010-9-27
 *      Author: keengo
 */

#ifndef KHTREWRITEMODULE_H_
#define KHTREWRITEMODULE_H_

#include "KHtModule.h"
//next|N
#define REWRITE_NEXT      1
//last|L
#define REWRITE_LAST      (1<<1)
//forbidden|F
#define REWRITE_FOBIDDEN  (1<<2)
//gone|G
#define REWRITE_GONE      (1<<3)
//chain|C
#define REWRITE_CHAIN_AND (1<<4)
//nocase|NC
#define REWRITE_NOCASE    (1<<5)
//nosubreq|NS
#define REWRITE_NOSUBREQ  (1<<6)
//proxy|P
#define REWRITE_PROXY     (1<<7)
//redirect
#define REWRITE_REDIRECT  (1<<8)
//skip|S=num
#define REWRITE_SKIP      (1<<9)

//qsappend|QSA
#define REWRITE_QSA       (1<<10)


#define REWRITE_OR        (1<<20)
struct rewrite_flag_t
{
	int flag;
	int code;
	int skip;
};
class KHtRewriteModule: public KHtModule {
public:
	KHtRewriteModule();
	virtual ~KHtRewriteModule();
	bool process(KApacheConfig *htaccess, const char *cmd,
			std::vector<char *> &item);
	bool getXml(std::stringstream &s);
	//bool getTable(std::stringstream &s);
	//bool getChain(std::stringstream &s);
private:
	void chainStart(KApacheConfig *htaccess);
	void chainEnd();
	bool enable;
	std::stringstream chain;
	std::stringstream rule;
	std::stringstream action;
	std::string rewriteBase;
	bool chainStarted;
	bool lastCondOr;
	int cur_chainid;
	bool last_chainand;
};
#endif /* KHTREWRITEMODULE_H_ */
