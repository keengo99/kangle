#include "KDsoExtendManage.h"
bool KDsoExtendManage::add(KDsoExtend *dso)
{
	const char *name = dso->GetName();
	if (name == NULL) {
		return false;
	}	
	std::map<const char *, KDsoExtend *, lessp>::iterator it;
	lock.Lock();
	it = dsos.find(name);
	if (it != dsos.end()) {
		lock.Unlock();
		return false;
	}
	dsos.insert(std::pair<const char *, KDsoExtend *>(name, dso));
	lock.Unlock();
	return true;
}
int KDsoExtendManage::dump(WhmContext* ctx) {
	KLocker locker(&lock);
	for (auto&& dso : dsos) {
		auto sl = ctx->data()->add_obj_array("dso");
		if (!sl) {
			continue;
		}
		dso.second->dump(sl);
	}
	return WHM_OK;
}
bool KDsoExtendManage::add(const KXmlAttribute &attribute)
{
	/* filter was shipped as a DSO between 3.5 and 3.6.  It is built into the
	 * server again, so tolerate an old loader entry during rolling upgrades
	 * without requiring the obsolete binary to remain installed. */
	if (strcasecmp(attribute("name"), "filter") == 0) {
		klog(KLOG_NOTICE, "ignore legacy filter dso entry; filters are built in\n");
		return true;
	}
	{
		KLocker locker(&lock);
		if (dsos.find(attribute("name")) != dsos.end()) {
			klog(KLOG_ERR, "dso extend name [%s] is duplicate\n", attribute("name"));
			return false;
		}
	}
	KDsoExtend *dso = new KDsoExtend(attribute["name"].c_str());
	if (!dso->load(attribute("filename"),attribute)) {
		if (dso->init_called) {
			/* hooks registered by the module may still refer to it, keep it loaded. */
			klog(KLOG_ERR, "dso extend [%s] init failed, it will be keeped in memory\n", dso->GetName());
			return false;
		}
		delete dso;
		return false;
	}
	if (!add(dso)) {
		dso->shutdown();
		klog(KLOG_ERR, "dso extend [%s] add failed, it will be keeped in memory\n", dso->GetName());
		return false;
	}
	klog(KLOG_NOTICE, "load dso extend name [%s] file [%s] successfully\n", dso->GetName(), dso->GetFileName());
	return true;
}
void KDsoExtendManage::whm(WhmContext *ctx)
{
	std::map<const char *, KDsoExtend *, lessp>::iterator it;
	lock.Lock();
	for (it = dsos.begin();it != dsos.end();it++) {
		KDsoExtend *dso = (*it).second;
		KStringBuf s;
		const char *orign_filename = dso->GetOrignFileName();
		s << "<name>" << dso->GetName() << "</name><version>" << HIWORD(dso->version.module_version) << "." << LOWORD(dso->version.module_version) << "</version>";
		s << "<filename>" << (orign_filename ? orign_filename : "") << "</filename>";
		ctx->add("dso", s.c_str());
	}
	lock.Unlock();
}
void KDsoExtendManage::html(KWStream &s)
{
	s << "<table border=1><tr><td>name</td><td>file</td><td>upstream</td><td>version</td></tr>";
	std::map<const char *, KDsoExtend *, lessp>::iterator it;
	lock.Lock();
	for (it=dsos.begin();it!=dsos.end();it++) {
		KDsoExtend *dso = (*it).second;
		const char *filename = dso->GetFileName();
		const char *orign_filename = dso->GetOrignFileName();
		s << "<tr><td>" << dso->GetName() << "</td>";
		s << "<td><a href=# title='"
			<< (filename ? filename : "")
			<< "'>"
			<< (orign_filename ? orign_filename : "")
			<< "</a></td><td>";
		dso->ListUpstream(s);	
		s << "</td><td>" << HIWORD(dso->version.module_version) << "." << LOWORD(dso->version.module_version) << "</td></tr>";
	}
	lock.Unlock();
	s << "</table>";
}
void KDsoExtendManage::ListTarget(std::vector<KString> &target)
{
	std::map<const char *, KDsoExtend *, lessp>::iterator it;
	lock.Lock();
	for (it = dsos.begin(); it != dsos.end(); it++) {
		(*it).second->ListTarget(target);
	}
	lock.Unlock();
}
KRedirect *KDsoExtendManage::RefsRedirect(KString &name)
{
	size_t index = name.find(':');
	if (index == std::string::npos) {
		return NULL;
	}
	auto  dso_name = name.substr(0, index);
	std::map<const char *, KDsoExtend *, lessp>::iterator it;
	lock.Lock();
	it = dsos.find(dso_name.c_str());
	if (it == dsos.end()) {
		lock.Unlock();
		return NULL;
	}
	auto  rd_name = name.substr(index + 1);
	KRedirect *rd = (*it).second->RefsRedirect(rd_name);
	lock.Unlock();
	return rd; 
}
void KDsoExtendManage::shutdown()
{
	for (auto it = dsos.begin();it != dsos.end();it++) {
		(*it).second->shutdown();
	}
}
bool KDsoExtendManage::on_config_event(kconfig::KConfigTree* tree, kconfig::KConfigEvent* ev) {
	switch (ev->type) {
	case kconfig::EvNew | kconfig::EvSubDir:
		add(ev->new_xml->attributes());
		break;
#ifdef MALLOCDEBUG
	case kconfig::EvRemove| kconfig::EvSubDir:
	{
		KLocker locker(&lock);
		auto name = ev->old_xml->attributes()["name"];
		auto it = dsos.find(name.c_str());
		if (it == dsos.end()) {
			klog(KLOG_ERR, "cann't remove dso extend [%s]\n", name.c_str());
		} else {
			(*it).second->shutdown();
			delete (*it).second;
			dsos.erase(it);
		}
		break;
	}
#endif
	default:
		klog(KLOG_ERR, "dso_extend do not support ev=[%d]\n", ev->type);
		break;
	}
	return true;
}
