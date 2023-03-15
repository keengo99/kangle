#ifndef KGL_NAMED_MODEL_H_INCLUDE
#define KGL_NAMED_MODEL_H_INCLUDE
#include "KConfigTree.h"
#include "KModel.h"
#include "KSharedObj.h"


class KNamedModel : public kconfig::KConfigListen
{
public:
	KNamedModel(const KSafeModel &model) : m(model){
		ref = 1;
	}
	KNamedModel* add_ref() {
		katom_inc((void*)&ref);
		return this;
	}
	void release() {
		if (katom_dec((void*)&ref) == 0) {
			delete this;
		}
	}
	KSafeAcl as_acl() {
		return KSafeAcl(static_cast<KAcl *>(m->add_ref()));
	}
	KSafeMark as_mark() {
		return KSafeMark(static_cast<KMark*>(m->add_ref()));
	}
	kconfig::KConfigEventFlag config_flag() const {
		return kconfig::ev_subdir;
	}
	bool on_config_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) {
		m->parse_child(ev);
		return true;
	}
private:
	~KNamedModel() noexcept {

	}
	KSafeModel m;
	volatile uint32_t ref;
};
using KSafeNamedModel = KSharedObj<KNamedModel>;
#endif

