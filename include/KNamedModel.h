#ifndef KGL_NAMED_MODEL_H_INCLUDE
#define KGL_NAMED_MODEL_H_INCLUDE
#include "KConfigTree.h"
#include "KModel.h"
#include "KSharedObj.h"
#include "KModelPtr.h"

class KNamedModel : public kconfig::KConfigListen
{
public:
	KNamedModel(KString name, const KSafeModel& model) : name{ name },m(model) {
		ref = 1;
	}
	KNamedModel(KString name, KModel *model) : name{ name }, m(model->add_ref()) {
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
	KModel *get_module() {
		return m.get();
	}
	KModelPtr<KAcl> as_acl() {
		return KModelPtr<KAcl>(name, static_cast<KAcl *>(m.get()->add_ref()));
	}
	KModelPtr<KMark> as_mark() {
		return KModelPtr<KMark>(name, static_cast<KMark*>(m.get()->add_ref()));
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
	KString name;
	KSafeModel m;
	volatile uint32_t ref;
};
using KSafeNamedModel = KSharedObj<KNamedModel>;
#endif

