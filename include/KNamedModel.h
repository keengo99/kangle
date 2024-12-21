#ifndef KGL_NAMED_MODEL_H_INCLUDE
#define KGL_NAMED_MODEL_H_INCLUDE
#include "KConfigTree.h"
#include "KModel.h"
#include "KSharedObj.h"

class KNamedMark final: public KMark {
public:
	KMark* new_instance() {
		return static_cast<KMark *>(add_ref());
	}
	virtual const char* getName() override {
		return mark->getName();
	}
	virtual void parse_config(const khttpd::KXmlNodeBody* xml) override {

	}
	virtual void parse_child(const kconfig::KXmlChanged* changed)override {
	}
	virtual void get_display(KWStream& s)override {
	}
	virtual void get_html(KWStream& s) override {
	}
	/* return KF_STATUS_REQ_FALSE, KF_STATUS_REQ_TRUE,KF_STATUS_REQ_FINISHED */
	uint32_t process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override {
		return mark->process(rq, obj, fo);
	}
private:
	KSafeMark mark;
};
class KNamedModel : public kconfig::KConfigListen
{
public:
	KNamedModel(KString name, const KSafeModel& model) : name{ name },m(model) {
		ref = 1;
		m->named = name;
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
	KString name;
	KSafeModel m;
	volatile uint32_t ref;
};
using KSafeNamedModel = KSharedObj<KNamedModel>;
#endif

