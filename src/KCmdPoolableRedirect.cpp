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
KCmdPoolableRedirect::KCmdPoolableRedirect(const KString&name) : KPoolableRedirect(name) {
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
bool KCmdPoolableRedirect::parse_config(const khttpd::KXmlNode* node) {
	auto& attr = node->attributes();
	auto proto = KPoolableRedirect::parseProto(attr("proto"));
	if (this->proto != proto) {
		this->proto = proto;
	}
	set_proto(proto);
	cmd = attr["file"];
	worker = atoi(attr("worker"));	
	this->chuser = (attr["chuser"] != "0");
	this->port = attr.get_int("port");
	int sig = attr.get_int("sig");
#ifndef _WIN32
	if (sig == 0) {
		sig = SIGKILL;
	}
#endif
	if (this->sig != sig) {
		this->sig = sig;
	}
	pm.param = attr["param"];
	setWorkType(attr("type"), true);
	return KExtendProgram::parse_config(node);
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
			return false;
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
	KString program;
	for (unsigned j = 0; j < args.size(); j++) {
		const char* tmp_arg = args[j];
		if (j == 0) {
			//cmd
			if (!isAbsolutePath(tmp_arg)) {
				program = conf.path + tmp_arg;
				tmp_arg = program.c_str();
			}
		}
		auto a = ds.parseString(tmp_arg);
		if (a == NULL) {
			continue;
		}
		if (*a == '\0') {
			continue;
		}
		arg[i] = a.release();
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
	KStringBuf s;
	s << "cmd:" << name;
	pm.setName(s.str().c_str());
	return true;
}
bool KCmdPoolableRedirect::parseEnv(const KXmlAttribute &attribute)
{
	if (KExtendProgram::parseEnv(attribute)) {
		pm.clean();
		return true;
	}
	return false;
}
void KCmdPoolableRedirect::set_proto(Proto_t proto)
{
	this->proto = proto;
	pm.set_proto(proto);
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

