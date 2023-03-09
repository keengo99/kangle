#ifndef KDSOEXTENDMANAGE_H_99
#define KDSOEXTENDMANAGE_H_99
#include <map>
#include <vector>
#include "KDsoExtend.h"
#include "utils.h"
#include "WhmContext.h"
#include "KConfigTree.h"

class KDsoExtendManage : public kconfig::KConfigListen
{
public:
	KDsoExtendManage()
	{

	}
	~KDsoExtendManage()
	{
		for (auto it = dsos.begin(); it != dsos.end(); it++) {
			delete (*it).second;
		}
		dsos.clear();
	}
	void html(std::stringstream &s);
	void whm(WhmContext *ctx);
	bool add(KDsoExtend *dso);
	bool add(const KXmlAttribute &attribute);
	void ListTarget(std::vector<std::string> &target);
	KRedirect *RefsRedirect(std::string &name);
	void shutdown();
	virtual bool on_config_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) override;
	kconfig::KConfigEventFlag config_flag() const override {
		return kconfig::ev_subdir | kconfig::ev_skip_vary;
	}
private:
	std::map<const char *, KDsoExtend *, lessp> dsos;
	KMutex lock;
};
#endif
