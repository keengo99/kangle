#include "global.h"
#include "KConfig.h"
#include "KDsoExtendManage.h"
#include "KDsoConfigParser.h"
bool KDsoConfigParser::startElement(KXmlContext *context, std::map<std::string, std::string> &attribute)
{
#ifdef ENABLE_KSAPI_FILTER
	if (context->path == "config") {
		if (context->qName == "dso_extend") {
			if (conf.dem == NULL) {
				conf.dem = new KDsoExtendManage;
			}
			conf.dem->add(attribute);
		}
	}
#endif
	return true;
}
