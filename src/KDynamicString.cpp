#include <string.h>
#include <map>
#include "utils.h"
#include "KDynamicString.h"
#include "kmalloc.h"
const char *getSystemEnv(const char *name) {
	if (strncasecmp(name,"env:",4)==0) {
		const char *value = getenv(name+4);
		if (value==NULL) {
			return "";
		}
		return value;
	}
	if (strcasecmp(name, "kangle_home") == 0) {
		return conf.path.c_str();
	}
	if (strcasecmp(name, "kangle_version") == 0) {
		return VERSION;
	}
	if (strcasecmp(name, "kangle_type") == 0) {
		return getServerType();
	}
	if (strcasecmp(name, "classpath_split_char") == 0) {
#ifdef _WIN32
		return ";";
#else
		return ":";
#endif
	}
	if (strcasecmp(name,"exe")==0) {
#ifdef _WIN32
		return ".exe";
#else
		return "";
#endif
	}
	if (strcasecmp(name, "dso") == 0) {
#ifdef _WIN32
		return "dll";
#else
		return "so";
#endif
	}
	return NULL;
}
KDynamicString::KDynamicString() {
	buf = hot = NULL;
	curBlock = NULL;
	dst = NULL;
	dimModel = true;
	blockModel = true;
	strictModel = true;
	envChar = '$';
}
KDynamicString::~KDynamicString() {
	if (buf) {
		xfree(buf);
	}
	if (dst) {
		delete dst;
	}
	clean();
}
void KDynamicString::clean() {
	if (curBlock) {
		delete curBlock;
		curBlock = NULL;
	}
	std::list<KControlCodeBlock *>::iterator it;
	for (it = blockStack.begin(); it != blockStack.end(); it++) {
		delete (*it);
	}
	blockStack.clear();
}
bool KDynamicString::controlString(const char control_char) {
	char *env = hot + 1;
	if (hot[1] == '{') {
		env++;
		char *end = strchr(env, '}');
		if (end == NULL) {
			//printf("cann't find end char `}`");
			//return ;//s.stealString();
			env = NULL;
		} else {
			*end = '\0';
			hot = end + 1;
		}
	} else if (!strictModel && control_char == envChar) {
		char *end = env;
		while (*end) {
			if ((*env >= 'a' && *env <= 'z') || (*env >= 'A' && *env <= 'Z')
					|| (*env >= '0' && *env <= '9') || *env == '_') {
				env++;
				continue;
			}
			*end = '\0';
			end++;
			break;
		}
		hot = end;
	} else {
		*dst << control_char;
		hot++;
		return true;
	}
	const char *val = NULL;
	if (control_char == envChar) {
		if (!buildValue(env,dst)) {
			val = getSystemEnv(env);
			if (val == NULL) {
				val = getValue(env);
			}
		}
	} else if (control_char == '@') {
		char *p = strchr(env, '[');
		if (p) {
			*p = '\0';
			p++;
			char *q = strchr(p, ']');
			if (q) {
				*q = '\0';
			}
			int index = 0;
			if (isdigit((int) *p)) {
				index = atoi(p);
			} else {
				index = getControlIndex(p);
			}
			val = getDimValue(env, index);
		}
	} else if (control_char == '#') {
		if (*env == '/' && curBlock) {
			//it is a block end;
			return false;
		} else {
			controlCode(env);
		}
	}
	if (val) {
		*dst << val;
	}
	return true;
}
void KDynamicString::parseString() {
	if (hot == NULL) {
		return;
	}
	bool slash = false;
	while (*hot) {
		if (slash) {
			if (*hot != envChar && *hot != '@' && *hot != '#' && *hot != '\\') {
				*dst << "\\";
			}
			*dst << *hot;
			slash = false;
			hot++;
		} else {
			if (*hot == '\\') {
				slash = true;
				hot++;
				continue;
			}
			if (!isControlChar(*hot)) {
				*dst << *hot;
				hot++;
				continue;
			}
			if (!controlString(*hot)) {
				return;
			}
		}
	}

}
void KDynamicString::controlCode(char *code) {
	while (*code && isspace((unsigned char) *code)) {
		code++;
	}
	char *p = code;
	while (*p && !isspace((unsigned char) *p)) {
		p++;
	}
	if (*p == '\0') {
		return;
	}
	*p = '\0';
	p++;
	if (curBlock) {
		blockStack.push_back(curBlock);
	}
	curBlock = new KControlCodeBlock;
	std::map<char *, char *, lessp_icase> attribute;
	buildAttribute(p, attribute);
	if (strcasecmp(code, "foreach") == 0) {
		curBlock->index = 0;
		char *key = attribute[(char *) "key"];
		char *dim = attribute[(char *) "dim"];
		if (key && dim) {
			curBlock->key = key;
			curBlock->count = getDimSize(dim);
			char *orig_hot = xstrdup(hot);
			int orig_len = strlen(orig_hot);
			char *saved_hot = hot;
			for (curBlock->index = 0; curBlock->index < curBlock->count; curBlock->index++) {
				if (curBlock->index > 0) {
					kgl_memcpy(saved_hot, orig_hot, orig_len);
					hot = saved_hot;
				}
				parseString();
			}
			xfree(orig_hot);
		}
	}
	assert(curBlock);
	delete curBlock;
	curBlock = NULL;
	if (blockStack.size() > 0) {
		curBlock = *(blockStack.begin());
		blockStack.pop_front();
	}
}
char *KDynamicString::parseDirect(char *str) {
	if (buf) {
		xfree(buf);
	}
	if (dst) {
		delete dst;
	}
	dst = new KStringBuf(2 * strlen(str));
	clean();
	buf = str;
	hot = buf;
	parseString();
	char *result = dst->stealString();
	buf = NULL;
	return result;
}
char *KDynamicString::parseString(const char *str) {
	if (str == NULL) {
		return NULL;
	}
	if (buf) {
		xfree(buf);
	}
	if (dst) {
		delete dst;
	}
	dst = new KStringBuf(2 * strlen(str));
	//dst.clean();
	//dst.init(2048);
	clean();
	buf = xstrdup(str);
	hot = buf;
	parseString();
	return dst->stealString();
}
int KDynamicString::getControlIndex(const char *value) {
	int index;
	if (curBlock) {
		index = curBlock->getBlockIndex(value);
		if (index >= 0) {
			return index;
		}
		std::list<KControlCodeBlock *>::iterator it;
		for (it = blockStack.begin(); it != blockStack.end(); it++) {
			index = (*it)->getBlockIndex(value);
			if (index >= 0) {
				return index;
			}
		}
	}
	return -1;
}
