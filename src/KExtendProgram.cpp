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
extern char** environ;
#endif
void addCurrentEnv(KCmdEnv* env) {
	//�����еĻ�����������
	for (int i = 0;; i++) {
		char* e = environ[i];
		if (e == NULL) {
			break;
		}
		env->addEnv(e);
	}
}
KCmdEnv* make_cmd_env(char** envs) {
	if (envs == NULL || envs[0] == NULL) {
		return NULL;
	}
	KCmdEnv* env = new KCmdEnv;
	for (int i = 0;; i++) {
		if (envs[i] == NULL) {
			break;
		}
		env->addEnv(envs[i]);
	}
	addCurrentEnv(env);
	env->addEnvEnd();
	return env;
}
KCmdEnv* make_cmd_env(std::map<KString, KString>& attribute, KExtendProgramString* ds) {
	if (attribute.size() == 0) {
		return NULL;
	}
	KCmdEnv* env = new KCmdEnv;
	for (auto it = attribute.begin();
		it != attribute.end();
		it++) {
		if (ds) {
			env->addEnv((*it).first.c_str(), ds->parseString((*it).second.c_str()).get());
		} else {
			env->addEnv((*it).first.c_str(), (*it).second.c_str());
		}
	}
	addCurrentEnv(env);
	env->addEnvEnd();
	return env;
}
u_short currend_extend_id = 1;
Token_t KExtendProgramString::getToken(bool& result) {
#ifdef ENABLE_VH_RUN_AS
	if (vh) {
		return vh->createToken(result);
	}
#endif
	result = false;
	return NULL;
}
const char* KExtendProgramString::interGetValue(const char* name) {
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
		s.clear();
		s << vh->id[0];
		return s.c_str();
	}
	if (strcasecmp(name, "gid") == 0) {
		s.clear();
		s << vh->id[1];
		return s.c_str();
	}
#else
	if (strcasecmp(name, "user") == 0) {
		return vh->user.c_str();
	}
	if (strcasecmp(name, "password") == 0) {
		return vh->group.c_str();
	}
