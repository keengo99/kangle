#include "KHttpFilterDsoManage.h"
#include "KHttpFilterDso.h"
#ifdef ENABLE_KSAPI_FILTER
#if 0
int KHttpFilterDsoManage::cur_dso_index = 0;
KHttpFilterDsoManage::KHttpFilterDsoManage()
{
	head = NULL;
	last = NULL;
}
KHttpFilterDsoManage::~KHttpFilterDsoManage()
{
}
bool KHttpFilterDsoManage::add(std::map<std::string,std::string> &attribute)
{
	KHttpFilterDso *dso = new KHttpFilterDso(attribute["name"].c_str());
	dso->attribute=attribute;
	if (!dso->load(attribute["filename"].c_str())) {
		delete dso;
		return false;
	}
	if (!add(dso)) {
		delete dso;
		return false;
	}
	klog(KLOG_NOTICE,"load http filter name [%s] file [%s] successfully\n",dso->get_name(),dso->get_filename());
	return true;
}
bool KHttpFilterDsoManage::add(KHttpFilterDso *dso)
{
	const char *name = dso->get_name();
	if (name==NULL) {
		return false;
	}
	lock.Lock();
	if (internal_find(name)) {
		lock.Unlock();
		klog(KLOG_ERR,"http filter name [%s] is duplicated\n",name);
		return false;
	}
	dso->next = NULL;
	dso->index = cur_dso_index;
	++cur_dso_index;
	dsos.insert(std::pair<const char *,KHttpFilterDso *>(name,dso));
	if (last) {
		last->next = dso;
	} else {
		head = dso;
	}
	last = dso;
	lock.Unlock();
	return true;
}
KHttpFilterDso *KHttpFilterDsoManage::find(const char *name)
{
	lock.Lock();
	KHttpFilterDso *dso = internal_find(name);
	lock.Unlock();
	return dso;
}
KHttpFilterDso *KHttpFilterDsoManage::internal_find(const char *name)
{
	std::map<const char *,KHttpFilterDso *,lessp>::iterator it;
	it = dsos.find(name);
	if (it==dsos.end()) {
		return NULL;
	}
	return (*it).second;
}
void KHttpFilterDsoManage::html(std::stringstream &s)
{
	s << "<table border=1><tr><td>name</td><td>file</td><td>status</td></tr>";
	lock.Lock();
	KHttpFilterDso *dso = head;
	while (dso) {
		const char *filename = dso->get_filename();
		const char *orign_filename = dso->get_orign_filename();
		s << "<tr><td>" << dso->get_name() << "</td>";
		s << "<td><a href=# title='" 
			<< (filename?filename:"") 
			<< "'>"  
			<<  (orign_filename?orign_filename:"") 
			<< "</a></td>";
		s << "<td>&nbsp;</td>";
		dso = dso->next;
	}
	lock.Unlock();
	s << "</table>";
}
#endif
#endif
