#ifndef KDSOCONFIGPARSER_H_99
#define KDSOCONFIGPARSER_H_99
#include "KXmlEvent.h"
class KDsoConfigParser : public KXmlEvent {
public:
	bool startElement(KXmlContext *context) override;
};
#endif
