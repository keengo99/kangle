#include <vector>
#include "klog.h"
#include "KExtendProgram.h"
#include "KFileName.h"
#include "utils.h"
#include "kmalloc.h"
#include "KTempleteVirtualHost.h"
#include "KSequence.h"
#include "kfile.h"
#ifndef _WIN32
extern char **environ;
#endif
void addCurrentEnv(KCmdEnv *env)
{
	//把现有的环境变量加入
	for(int i=0;;i++){
		char *e = environ[i];
		if(e==NULL){
			break;
		}
		env->addEnv(e);
	}
}
KCmdEnv *make_cmd_env(char **envs)
{
	if(envs==NULL || envs[0]==NULL){
		return NULL;
	}
	KCmdEnv *env = new KCmdEnv;
	for(int i=0;;i++){
		if(envs[i]==NULL){
			break;
		}
		env->addEnv(envs[i]);
	}
	addCurrentEnv(env);
	env->addEnvEnd();
	return env;
}
KCmdEnv *make_cmd_env(std::map<std::string,std::string> &attribute,KExtendProgramString *ds)
{
	if(attribute.size()==0){
		return NULL;
	}
	KCmdEnv *env = new KCmdEnv;
	for(std::map<std::string,std::string>::iterator it = attribute.begin();
		it!=attribute.end();
		it++){
			if (ds) {
				char *value = ds->parseString((*it).second.c_str());
				env->addEnv((*it).first.c_str(),value);
				free(value);
			} else {
				env->addEnv((*it).first.c_str(),(*it).second.c_str());
			}
	}
	addCurrentEnv(env);
	env->addEnvEnd();
	return env;
}
u_short currend_extend_id = 1;
Token_t KExtendProgramString::getToken(bool &result) {
#ifdef ENABLE_VH_RUN_AS
	if (vh) {
		return vh->createToken(result);
	}
#endif
	result = false;
	return NULL;
}
const char *KExtendProgramString::interGetValue(const char *name) {
		if (vh == NULL) {
			return "";
		}

		if (strcasecmp(name, "doc_root") == 0) {
			return vh->doc_root.c_str();
		}
		if (strcasecmp(name, "name") == 0) {
			return vh->name.c_str();
		}
#ifdef ENABLE_VH_RUN_AS
#ifndef _WIN32
		if (strcasecmp(name, "uid") == 0) {
			s.clean();
			s << vh->id[0];
			return s.getString();
		}
		if (strcasecmp(name, "gid") == 0) {
			s.clean();
			s << vh->id[1];
			return s.getString();
		}
#else
		if(strcasecmp(name,"user")==0) {
			return vh->user.c_str();
		}
		if(strcasecmp(name,"password")==0) {
			return vh->group.c_str();
		}
#endif
#endif
		if (strcasecmp(name, "templete") ==0) {
			if (vh->tvh==NULL) {
				return NULL;
			}
			return vh->tvh->name.c_str();
		}
		if (strcasecmp(name, "subtemplete") ==0) {
			if (vh->tvh==NULL) {
				return NULL;
			}
			s.clean();
			s << vh->tvh->name.c_str();
			char *subname = s.getString();
			char *p = strchr(subname,':');
			if (p==NULL) {
				return NULL;
			}
			return p+1;
		}		
		if (strcasecmp(name, "time") == 0) {
			s.clean();
			s << (INT64) time(NULL);
			return s.getString();
		}
		if (strcasecmp(name, "rand") == 0) {
			s.clean();
			s << rand();
			return s.getString();
		}
		if (strcasecmp(name, "pid") == 0) {
			s.clean();
			s << pid;
			return s.getString();
		}
		if (vh->getEnvValue(name, vh_value)) {
			return vh_value.c_str();
		}
		if (conf.gvm->globalVh.getEnvValue(name, vh_value)) {
			return vh_value.c_str();
		}
		//{{ent
		char *tbuf = strdup(name);
		char *p = strchr(tbuf,'=');
		if (p) {
			*p = '\0';
			const char *split_char = p + 1;
			s.clean();
			std::list<KSubVirtualHost *>::iterator it;
			for (it = vh->hosts.begin(); it != vh->hosts.end(); it++) {
				if (!(*it)->allSuccess) {
					continue;
				}
				if (s.getSize()>0) {
					s << split_char;
				}
				if (strcasecmp(tbuf,"host_dir")==0) {
					if (*(*it)->host == '.') {
						s << "*";
					}
					s << (*it)->host << "|" << (*it)->doc_root;
				} else if (strcasecmp(tbuf, "host") == 0) {
					if (*(*it)->host == '.') {
						s << "*";
					}
					s << (*it)->host;
				} else	if (strcasecmp(tbuf, "subdir") == 0) {
					s << (*it)->dir;
				} else	if (strcasecmp(tbuf, "full_sub_doc_root") == 0) {
					s << (*it)->doc_root;
				}
			}
			if (strcasecmp(tbuf,"http_error")==0) {
				vh->getErrorEnv(split_char,s);
			} else if(strcasecmp(tbuf,"index_file")==0) {
				vh->getIndexFileEnv(split_char,s);
			}
			free(tbuf);
			return s.getString();		
		}
		free(tbuf);
		//}}
		return KDynamicString::getValue(name);
}
bool KExtendProgramConfig::handle(KExtendProgramString *ds) {
	KFileName file;
	char *src_file_name = NULL;
	char *dst_file_name = NULL;
	char *src_buf = NULL;
	char *dst_buf = NULL;
	KFile src_fp;
	KFile dst_fp;
	char *dir;
	int len;
	KStringBuf dst_tmp;
	bool result = false;
	src_file_name = ds->parseString(src.c_str());
	if (src_file_name == NULL) {
		klog(KLOG_ERR, "cann't parse src file name [%s]\n", src.c_str());
		return false;
	}
	if (!file.setName(src_file_name)) {
		klog(KLOG_ERR, "cann't get src_file[%s] infomation\n", src_file_name);
		goto done;
	}
	ds->addEnv("src_file",src_file_name);
	dir = getPath(src_file_name);
	if(dir){
		ds->addEnv("src_dir",dir);
		xfree(dir);
	}
	if (file.fileSize > 4194304) {
		klog(KLOG_ERR, "file [%s] is too big(size=%d) max is 4M\n", dst.c_str());
		goto done;
	}
	dst_file_name = ds->parseString(dst.c_str());
	if (dst_file_name == NULL) {
		klog(KLOG_ERR, "cann't parse dst file name[%s]\n", dst.c_str());
		goto done;
	}	
	dst_tmp << dst_file_name << "." << sequence.getNext() ;
	if (!force) {
		KFileName dstFile;
		if(dstFile.setName(dst_file_name)){
			//dst file is exsit.
			result = true;
			goto done;
		}
	}
	ds->addEnv("dst_file",dst_file_name);
	dir = getPath(dst_file_name);
	if(dir){
		ds->addEnv("dst_dir",dir);
		xfree(dir);
	}
	if (!src_fp.open(src_file_name,fileRead)) {
		klog(KLOG_ERR, "cann't open src file [%s] for read\n", file.getName());
		goto done;
	}
	src_buf = (char *) xmalloc((int)file.fileSize+1);
	len = src_fp.read(src_buf,(int)file.fileSize);
	if (len!=file.fileSize) {
		klog(KLOG_ERR, "cann't read complete src file [%s] readed=[%d],fileSize=[%d]\n", file.getName(),len,file.fileSize);
		goto done;
	}
	if (!dst_fp.open(dst_tmp.getString(),fileWrite)) {
		klog(KLOG_ERR, "cann't open dst file [%s] for write\n", dst_file_name);
		goto done;
	}
	if (only_copy) {
		if (file.fileSize!=dst_fp.write(src_buf,(int)file.fileSize)) {
			klog(KLOG_ERR,"cann't write to tmp file .\n");
			goto done;
		}
	} else {
		src_buf[file.fileSize] = '\0';
		dst_buf = ds->parseString(src_buf);
		if (dst_buf == NULL) {
			klog(KLOG_ERR, "cann't parse src file content [%s]\n", file.getName());
			goto done;
		}
		len = (int)strlen(dst_buf);
		if (len!=dst_fp.write(dst_buf,len)) {
			klog(KLOG_ERR,"cann't complete write to tmp file .\n");
			goto done;
		}
	}
	result = true;
	done: 
	if (dst_fp.opened()) {
		dst_fp.close();
		if (result) {
#ifndef _WIN32
			if (run_as_user) {
				bool token_result = false;
				Token_t token = ds->getToken(token_result);
				if(token_result && token){
					chown(dst_tmp.getString(),token[0],token[1]);
				}
			}
			result = (0==rename(dst_tmp.getString(),dst_file_name));
#else
			result = (TRUE == MoveFileEx(dst_tmp.getString(),dst_file_name,MOVEFILE_REPLACE_EXISTING));
#endif

		} 
		if (!result) {
			klog(KLOG_NOTICE,"now remove tmp file [%s]\n",dst_tmp.getString());
			//如果没有成功，则删除临时文件			
			unlink(dst_tmp.getString());
		}
	}
	if (src_file_name) {
		xfree(src_file_name);
	}
	if (dst_file_name) {
		xfree(dst_file_name);
	}
	if (src_buf) {
		xfree(src_buf);
	}
	if (dst_buf) {
		xfree(dst_buf);
	}
	return result;
}
bool KExtendProgramUnlink::handle(KExtendProgramString *ds) {
	char *buf = ds->parseString(file.c_str());
	if (buf == NULL) {
		return false;
	}
	int ret = unlink(buf);
	xfree(buf);
	return ret == 0;
}
bool KExtendProgramCmd::handle(KExtendProgramString *ds) {
	KPipeStream *st = NULL;
	std::vector<std::string> args;
	explode(cmd.c_str(), ' ', args);
	char **arg = new char *[args.size() + 1];
	size_t i = 0;
	for (; i < args.size(); i++) {
		arg[i] = ds->parseString(args[i].c_str());
	}
	arg[i] = NULL;
	Token_t token = NULL;
	bool result = true;
	if (run_as_user) {
		token = ds->getToken(result);
		if (!result) {
			klog(KLOG_ERR, "cann't get user token\n");
			goto done;
		}
	}
	st = createProcess(token, arg, NULL,
#ifdef _WIN32
			RDSTD_NONE
#else
			RDSTD_ALL
#endif
			);
	if (token != NULL) {
		KVirtualHost::closeToken(token);
	}
	result = false;
	if (st) {
		//等待子进程结束
		st->waitClose();
		delete st;
		result = true;
	}
done:
	for (i = 0; i < args.size(); i++) {
		xfree(arg[i]);
	}
	delete[] arg;
	return result;
}
KExtendProgramEvent *KExtendProgramEvent::buildEvent(std::map<std::string,
		std::string> &attribute) {
	KExtendProgramEvent *epe = NULL;
	if (attribute["unlink"].size() > 0) {
		KExtendProgramUnlink *ep = new KExtendProgramUnlink;
		ep->file = attribute["unlink"];
		epe = ep;
	} else if (attribute["cmd"].size() > 0) {
		KExtendProgramCmd *ep = new KExtendProgramCmd;
		ep->cmd = attribute["cmd"];
		ep->run_as_user = (attribute["run_as"] == "user");
		epe = ep;
	} else {
		std::string src_file = attribute["src_file"];
		std::string dst_file = attribute["dst_file"];
		if (src_file.size() > 0 && dst_file.size() > 0) {
			KExtendProgramConfig *ep = new KExtendProgramConfig;
			ep->src = src_file;
			ep->dst = dst_file;
			ep->run_as_user = (attribute["run_as"] == "user");
			if (attribute["force"]=="0") {
				ep->force = false;
			}
			ep->only_copy = (attribute["only_copy"] == "1");
			epe = ep;
		}
	}
	return epe;
}
KExtendProgram::~KExtendProgram() {
	std::list<KExtendProgramEvent *>::iterator it;
	for (it = preEvents.begin(); it != preEvents.end(); it++) {
		delete (*it);
	}
	for (it = postEvents.begin(); it != postEvents.end(); it++) {
		delete (*it);
	}
}
std::string KExtendProgram::getEnv()
{
	std::stringstream s;
	std::map<std::string,std::string>::iterator it;
	for(it=envs.begin();it!=envs.end();it++){
		if (it!=envs.begin()) {
			s << " ";
		}
		s << (*it).first << "=\"" << (*it).second << "\"";
	}
	return s.str();
}
bool KExtendProgram::addEvent(bool preEvent,
		std::map<std::string, std::string> &attribute) {
	KExtendProgramEvent *epe = KExtendProgramEvent::buildEvent(attribute);
	if (epe) {
		if (preEvent) {
			preEvents.push_back(epe);
		} else {
			postEvents.push_back(epe);
		}
		return true;
	}
	return false;
}
bool KExtendProgram::preLoad(KExtendProgramString *ds) {
#ifdef _WIN32
	RevertToSelf();
#endif
	std::list<KExtendProgramEvent *>::iterator it;
	for (it = preEvents.begin(); it != preEvents.end(); it++) {
		(*it)->handle(ds);
	}
	return true;
}
bool KExtendProgram::postLoad(KExtendProgramString *ds) {
	std::list<KExtendProgramEvent *>::iterator it;
	for (it = postEvents.begin(); it != postEvents.end(); it++) {
		(*it)->handle(ds);
	}
	return true;
}
void KExtendProgram::buildConfig(std::stringstream &s) {
	//if (lifeTime != EXTENDPROGRAM_DEFAULT_LIFETIME) {
	s << " life_time='" << lifeTime << "'";
	//}
	if (idleTime != EXTENDPROGRAM_DEFAULT_IDLETIME) {
		s << " idle_time='" << idleTime << "'";
	}
	if (maxRequest != EXTENDPROGRAM_DEFAULT_MAXREQUEST) {
		s << " max_request='" << maxRequest << "'";
	}
	if(maxConnect != EXTENDPROGRAM_DEFAULT_MAXCONNECT){
		s << " max_connect='" << maxConnect << "'";
	}
	if (max_error_count>0) {
		s << " max_error_count='" << max_error_count << "'";
	}
	s << ">\n";
	std::list<KExtendProgramEvent *>::iterator it;
	for (it = preEvents.begin(); it != preEvents.end(); it++) {
		s << "\t\t<pre_event ";
		(*it)->build(s);
		s << "/>\n";
	}
	for (it = postEvents.begin(); it != postEvents.end(); it++) {
		s << "\t\t<post_event ";
		(*it)->build(s);
		s << "/>\n";
	}
}
void KExtendProgram::parseConfig(std::map<std::string, std::string> &attribute) {
	if (attribute["life_time"].size() > 0) {
		lifeTime = atoi(attribute["life_time"].c_str());
	}
	if (attribute["idle_time"].size() > 0) {
		idleTime = atoi(attribute["idle_time"].c_str());
	}
	if (attribute["max_request"].size() > 0) {
		maxRequest = atoi(attribute["max_request"].c_str());
	}
	if(attribute["max_connect"].size() > 0){
		maxConnect = atoi(attribute["max_connect"].c_str());
	}
	max_error_count = atoi(attribute["max_error_count"].c_str());
}
bool KExtendProgram::parseEnv(std::map<std::string, std::string> &attribute)
{
	envs.clear();
	for (std::map<std::string, std::string>::iterator it = attribute.begin();it!=attribute.end();it++) {
		envs[(*it).first] = (*it).second;
	}
	return true;
}
KCmdEnv *KExtendProgram::makeEnv(KExtendProgramString *ds)
{
	return make_cmd_env(envs,ds);
}
bool KExtendProgram::isChanged(KExtendProgram *ep)
{
	std::stringstream s1,s2;
	buildConfig(s1);
	ep->buildConfig(s2);
	if (s1.str()!=s2.str()) {
		return true;
	}
	return false;
}
