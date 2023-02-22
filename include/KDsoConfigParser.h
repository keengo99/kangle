#ifndef KDSOCONFIGPARSER_H_99
#define KDSOCONFIGPARSER_H_99
#include "KXmlEvent.h"
#include "KConfigTree.h"
void on_dso_event(void *data, kconfig::KConfigTree* tree, KXmlNode* xml, kconfig::KConfigEventType ev);
#endif
