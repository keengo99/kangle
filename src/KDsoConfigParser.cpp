#include "global.h"
#include "KConfig.h"
#include "KDsoExtendManage.h"
#include "KDsoConfigParser.h"

void on_dso_event(void* data, kconfig::KConfigTree* tree,kconfig::KConfigEvent *ev) {
	switch (ev->type) {
	case kconfig::EvNew|kconfig::EvSubDir:
		if (conf.dem == NULL) {
			conf.dem = new KDsoExtendManage;
		}
		conf.dem->add(ev->xml->attributes());
		break;
	default:
		klog(KLOG_ERR, "dso_extend do not support ev=[%d]\n", ev->type);
		break;
	}
}