#endif
#endif
	if (strcasecmp(name, "time") == 0) {
		s.clear();
		s << (INT64)time(NULL);
		return s.c_str();
	}
	if (strcasecmp(name, "rand") == 0) {
		s.clear();
		s << rand();
		return s.c_str();
	}
	if (strcasecmp(name, "pid") == 0) {
		s.clear();
		s << pid;
		return s.c_str();
	}
	if (vh->getEnvValue(name, vh_value)) {
		return vh_value.c_str();
	}
	if (conf.gvm->vhs.getEnvValue(name, vh_value)) {
		return vh_value.c_str();
	}
	//{{ent
	char* tbuf = strdup(name);
	char* p = strchr(tbuf, '=');
	if (p) {
		*p = '\0';
		const char* split_char = p + 1;
		s.clear();
		std::list<KSubVirtualHost*>::iterator it;
		for (it = vh->hosts.begin(); it != vh->hosts.end(); it++) {
			if (s.size() > 0) {
				s << split_char;
			}
			if (strcasecmp(tbuf, "host_dir") == 0) {
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
		if (strcasecmp(tbuf, "http_error") == 0) {
			vh->getErrorEnv(split_char, s);
		} else if (strcasecmp(tbuf, "index_file") == 0) {
			vh->getIndexFileEnv(split_char, s);
		}
		free(tbuf);
		return s.c_str();
	}
	free(tbuf);
	//}}
	return KDynamicString::getValue(name);
}
bool KExtendProgramConfig::handle(KExtendProgramString* ds) {
	KFileName file;
	kgl_auto_cstr dst_file_name;
	kgl_auto_cstr src_buf;
	kgl_auto_cstr dst_buf;
	KFile src_fp;
	KFile dst_fp;
	kgl_auto_cstr dir;
	int len;
	KStringBuf dst_tmp;
	bool result = false;
	auto src_file_name = ds->parseString(src.c_str());
	if (src_file_name == NULL) {
		klog(KLOG_ERR, "cann't parse src file name [%s]\n", src.c_str());
		return false;
	}
	if (!file.setName(src_file_name.get())) {
		klog(KLOG_ERR, "cann't get src_file[%s] infomation\n", src_file_name.get());
		goto done;
	}
	ds->addEnv("src_file", src_file_name.get());
	dir = getPath(src_file_name.get());
	if (dir) {
		ds->addEnv("src_dir", dir.get());
	}
	if (file.get_file_size() > 4194304) {
		klog(KLOG_ERR, "file [%s] is too big(size=%d) max is 4M\n", dst.c_str());
		goto done;
	}
	dst_file_name = ds->parseString(dst.c_str());
	if (dst_file_name == NULL) {
		klog(KLOG_ERR, "cann't parse dst file name[%s]\n", dst.c_str());
		goto done;
	}
	dst_tmp << dst_file_name.get() << "." << sequence.getNext();
	if (!force) {
		KFileName dstFile;
		if (dstFile.setName(dst_file_name.get())) {
			//dst file is exsit.
			result = true;
			goto done;
		}
	}
	ds->addEnv("dst_file", dst_file_name.get());
	dir = getPath(dst_file_name.get());
	if (dir) {
		ds->addEnv("dst_dir", dir.get());
	}
	if (!src_fp.open(src_file_name.get(), fileRead)) {
		klog(KLOG_ERR, "cann't open src file [%s] for read\n", file.getName());
		goto done;
	}
	src_buf = kgl_auto_cstr((char*)xmalloc((int)file.get_file_size() + 1));
	len = src_fp.read(src_buf.get(), (int)file.get_file_size());
	if (len != file.get_file_size()) {
		klog(KLOG_ERR, "cann't read complete src file [%s] readed=[%d],fileSize=[%d]\n", file.getName(), len, file.get_file_size());
		goto done;
	}
	if (!dst_fp.open(dst_tmp.c_str(), fileWrite)) {
		klog(KLOG_ERR, "cann't open dst file [%s] for write\n", dst_file_name.get());
		goto done;
	}
	if (only_copy) {
		if (file.get_file_size() != dst_fp.write(src_buf.get(), (int)file.get_file_size())) {
			klog(KLOG_ERR, "cann't write to tmp file .\n");
			goto done;
		}
	} else {
		src_buf.get()[file.get_file_size()] = '\0';
		dst_buf = ds->parseString(src_buf.get());
		if (dst_buf == NULL) {
			klog(KLOG_ERR, "cann't parse src file content [%s]\n", file.getName());
			goto done;
		}
		len = (int)strlen(dst_buf.get());
		if (len != dst_fp.write(dst_buf.get(), len)) {
			klog(KLOG_ERR, "cann't complete write to tmp file .\n");
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
				if (token_result && token) {
					chown(dst_tmp.c_str(), token[0], token[1]);
				}
			}
			result = (0 == rename(dst_tmp.c_str(), dst_file_name.get()));
#else
			result = (TRUE == MoveFileEx(dst_tmp.c_str(), dst_file_name.get(), MOVEFILE_REPLACE_EXISTING));
#endif

		}
		if (!result) {
			klog(KLOG_NOTICE, "now remove tmp file [%s]\n", dst_tmp.c_str());
			//���û�гɹ�����ɾ����ʱ�ļ�			
			unlink(dst_tmp.c_str());
		}
	}


	return result;
}
bool KExtendProgramUnlink::handle(KExtendProgramString* ds) {
	auto buf = ds->parseString(file.c_str());
	if (buf == NULL) {
		return false;
	}
	return 0 == unlink(buf.get());
}
bool KExtendProgramCmd::handle(KExtendProgramString* ds) {
	KPipeStream* st = NULL;
	std::vector<KString> args;
	explode(cmd.c_str(), ' ', args);
	char** arg = new char* [args.size() + 1];
	size_t i = 0;
	for (; i < args.size(); i++) {
		arg[i] = ds->parseString(args[i].c_str()).release();
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
		//�ȴ��ӽ��̽���
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
KExtendProgramEvent* KExtendProgramEvent::buildEvent(const KXmlAttribute& attribute) {
	KExtendProgramEvent* epe = NULL;
	if (attribute["unlink"].size() > 0) {
		KExtendProgramUnlink* ep = new KExtendProgramUnlink;
		ep->file = attribute["unlink"];
		epe = ep;
	} else if (attribute["cmd"].size() > 0) {
		KExtendProgramCmd* ep = new KExtendProgramCmd;
		ep->cmd = attribute["cmd"];
		ep->run_as_user = (attribute["run_as"] == "user");
		epe = ep;
	} else {
		auto src_file = attribute["src_file"];
		auto dst_file = attribute["dst_file"];
		if (src_file.size() > 0 && dst_file.size() > 0) {
			KExtendProgramConfig* ep = new KExtendProgramConfig;
			ep->src = src_file;
			ep->dst = dst_file;
			ep->run_as_user = (attribute["run_as"] == "user");
			if (attribute["force"] == "0") {
				ep->force = false;
			}
			ep->only_copy = (attribute["only_copy"] == "1");
			epe = ep;
		}
	}
	return epe;
}
void KExtendProgram::clean_event() {
	for (auto it = preEvents.begin(); it != preEvents.end(); it++) {
		delete (*it);
	}
	preEvents.clear();

	for (auto it = postEvents.begin(); it != postEvents.end(); it++) {
		delete (*it);
	}
	postEvents.clear();
}
KExtendProgram::~KExtendProgram() {
	clean_event();
}
KString KExtendProgram::getEnv() {
	KStringBuf s;
	for (auto it = envs.begin(); it != envs.end(); it++) {
		if (it != envs.begin()) {
			s << " ";
		}
		s << (*it).first << "=\"" << (*it).second << "\"";
	}
	return s.str();
}
bool KExtendProgram::addEvent(bool preEvent,const KXmlAttribute &attribute) {
	KExtendProgramEvent* epe = KExtendProgramEvent::buildEvent(attribute);
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
bool KExtendProgram::preLoad(KExtendProgramString* ds) {
#ifdef _WIN32
	RevertToSelf();
#endif
	std::list<KExtendProgramEvent*>::iterator it;
	for (it = preEvents.begin(); it != preEvents.end(); it++) {
		(*it)->handle(ds);
	}
	return true;
}
bool KExtendProgram::postLoad(KExtendProgramString* ds) {
	std::list<KExtendProgramEvent*>::iterator it;
	for (it = postEvents.begin(); it != postEvents.end(); it++) {
		(*it)->handle(ds);
	}
	return true;
}
bool KExtendProgram::parse_config(const khttpd::KXmlNode* xml) {
	auto& attr = xml->attributes();
	life_time = attr.get_int("life_time");
	idleTime = attr.get_int("idle_time");
	maxRequest = attr.get_int("max_request");
	//maxConnect = attr.get_int("max_connect");
	max_error_count = attr.get_int("max_error_count");
	auto envs_node = kconfig::find_child(xml->get_first(), _KS("env"));
	if (envs_node != nullptr) {
		parseEnv(envs_node->attributes());
	} else {
		envs.clear();
	}
	clean_event();
	auto events = kconfig::find_child(xml->get_first(), _KS("pre_event"));
	if (events) {
		for (uint32_t index = 0;; ++index) {
			auto body = events->get_body(index);
			if (!body) {
				break;
			}
			addEvent(true, body->attributes);
		}
	}
	events = kconfig::find_child(xml->get_first(), _KS("post_event"));
	if (events) {
		for (uint32_t index = 0;; ++index) {
			auto body = events->get_body(index);
			if (!body) {
				break;
			}
			addEvent(false, body->attributes);
		}
	}
	return true;
}
bool KExtendProgram::parseEnv(const KXmlAttribute& attribute) {
	envs.clear();
	for (auto it = attribute.begin(); it != attribute.end(); ++it) {
		envs[(*it).first] = (*it).second;
	}
	return true;
}
KCmdEnv* KExtendProgram::makeEnv(KExtendProgramString* ds) {
	return make_cmd_env(envs, ds);
}
bool KExtendProgram::isChanged(KExtendProgram* ep) {
	return false;
}
