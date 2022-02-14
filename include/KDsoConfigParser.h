#ifndef KDSOCONFIGPARSER_H_99
#define KDSOCONFIGPARSER_H_99
#include "KXmlEvent.h"
class KDsoConfigParser : public KXmlEvent {
	bool startElement(KXmlContext *context, std::map<std::string,std::string> &attribute);
};
#endif
