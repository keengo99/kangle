/*
 * KHtModule.h
 *
 *  Created on: 2010-9-27
 *      Author: keengo
 */

#ifndef KHTMODULE_H_
#define KHTMODULE_H_
#include <vector>
#include <sstream>
#include <map>
#include "global.h"
#include "utils.h"
/*
 * .htaccess½âÎöÄ£¿é
 */
class KApacheConfig;
class KHtModule {
public:
	KHtModule();
	virtual ~KHtModule();
	virtual bool startContext(KApacheConfig *htaccess,const char *cmd,std::map<char *,char *,lessp_icase> &attribute)
	{
		return false;
	}
	virtual bool endContext(KApacheConfig *htaccess,const char *cmd)
	{
		return false;
	}
	virtual bool process(KApacheConfig *htaccess,const char *cmd,std::vector<char *> &item) = 0;
	virtual bool getXml(std::stringstream &s) = 0;
};
#endif /* KHTMODULE_H_ */
