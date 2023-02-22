#include "global.h"
#include "KConfig.h"
#include "KDsoExtendManage.h"
#include "KDsoConfigParser.h"

void on_dso_event(void* data, kconfig::KConfigTree* tree, KXmlNode* xml, kconfig::KConfigEventType ev) {
	switch (ev) {
	case kconfig::EvNew|kconfig::EvSubDir:
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
