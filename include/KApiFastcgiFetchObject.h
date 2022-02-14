/*
 * KApiFastcgiFetchObject.h
 *
 *  Created on: 2010-6-15
 *      Author: keengo
 */

#ifndef KAPIFASTCGIFETCHOBJECT_H_
#define KAPIFASTCGIFETCHOBJECT_H_

#include "KFastcgiFetchObject.h"
#include "KApiRedirect.h"
#include "KFileName.h"
class KApiFastcgiFetchObject: public KFastcgiFetchObject {
public:
	KApiFastcgiFetchObject()
	{

	};
protected:
	bool isExtend() {
		return true;
	}
};

#endif /* KAPIFASTCGIFETCHOBJECT_H_ */
