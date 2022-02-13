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
bool KDsoExtendManage::add(std::map<std::string, std::string> &attribute)
{
	KDsoExtend *dso = new KDsoExtend(attribute["name"].c_str());
	if (!dso->load(attribute["filename"].c_str(),attribute)) {
		delete dso;
		return false;
	}
	if (!add(dso)) {
		delete dso;
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
		ctx->add("dso", s.getString());
	}
	lock.Unlock();
}
void KDsoExtendManage::html(std::stringstream &s)
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
		s << "</td><td>" << HIWORD(dso->version.module_version) << "." << LOWORD(dso->version.module_version) << "</td>";
	}
	lock.Unlock();
	s << "</table>";
}
void KDsoExtendManage::ListTarget(std::vector<std::string> &target)
{
	std::map<const char *, KDsoExtend *, lessp>::iterator it;
	lock.Lock();
	for (it = dsos.begin(); it != dsos.end(); it++) {
		(*it).second->ListTarget(target);
	}
	lock.Unlock();
}
KRedirect *KDsoExtendManage::RefsRedirect(std::string &name)
{
	size_t index = name.find_first_of(':');
	if (index == std::string::npos) {
		return NULL;
	}
	std::string dso_name = name.substr(0, index);
	std::map<const char *, KDsoExtend *, lessp>::iterator it;
	lock.Lock();
	it = dsos.find(dso_name.c_str());
	if (it == dsos.end()) {
		lock.Unlock();
		return NULL;
	}
	std::string rd_name = name.substr(index + 1);
	KRedirect *rd = (*it).second->RefsRedirect(rd_name);
	lock.Unlock();
	return rd; 
}
void KDsoExtendManage::Shutdown()
{
	std::map<const char *, KDsoExtend *, lessp>::iterator it;
	for (it = dsos.begin();it != dsos.end();it++) {
		(*it).second->Shutdown();
	}
}