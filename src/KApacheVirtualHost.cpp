#include "KHtAccess.h"
#include "KHtRewriteModule.h"
#include "KApacheVirtualHost.h"
#include "KXml.h"
bool KApacheVirtualHost::process(KApacheConfig *htaccess,const char *cmd,std::vector<char *> &item)
{	
	KApacheVirtualHostItem *vh = vitem?vitem:&global;
	if (strcasecmp(cmd,"DocumentRoot")==0) {
		vh->documentRoot = item[0];
		return true;
	}
	if (strcasecmp(cmd,"SSLCertificateFile")==0) {
		vh->certificate = htaccess->getFullPath(item[0]);
		return true;
	}
	if (strcasecmp(cmd,"SSLCertificateKeyFile")==0) {
		vh->certificate_key = htaccess->getFullPath(item[0]);
		return true;
	}
	if (strcasecmp(cmd,"DirectoryIndex") == 0) {
		for (size_t i=0;i<item.size();i++) {
			vh->index.push_back(item[i]);
		}
	}
	if (vitem==NULL) {
		return false;
	}
	if(strcasecmp(cmd,"ServerName")==0){
		vitem->serverName = item[0];
		return true;
	}
	if(strcasecmp(cmd,"ServerAlias")==0){
		vitem->hosts.push_back(item[0]);
		return true;
	}
	return false;
}
bool KApacheVirtualHost::getXml(std::stringstream &s)
{
	std::list<KApacheVirtualHostItem *>::iterator it;
	if(global.certificate.size()>0){
		s << "<certificate>" << global.certificate << "</certificate>\n";
	}
	if(global.certificate_key.size()>0){
		s << "<certificate_key>" << global.certificate_key << "</certificate_key>\n";
	}
	if (vitems.size()>0) {
		s << "<vhs>\n";
	}
	global.buildIndex(s);
	if (global.documentRoot.size()>0) {
		s << "<vh name='default' doc_root='" << KXml::param(global.documentRoot.c_str()) << "' htaccess='.htaccess' templete='_apache'>\n";
		s << "<host>*</host>\n";
		s << "</vh>";
	}
	for (it=vitems.begin();it!=vitems.end();it++) {
		s << "<vh name='" << (*it)->serverName << "' doc_root='" << KXml::param((*it)->documentRoot.c_str()) << "'";
		if ((*it)->certificate.size()>0) {
			s << " certificate='" << (*it)->certificate << "'";
		}
		if ((*it)->certificate_key.size()>0) {
			s << " certificate_key='" << (*it)->certificate_key << "'";
		}
		s << " htaccess='.htaccess' templete='_apache'>\n";
		(*it)->buildIndex(s);
		if ((*it)->port>0) {
			s << "<port>" << (*it)->port << "</port>\n";
		}
		s << "<host>" << (*it)->serverName << "</host>\n";
		for(std::list<std::string>::iterator it2=(*it)->hosts.begin();it2!=(*it)->hosts.end();it2++){
			s << "<host>" << (*it2) << "</host>\n";
		}
		s << "</vh>\n";
	}
	if (vitems.size()>0) {
		s << "</vhs>\n";
	}

	return true;
}
bool KApacheVirtualHost::startContext(KApacheConfig *htaccess,const char *cmd,std::map<char *,char *,lessp_icase> &attribute)
{
	if(strcasecmp(cmd,"VirtualHost")!=0){
		return false;
	}
	if(vitem==NULL){
		vitem = new KApacheVirtualHostItem;
		vitem->port = 0;
	}
	if(attribute.size()>0){
		char *v = (*(attribute.begin())).first;
		char *p = strchr(v,':');
		if (p) {
			vitem->port = atoi(p+1);
		}
	}
	return true;
}
bool KApacheVirtualHost::endContext(KApacheConfig *htaccess,const char *cmd)
{
	if(vitem){
		vitems.push_back(vitem);
		vitem = NULL;
	}
	return true;
}
