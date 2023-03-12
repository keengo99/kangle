#include "KWriteBackManager.h"
#include<sstream>
#include "lang.h"
#include "kmalloc.h"
using namespace std;
#ifdef ENABLE_WRITE_BACK
KWriteBackManager writeBackManager;
static KWriteBack *curWriteBack=NULL;
static bool edit=false;
KWriteBackManager::KWriteBackManager() {
}

KWriteBackManager::~KWriteBackManager() {
	destroy();
}
void KWriteBackManager::destroy() {
	std::map<KString,KWriteBack *>::iterator it;
	for (it=writebacks.begin(); it!=writebacks.end(); it++) {
		(*it).second->release();
	}
	writebacks.clear();
}
KWriteBack * KWriteBackManager::refsWriteBack(KString name)
{
	lock.RLock();
	KWriteBack *wb = getWriteBack(name);
	if(wb){
		wb->add_ref();
	}
	lock.RUnlock();
	return wb;
}
KWriteBack * KWriteBackManager::getWriteBack(KString table_name) {
	std::map<KString,KWriteBack *>::iterator it;
	it = writebacks.find(table_name);
	if (it!=writebacks.end()) {
		return (*it).second;
	}
	return NULL;
}
bool KWriteBackManager::editWriteBack(KString name, KWriteBack &m_a,
		KString &err_msg) {
	lock.WLock();
	KWriteBack *m_tmp=getWriteBack(name);
	if(m_tmp==NULL) {
		err_msg=LANG_TABLE_NAME_ERR;
		lock.WUnlock();
		return false;
	}
	m_tmp->name=m_a.name;
	m_tmp->setMsg(m_a.getMsg());
	lock.WUnlock();
	return true;

}
bool KWriteBackManager::newWriteBack(KString name, KString msg,
		KString &err_msg) {
	bool result=false;
	if (name.size()<2 || name.size()>32) {
		err_msg=LANG_TABLE_NAME_LENGTH_ERROR;
		return false;
	}
	KWriteBack *m_a=new KWriteBack;
	m_a->name=name;
	m_a->setMsg(msg);
	lock.WLock();
	if(getWriteBack(name)!=NULL) {
		delete m_a;
		goto done;
	}
	writebacks.insert(std::pair<KString,KWriteBack *>(name,m_a));
	result=true;
	done:
	lock.WUnlock();
	if(!result) {
		err_msg=LANG_TABLE_NAME_IS_USED;
	}
	return result;
}
bool KWriteBackManager::delWriteBack(KString name, KString &err_msg) {
	err_msg=LANG_TABLE_NAME_ERR;
	lock.WLock();
	auto it=writebacks.find(name);
	if (it==writebacks.end()) {
		lock.WUnlock();
		return false;
	}
	if ((*it).second->get_ref()>1) {
		err_msg=LANG_TABLE_REFS_ERR;
		lock.WUnlock();
		return false;
	}
	(*it).second->release();
	writebacks.erase(it);
	lock.WUnlock();
	return true;
}
std::vector<KString> KWriteBackManager::getWriteBackNames() {
	std::vector<KString> table_names;
	std::map<KString,KWriteBack *>::iterator it;
	KRLocker locker(&lock);
	for(it=writebacks.begin();it!=writebacks.end();it++) {
		table_names.push_back((*it).first);
	}
	return table_names;
}
KString KWriteBackManager::writebackList(KString name) {
	KStringBuf s;
	std::map<KString,KWriteBack *>::iterator it;
	s << "<html><LINK href=/main.css type='text/css' rel=stylesheet><body>\n";
	KWriteBack *m_a=NULL;
	s << "<table border=1><tr><td>" << LANG_OPERATOR << "</td><td>"
			<< LANG_NAME << "</td><td>" << LANG_REFS << "</td></tr>\n";
	lock.RLock();

	for(it=writebacks.begin();it!=writebacks.end();it++) {
		m_a = (*it).second;
		s << "<tr><td>";
		s << "[<a href=\"javascript:if(confirm('" << LANG_CONFIRM_DELETE << m_a->name;
		s << "')){ window.location='/writebackdelete?name=" << m_a->name << "';}\">" << LANG_DELETE << "</a>]";
		s << "[<a href=\"/writeback?name=" << m_a->name << "\">" << LANG_EDIT << "</a>]";
		s << "</td><td>" << m_a->name << "</td><td>" << m_a->get_ref() << "</td></tr>\n";
	}
	if (name.size()>0) {
		m_a=getWriteBack(name);
	} else {
		m_a = NULL;
	}
	s << "</table>\n<hr>";
	s << "<form action=" << (m_a?"/writebackedit":"/writebackadd") << " meth=post>\n";
	s << LANG_NAME << ":<input name=name value='" << (m_a?m_a->name:"") << "'><br>";
	s << LANG_MSG << ":<textarea name=msg rows=7 cols=50>" << (m_a?m_a->getMsg():"") << "</textarea><br>\n";
	if(m_a) {
		s << "<input type=hidden name=namefrom value='" << m_a->name << "'>\n";
	}
	lock.RUnlock();
	s << "<input type=submit value=" << (m_a?LANG_EDIT:LANG_SUBMIT) << "></form>\n";
	s << endTag() << "</body></html>";
	return s.str();

}
bool KWriteBackManager::addWriteBack(KWriteBack *wb)
{
	lock.WLock();
	if (getWriteBack(wb->name)) {
		lock.WUnlock();
		return false;
	}
	writebacks.insert(std::pair<KString,KWriteBack *>(wb->name,wb));
	lock.WUnlock();
	return true;
}
#endif
