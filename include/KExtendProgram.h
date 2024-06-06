#ifndef KEXTENDPROGRAM_H
#define KEXTENDPROGRAM_H
#include <string.h>
#include "KVirtualHost.h"
#include "KDynamicString.h"
#include "KVirtualHostManage.h"

#include "KFileName.h"

#define WORK_TYPE_SP   0
#define WORK_TYPE_MP    1

#define WORK_TYPE_MT    3

class KExtendProgramString: public KDynamicString {
public:
	KExtendProgramString(const char *extendName, KVirtualHost *vh) {
		this->vh = vh;
		pid = 0;
#ifdef ENABLE_CMD_DPORT
		if (extendName) {
			s << extendName << "_port";
			listenName = s.c_str();
		}
		listenPort = 0;
#endif
	}
	void setPid(int pid) {
		this->pid = pid;
	}
	//{{ent
#ifdef ENABLE_CMD_DPORT
	int getListenPort() {
		if (listenPort == 0) {
			const char *value = interGetValue(listenName.c_str());
			if (value) {
				listenPort = atoi(value);
			}
		}
		return listenPort;
	}
	const char *getListenPortValue() {
		s.clear();
		s << listenPort;
		return s.c_str();
	}
#endif
	//}}
	const char *getValue(const char *name) {
		return interGetValue(name);

	}
	const char *getDimValue(const char *name, int index) {
		std::list<KSubVirtualHost *>::iterator it;
		if (vh==NULL) {
			return "";
		}
		for (it = vh->hosts.begin(); it != vh->hosts.end(); it++, index--) {
			if (index == 0) {
				if (strcasecmp(name, "host") == 0) {
					if (strcmp((*it)->host, "*") == 0) {
						const char *value = getValue("default_host");
						if (value == NULL || *value == '\0') {
							return "localhost";
						} else {
							return value;
						}
					}
					return (*it)->host;
				}
				if (strcasecmp(name, "subdir") == 0) {
					if (strcmp((*it)->dir, "/") == 0) {
						return NULL;
					}
					return (*it)->dir;
				}
				if (strcasecmp(name, "full_sub_doc_root") == 0) {
					return (*it)->doc_root;
				}
				if (strcasecmp(name, "sub_doc_root") == 0) {
					char *dir = KFileName::concatDir(vh->doc_root.c_str(),
							(*it)->dir);
					if (dir) {
						vh_value = dir;
						xfree(dir);
					} else {
						vh_value = "";
					}
					return vh_value.c_str();
				}
				break;
			}
		}
		if (strcasecmp(name, "name") == 0) {
			if (index < 0 || index >= (int) vh->name.size()) {
				return NULL;
			}
			s.clear();
			char ch[2] = { vh->name[index], 0 };
			s << ch;
			return s.c_str();
		}
		if (strcasecmp(name, "doc_root") == 0) {
			if (index < 0 || index >= (int) vh->doc_root.size()) {
				return NULL;
			}
			char ch[2] = { vh->doc_root[index], 0 };
			s << ch;
			return s.c_str();
		}
		return NULL;
	}
	int getDimSize(const char *name) {
		if (vh==NULL) {
			return 0;
		}
		if (strcasecmp(name, "host") == 0 ||
			strcasecmp(name, "subdir") == 0	||
			strcasecmp(name, "sub_doc_root") == 0 || 
			strcasecmp(name, "full_sub_doc_root") == 0) {
			return (int)vh->hosts.size();
		}
		if (strcasecmp(name, "name") == 0) {
			return (int)vh->name.size();
		}
		if (strcasecmp(name, "doc_root") == 0) {
			return (int)vh->doc_root.size();
		}
		return 0;
	}
	const char *interGetValue(const char *name);	
	Token_t getToken(bool &result);
#ifdef ENABLE_CMD_DPORT
	int listenPort;
	KString listenName;
	std::map<KString,int> port;
#endif
private:
	KVirtualHost *vh;
	int pid;
	//std::stringstream s;
	KStringBuf s;
	KString vh_value;

};
class KExtendProgramEvent {
public:
	virtual bool handle(KExtendProgramString *ds) = 0;
	virtual ~KExtendProgramEvent() {
	}
	;
	static KExtendProgramEvent *buildEvent(const KXmlAttribute &attribute);
};
class KExtendProgramConfig: public KExtendProgramEvent {
public:
	KExtendProgramConfig()
	{
		force = true;
		only_copy = false;
		run_as_user = false;
	}
	bool handle(KExtendProgramString *ds);
	KString src;
	KString dst;
	bool only_copy;
	bool force;
	bool run_as_user;
};
class KExtendProgramUnlink: public KExtendProgramEvent {
public:
	bool handle(KExtendProgramString *ds);
	KString file;
};
class KExtendProgramCmd: public KExtendProgramEvent {
public:
	KExtendProgramCmd() {
		run_as_user = false;
	}
	bool handle(KExtendProgramString *ds);
	KString cmd;
	bool run_as_user;
};
#define EXTENDPROGRAM_DEFAULT_LIFETIME    0
#define EXTENDPROGRAM_DEFAULT_IDLETIME    600
#define EXTENDPROGRAM_DEFAULT_MAXREQUEST  0
//#define EXTENDPROGRAM_DEFAULT_MAXCONNECT  0
extern u_short currend_extend_id;
class KExtendProgram {
public:
	KExtendProgram() {
		life_time = EXTENDPROGRAM_DEFAULT_LIFETIME;
		idleTime = EXTENDPROGRAM_DEFAULT_IDLETIME;
		maxRequest = EXTENDPROGRAM_DEFAULT_MAXREQUEST;
		//maxConnect = EXTENDPROGRAM_DEFAULT_MAXCONNECT;
		id = ++currend_extend_id;
		max_error_count = 0;
	}
	virtual ~KExtendProgram();
	static const char *getTypeString(int type) {
		switch (type) {
		case WORK_TYPE_MT:
			return "mt";
		case WORK_TYPE_MP:
			return "mp";
		case WORK_TYPE_SP:
			return "sp";
		}
		return "sp";
	}
	static int getTypeValue(const char *str) {
		if (strcasecmp(str, "mp") == 0) {
			return WORK_TYPE_MP;
		}
		if (strcasecmp(str, "mt") == 0) {
			return WORK_TYPE_MT;
		}
		if (strcasecmp(str, "sp") == 0) {
			return WORK_TYPE_SP;
		}
		return WORK_TYPE_SP;
	}
	bool isChanged(KExtendProgram *ep);
	KString getEnv();
	virtual bool parse_config(const khttpd::KXmlNode* xml);
	virtual bool parseEnv(const KXmlAttribute&attribute);
	bool addEvent(bool preEvent, const KXmlAttribute& attribute);
	bool preLoad(KExtendProgramString *ds);
	bool postLoad(KExtendProgramString *ds);
	int type;
	int idleTime;
	unsigned maxRequest;
	//unsigned maxConnect;
	int max_error_count;
	int life_time;
	u_short id;
protected:
	std::map<KString, KString> envs;
	KCmdEnv *makeEnv(KExtendProgramString *ds);
private:
	void clean_event();
	std::list<KExtendProgramEvent *> preEvents;
	std::list<KExtendProgramEvent *> postEvents;
};
KCmdEnv *make_cmd_env(std::map<KString, KString>&attribute,KExtendProgramString *ds=NULL);
KCmdEnv *make_cmd_env(char **env);
#endif
