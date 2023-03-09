#ifndef KREWRITEMARKEX_H
#define KREWRITEMARKEX_H
#include <list>
#include "KMark.h"
#include "KReg.h"
#include "KStringBuf.h"
/*
 条件测试基类
 */
class KRewriteCondTestor {
public:
	virtual ~KRewriteCondTestor() {
	}
	/*
	 test a string the lastSubString must be delete if exsit when reset it;
	 */
	virtual bool test(const char *str, KRegSubString **lastSubString) = 0;
	virtual std::string getString() = 0;
	virtual bool parse(const char *str, bool nc) = 0;
};
/*
 文件属性测试
 */
class KFileAttributeTestor: public KRewriteCondTestor {
public:
	KFileAttributeTestor() {

	}
	bool test(const char *str, KRegSubString **lastSubString) override ;
	std::string getString() override {
		std::stringstream s;
		s << "-" << type;
		return s.str();
	}
	bool parse(const char *str, bool nc) override {
		type = str[1];
		return true;
	}
private:
	char type;
};
/*
 正则表达式测试
 */
class KRegexTestor: public KRewriteCondTestor {
public:
	std::string getString() override {
		return reg.getModel();
	}
	bool parse(const char *str, bool nc) override {
		return reg.setModel(str, (nc ? PCRE_CASELESS : 0));
	}
	bool test(const char *str, KRegSubString **lastSubString) override {
		if (*lastSubString) {
			delete *lastSubString;
		}
		*lastSubString = reg.matchSubString(str, (int)strlen(str), 0);
		return *lastSubString != NULL;
	}
private:
	KReg reg;
};
/*
 字符串比较测试
 */
class KCompareTestor: public KRewriteCondTestor {
public:
	KCompareTestor() {
		str = NULL;
	}
	~KCompareTestor() {
		if (str) {
			xfree(str);
		}
	}
	bool test(const char *buf, KRegSubString **lastSubString) override {
		int r;
		if(str==NULL){
			return false;
		}
		if (nc) {
			r = strcasecmp(str+1, buf );
		} else {
			r = strcmp(str+1, buf);
		}
		switch (*str) {
		case '>':
			return r > 0;
		case '=':
			return r == 0;
		case '<':
			return r < 0;
		}
		return false;
	}
	std::string getString() override {
		return str;
	}
	bool parse(const char *buf, bool nc) override {
		if (*buf == '\0') {
			return false;
		}
		this->nc = nc;
		str = xstrdup(buf);
		return true;
	}
private:
	char *str;
	bool nc;
};
class KRewriteCond {
public:
	KRewriteCond() {
		str = NULL;
		testor = NULL;
		revert = false;
		is_or = false;
	}
	~KRewriteCond() {
		if (str) {
			xfree(str);
		}
		if (testor) {
			delete testor;
		}
	}
	bool parse_config(const khttpd::KXmlNodeBody* body) {
		str = xstrdup(body->attributes["str"].c_str());		
		is_or = body->attributes["or"] == "1";		
		nc = body->attributes["nc"] == "1";
		
		const char* test = body->get_text("");
		if (*test == '!') {
			revert = true;
			test++;
		}
		if (*test == '-') {
			//it is file attribute
			testor = new KFileAttributeTestor;
		} else if (*test == '>' || *test == '<' || *test == '=') {
			//compare string
			testor = new KCompareTestor;
		} else {
			testor = new KRegexTestor;
			//reg
		}
		return testor->parse(test, nc);
	}
	bool nc;
	bool revert;
	//true is or false is and with prev cond;
	//the first cond or must be false
	bool is_or;
	//cond origin str
	char *str;
	KRewriteCondTestor *testor;
};
class KRewriteRule {
public:
	KRewriteRule();
	~KRewriteRule();
	bool mark(KHttpRequest *rq, KHttpObject *obj, std::list<KRewriteCond *> *conds, const std::string &prefix, const char* rewriteBase, KFetchObject** fo);
	bool parse(const KXmlAttribute &attribute);
	void buildXml(std::stringstream &s);
	bool revert;
	KReg reg;
	char *dst;
	bool internal;
	bool nc;
	bool qsa;
	char *proxy;
	int code;
};
class KRewriteMarkEx: public KMark {
public:
	KRewriteMarkEx(void);
	~KRewriteMarkEx(void);
	bool mark(KHttpRequest *rq, KHttpObject *obj, KFetchObject** fo) override;
	KMark * new_instance() override;
	const char *getName() override;
	std::string getHtml(KModel *model) override;

	std::string getDisplay() override;
	static void getEnv(KHttpRequest *rq, char *env, KStringBuf &s);
	void parse_config(const khttpd::KXmlNodeBody* body) override;
	void parse_child(const kconfig::KXmlChanged* changed) override;
	static KStringBuf *getString(const char *prefix, const char *str,
			KHttpRequest *rq, KRegSubString *s1, KRegSubString *s2);
	static void getString(const char *prefix, const char *str,
			KHttpRequest *rq, KRegSubString *s1, KRegSubString *s2,KStringBuf *s);
private:
	std::list<KRewriteCond *> conds;
	std::list<KRewriteRule *> rules;
	std::string prefix;
	std::string rewriteBase;
	int flag;
};
#endif
