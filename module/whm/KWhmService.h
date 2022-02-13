/*
 * KWhmService.h
 *
 *  Created on: 2010-8-30
 *      Author: keengo
 */

#ifndef KWHMSERVICE_H_
#define KWHMSERVICE_H_
#include "KServiceProvider.h"
class KWhmService {
public:
	KWhmService();
	virtual ~KWhmService();
	bool service(KServiceProvider *provider);
private:

};

#endif /* KWHMSERVICE_H_ */
