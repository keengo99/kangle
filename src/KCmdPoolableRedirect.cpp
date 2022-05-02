/*
 * KCmdPoolableRedirect.cpp
 *
 *  Created on: 2010-8-26
 *      Author: keengo
 */
#include "utils.h"
#include "KCmdPoolableRedirect.h"
#include "KApiRedirect.h"
#include "KCmdProcess.h"
#include "http.h"

using namespace std;
#ifdef ENABLE_VH_RUN_AS
KCmdPoolableRedirect::KCmdPoolableRedirect() {
	//lifeTime = EXTENDPROGRAM_DEFAULT_LIFETIME;
	type = WORK_TYPE_SP;
	chuser = true; 
#ifndef _WIN32
	sig = SIGKILL;
#endif
	port = 0;
	lock = kfiber_mutex_init();
}
KCmdPoolableRedirect::~KCmdPoolableRedirect() {
	kfiber_mutex_destroy(lock);
}
KUpstream* KCmdPoolableRedirect::GetUpstream(KHttpRequest* rq)
{
	return pm.GetUpstream(rq, this);
}
void KCmdPoolableRedirect::buildXML(std::stringstream &s) {
	s << "\t<cmd name='" << name << "' proto='";
	s << KPoolableRedirect::buildProto(proto) << "'";
	s << " file='" << cmd << "'";
	//s << " split_char='" << split_char << "'";
	s << " type='" << getTypeString(type) << "'";
	if(!chuser){
		s << " chuser='0'";
	}	
	if(worker>0){
		s << " worker='" << worker << "'";
	}
	if(port>0){
		s << " port='" << port << "'";
	}
	if (pm.param.empty()) {
		s << " param='" << pm.param << "'";
	}
#ifndef _WIN32
	if (sig>0 && sig!=SIGKILL) {
		s << " sig='" << sig << "'";
	}
#endif
	KExtendProgram::buildConfig(s);
	if(envs.size()>0){
		s << "\t\t<env ";
		for(std::map<std::string,std::string>::iterator it = envs.begin();it!=envs.end();it++){
			s << (*it).first << "=\"" << (*it).second << "\" ";
		}
		s << "/>\n";
	}
	s << "\t</cmd>\n";
}
bool KCmdPoolableRedirect::Exec(KVirtualHost* vh, KListenPipeStream* st,bool isSameRunning)
{
	std::vector<char*> args;
	int rdst = RDSTD_NONE;
	KExtendProgramString ds(name.c_str(), vh);
	Token_t token = NULL;
	bool result = false;
	if (chuser) {
		token = vh->createToken(result);
		if (!result) {
			return NULL;
		}
		
	}
	char* cmd_buf = xstrdup(cmd.c_str());
	explode_cmd(cmd_buf, args);
	int args_count = (int)args.size() + 1;
	if (type == WORK_TYPE_MP && worker > 0) {
		args_count++;
		rdst = RDSTD_NAME_PIPE;
		st->process.detach();
	}
	char** arg = new char* [args_count];
	size_t i = 0;
	if (type == WORK_TYPE_MP) {
		if (worker > 0) {
			arg[i] = strdup(conf.extworker.c_str());
			i++;
		} else {
			rdst = RDSTD_INPUT;
		}
	}
	std::string program;
	for (unsigned j = 0; j < args.size(); j++) {
		const char* tmp_arg = args[j];
		if (j == 0) {
			//cmd
			if (!isAbsolutePath(tmp_arg)) {
				program = conf.path + tmp_arg;
				tmp_arg = program.c_str();
			}
		}
		char* a = ds.parseString(tmp_arg);
		if (a == NULL) {
			continue;
		}
		if (*a == '\0') {
			free(a);
			continue;
		}

		arg[i] = a;
		i++;
	}
	arg[i] = NULL;
	/**
	* 对于同一个应用程序池，有一个正在运行(isSameRunning),这时我们是无法确切知道该进程的状态
	* 有可能该进程还正在启动，前面调用的preLoad(写配置文件模板),还未发生作用。
	* 此时如果再调用preLoad, 即发生竞态条件(race condition)
	*/
	if (isSameRunning || preLoad(&ds)) {
		KCmdEnv* env = makeEnv(&ds);
		result = ::createProcess(st, token, arg, env, rdst);
		if (env) {
			delete env;
		}
		if (result) {
			ds.setPid(st->process.getProcessId());
			if (type == WORK_TYPE_MP && worker>0) {
				FCGI_Header h;
				memset(&h, 0, sizeof(FCGI_Header));
				h.type = CMD_CREATE_PROCESS;
#ifdef KSOCKET_UNIX	
			//	if (unix_socket) {
			//		h.type = CMD_CREATE_PROCESS_UNIX;
			//	}
#endif
				h.id = worker;
				st->write_all((char*)&h, sizeof(FCGI_Header));
				if (st->read_all((char*)&h, sizeof(FCGI_Header))) {
					st->setPort(h.id);
				}
#ifdef KSOCKET_UNIX	
				//if (unix_socket) {
			//		std::stringstream s;
			//		s << "/tmp/extworker." << h.id << ".sock";
			//		s.str().swap(unix_path);
				//}
#endif
			}
			st->closeServer();
		}
	}
	for (i = 0;; i++) {
		if (arg[i] == NULL) {
			break;
		}
		xfree(arg[i]);
	}
	delete[] arg;
	xfree(cmd_buf);
	if (token) {		
		KVirtualHost::closeToken(token);			
	}
	return result;
}
#if 0
KTcpUpstream *KCmdPoolableRedirect::createPipeStream(KVirtualHost *vh,KListenPipeStream *st, std::string &unix_path,bool isSameRunning){
	vector<char *> args;
	int rdst = RDSTD_NONE;
	KExtendProgramString ds(name.c_str(),vh);
	Token_t token = NULL;
	bool result = false;
	KTcpUpstream *socket = NULL;
	st->process.sig = sig;
#ifdef KSOCKET_UNIX	
	bool unix_socket = conf.unix_socket;
#endif
	if(chuser){
		token = vh->createToken(result);
		if (!result) {
			return NULL;
		}
	}
	char *cmd_buf = xstrdup(cmd.c_str());
	explode_cmd(cmd_buf,args);
	int args_count = args.size() + 1;
	bool saveProcessToFile = true;
	if (type==WORK_TYPE_MP && worker>0) {
		args_count++ ;
		rdst = RDSTD_NAME_PIPE;
		st->process.detach();	
		saveProcessToFile = false;
	}
	char **arg = new char *[args_count];
	size_t i = 0;
	if(type==WORK_TYPE_MP){
		if(worker>0){
			arg[i] = strdup(conf.extworker.c_str());
			i++;
		} else {
			rdst = RDSTD_INPUT;
		}
	}
	std::string program;
	for (unsigned j=0; j < args.size(); j++) {
		const char *tmp_arg = args[j];
		if (j == 0) {
			//cmd
			if (!isAbsolutePath(tmp_arg)) {
				program = conf.path + tmp_arg;
				tmp_arg = program.c_str();
			}
		}
		char *a = ds.parseString(tmp_arg);
		if(a==NULL){
			continue;
		}
		if(*a=='\0'){
			free(a);
			continue;
		}

		arg[i] = a;
		i++;
	}
	arg[i] = NULL;
	int port2 = 0;
	//{{ent
	/**
	* 对于同一个应用程序池，有一个正在运行(isSameRunning),这时我们是无法确切知道该进程的状态
	* 有可能该进程还正在启动，前面调用的preLoad(写配置文件模板),还未发生作用。
	* 此时如果再调用preLoad, 即发生竞态条件(race condition)
	*/
	//}}
	if (isSameRunning || preLoad(&ds)) {
		KCmdEnv *env = makeEnv(&ds);
		result = ::createProcess(st, token, arg, env, rdst);
		if (env) {
			delete env;
		}
		if(result) {
			if (saveProcessToFile) {
				st->process.saveFile(conf.tmppath.c_str(),(unix_path.size()>0?unix_path.c_str():NULL));
			}
			ds.setPid(st->process.getProcessId());
			if (type==WORK_TYPE_MP) {
				if (worker>0) {
					FCGI_Header h;
					memset(&h,0,sizeof(FCGI_Header));
					h.type = CMD_CREATE_PROCESS;
	#ifdef KSOCKET_UNIX	
					if(unix_socket){
						h.type = CMD_CREATE_PROCESS_UNIX;
					}
	#endif
					h.id = worker;
					st->write_all((char *)&h,sizeof(FCGI_Header));
					if (st->read_all((char *)&h,sizeof(FCGI_Header))) {
						st->setPort(h.id);
					}
	#ifdef KSOCKET_UNIX	
					if(unix_socket){
						std::stringstream s;
						s << "/tmp/extworker." << h.id << ".sock";
						s.str().swap(unix_path);
					}
	#endif
				}
			} else {
				port2 = port;
			}	
			if (port2==0) {
				port2 = st->getPort();
			} else {
				st->setPort(port);
			}

			//st->portMap.swap(ds.port);
			st->closeServer();
			//printf("port2=[%d],unix_path=[%s]\n",port2,unix_path.c_str());
			if (port2 > 0 || !unix_path.empty()) {	
				//debug("cmd port=%d\n",port);
				//第一次连接，要点时间(10秒)
				for (int i = 0; i < (int)conf.time_out; i++) {
					//socket = new KTcpUpstream();					
					SOCKET sockfd = INVALID_SOCKET;
					if (!unix_path.empty()) {
#ifdef KSOCKET_UNIX
						struct sockaddr_un addr;
						ksocket_unix_addr(unix_path.c_str(),&addr);
						sockfd = ksocket_connect((sockaddr_i *)&addr, NULL,  1);
						if(ksocket_opened(sockfd)) {
							kconnection *cn = kconnection_new(NULL);
							cn->st.fd = sockfd;
							socket = new KTcpUpstream(cn);
							debug("connect to unix socket [%s] success\n",unix_path.c_str());
							break;
						}
#endif
					} else {
						sockaddr_i addr;
						ksocket_getaddr("127.0.0.1", port2, 0, AI_NUMERICHOST, &addr);
						sockfd = ksocket_connect(&addr, NULL,  1);
						if(ksocket_opened(sockfd)) {
							kconnection *cn = kconnection_new(&addr);
							cn->st.fd = sockfd;
							socket = new KTcpUpstream(cn);
							debug("connect to port %d success\n",port2);
							break;
						}
					}

					if (!st->process.isActive()) {
						break;
					}
					debug("cann't connect to port %d try it again (%d/%d)\n",port2,i,conf.time_out);
					sleep(1);
				}
			}
			postLoad(&ds);
		}
	}
	for (i=0;;i++) {
		if(arg[i]==NULL){
			break;
		}
		xfree(arg[i]);
	}
	delete[] arg;
	xfree(cmd_buf);
	if (token) {
		KVirtualHost::closeToken(token);
	}
#ifndef _WIN32
	if(socket){
		ksocket_no_block(socket->GetConnection()->st.fd);
	}
#endif
	return socket;
}
#endif
bool KCmdPoolableRedirect::setWorkType(const char *typeStr,bool changed) {
	int type = getTypeValue(typeStr);
	if (type != WORK_TYPE_MP) {
		type = WORK_TYPE_SP;
		pm.setWorker(1);
	} else {	
		if(worker<0){
			worker = 0;
		}
		pm.setWorker(worker);
	}
	if (this->type != type) {
		this->type = type;
		changed = true;
	}
	if (changed) {
		pm.clean();
	}
	std::stringstream s;
	s << "cmd:" << name;
	pm.setName(s.str().c_str());
	return true;
}
bool KCmdPoolableRedirect::parseEnv(std::map<std::string, std::string> &attribute)
{
	if (KExtendProgram::parseEnv(attribute)) {
		pm.clean();
		return true;
	}
	return false;
}
void KCmdPoolableRedirect::parseConfig(std::map<std::string, std::string> &attribute)
{
	KExtendProgram::parseConfig(attribute);
	bool changed = false;
	Proto_t proto = KPoolableRedirect::parseProto(attribute["proto"].c_str());
	if (this->proto!=proto) {
		this->proto = proto;
		changed = true;
	}
	if (cmd!=attribute["file"]) {
		changed = true;
		cmd = attribute["file"];
	}
	int worker = atoi(attribute["worker"].c_str());
	if (this->worker != worker) {
		changed = true;
		this->worker = worker;
	}
	bool chuser = (attribute["chuser"] != "0");
	if (this->chuser != chuser) {
		changed = true;
		this->chuser = chuser;
	}
	int port = atoi(attribute["port"].c_str());
	if (this->port != port) {
		changed = true;
		this->port = port;
	}
	int sig = atoi(attribute["sig"].c_str());
#ifndef _WIN32
	if(sig==0){
		sig = SIGKILL;
	}
#endif
	if (this->sig != sig) {
		changed = true;
		this->sig = sig;
	}
	pm.param = attribute["param"];
	setWorkType(attribute["type"].c_str(),changed);
}
bool KCmdPoolableRedirect::isChanged(KPoolableRedirect *rd)
{
	if (KPoolableRedirect::isChanged(rd)) {
		return true;
	}
	KCmdPoolableRedirect *c = static_cast<KCmdPoolableRedirect *>(rd);
	if (cmd!=c->cmd) {
		return true;
	}
	if (worker!=c->worker) {
		return true;
	}
	if (chuser!=c->chuser) {
		return true;
	}
	if (port!=c->port) {
		return true;
	}
	if (sig!=c->sig) {
		return true;
	}
	if (type!=c->type) {
		return true;
	}
	if (pm.param != c->pm.param) {
		return true;
	}
	return KExtendProgram::isChanged(c);
}
#endif

