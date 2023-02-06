#include "global.h"
#include "KConfig.h"
#include "KDsoExtendManage.h"
#include "KDsoConfigParser.h"
bool KDsoConfigParser::startElement(KXmlContext *context)
{
#ifdef ENABLE_KSAPI_FILTER
	if (context->path == "config") {
		if (context->qName == "dso_extend") {
			if (conf.dem == NULL) {
				conf.dem = new KDsoExtendManage;
			}
			conf.dem->add(context->attribute);
		}
	}
#endif
	return true;
}
