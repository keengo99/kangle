#include "global.h"
#include "KConfig.h"
#include "KDsoExtendManage.h"
#include "KDsoConfigParser.h"
void KDsoConfigListen::on_event(kconfig::KConfigTree* tree, KXmlNode* xml, kconfig::KConfigEventType ev) {
	switch (ev) {
	case kconfig::KConfigEventType::New:
		if (conf.dem == NULL) {
			conf.dem = new KDsoExtendManage;
		}
		conf.dem->add(xml->attributes);
		break;
	default:
		klog(KLOG_ERR, "dso_extend do not support ev=[%d]\n", ev);
		break;
	}
}
#if 0
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
#endif
