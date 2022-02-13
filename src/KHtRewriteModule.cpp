/*
 * KHtRewriteModule.cpp
 *
 *  Created on: 2010-9-27
 *      Author: keengo
 */
#include <stdio.h>
#include <string.h>
#include "KHtAccess.h"
#include "KHtRewriteModule.h"
#include "KXml.h"
#include "kmalloc.h"
void parseRewriteFlag(char *flag, rewrite_flag_t &rf) {
	flag = strchr(flag, '[');
	if (flag == NULL) {
		return;
	}
	flag++;
	char *p = strchr(flag, ']');
	if (p != NULL) {
		*p = '\0';
	}
	for (;;) {
		char *hot = strchr(flag, ',');
		if (hot) {
			*hot = '\0';
		}
		while (*flag && isspace((unsigned char) (*flag))) {
			flag++;
		}
		p = flag;
		for (;;) {
			if (*p == '\0') {
				break;
			}
			if (isspace((unsigned char) *p)) {
				*p = '\0';
				break;
			}
			p++;
		}
		if (strcasecmp(flag, "C") == 0 || strcasecmp(flag, "chain") == 0) {
			SET(rf.flag,REWRITE_CHAIN_AND);
		} else if (strcasecmp(flag, "F") == 0 || strcasecmp(flag, "forbidden")
				== 0) {
			SET(rf.flag,REWRITE_FOBIDDEN);
		} else if (strcasecmp(flag, "G") == 0 || strcasecmp(flag, "gone") == 0) {
			SET(rf.flag,REWRITE_GONE);
		} else if (strcasecmp(flag, "L") == 0 || strcasecmp(flag, "last") == 0) {
			SET(rf.flag,REWRITE_LAST);
		} else if (strcasecmp(flag, "N") == 0 || strcasecmp(flag, "next") == 0) {
			SET(rf.flag,REWRITE_NEXT);
		} else if (strcasecmp(flag, "NC") == 0 || strcasecmp(flag, "nocase")
				== 0) {
			SET(rf.flag,REWRITE_NOCASE);
		} else if (strcasecmp(flag,"redirect")==0 || strcasecmp(flag,"R")==0){
			SET(rf.flag,REWRITE_REDIRECT);
			rf.code = 0;
		} else if (strncasecmp(flag, "redirect=", 9) == 0) {
			SET(rf.flag,REWRITE_REDIRECT);
			rf.code = atoi(flag + 9);
		} else if (strncasecmp(flag, "R=", 2) == 0) {
			SET(rf.flag,REWRITE_REDIRECT);
			rf.code = atoi(flag + 2);
		} else if (strncasecmp(flag, "skip=", 5) == 0) {
			SET(rf.flag,REWRITE_SKIP);
			rf.skip = atoi(flag + 5);
		} else if (strncasecmp(flag, "S=", 2) == 0) {
			SET(rf.flag,REWRITE_SKIP);
			rf.skip = atoi(flag + 2);
		} else if (strcasecmp(flag, "OR") == 0 || strcasecmp(flag, "ornext")
				== 0) {
			SET(rf.flag,REWRITE_OR);
		} else if (strcasecmp(flag,"QSA")==0 || strcasecmp(flag,"qsappend")==0) {
			SET(rf.flag,REWRITE_QSA);
		} else if (strcasecmp(flag,"P")==0 || strcasecmp(flag,"proxy")==0) {
			SET(rf.flag,REWRITE_PROXY);
		}
		if (hot == NULL) {
			break;
		}
		flag = hot + 1;
	}
}
KHtRewriteModule::KHtRewriteModule() {
	enable = true;
	cur_chainid = 0;
	last_chainand = false;
	chainStarted = false;
	lastCondOr = false;
}

KHtRewriteModule::~KHtRewriteModule() {
}
bool KHtRewriteModule::process(KApacheConfig *htaccess, const char *cmd,
		std::vector<char *> &item) {
	if (strcasecmp(cmd, "RewriteEngine") == 0) {
		if (item.size() <= 0) {
			return true;
		}
		if (strcasecmp(item[0], "OFF") == 0) {
			enable = false;
		}
		if (strcasecmp(item[0], "ON") == 0) {
			enable = true;
		}
		return true;
	}
	if (!enable) {
		return false;
	}
	if (strcasecmp(cmd,"RewriteBase") == 0) {
		if (item.size() <= 0) {
			return true;
		}
		rewriteBase = item[0];
		return true;
	}
	if (strcasecmp(cmd, "RewriteCond") == 0) {
		if (item.size() < 2) {
			return true;
		}
		rewrite_flag_t rf;
		memset(&rf, 0, sizeof(rf));
		if (item.size() > 2) {
			parseRewriteFlag(item[2], rf);
		}
		chainStart(htaccess);
		rule << "<cond str='" << item[0] << "'";
		if(lastCondOr){
			rule << " or='1'";
		}
		if (TEST(rf.flag,REWRITE_OR)) {
			lastCondOr = true;
		}else{
			lastCondOr = false;
		}

		rule << " nc='";
		if (TEST(rf.flag,REWRITE_NOCASE)) {
			rule << "1";
		} else {
			rule << "0";
		}
		rule << "'";
		//if (TEST(rf.flag,REWRITE_QSA)) {
		//	rule << " qsa='1'";
		//}
		rule << ">" << CDATA_START << item[1] << CDATA_END << "</cond>\n";
		return true;
	}
	if (strcasecmp(cmd, "RewriteRule") == 0) {
		if (item.size() < 2) {
			return true;
		}
		rewrite_flag_t rf;
		memset(&rf, 0, sizeof(rf));
		if (item.size() > 2) {
			parseRewriteFlag(item[2], rf);
		}
		if (action.str().size() == 0) {
			//chain << "<chain action='";
			if (TEST(rf.flag,REWRITE_LAST)) {
				action << "default";
			} else if (TEST(rf.flag,REWRITE_NEXT)) {
				action << "table:BEGIN";
			} else if (TEST(rf.flag,REWRITE_SKIP)) {
				action << "tablechain:BEGIN:" << cur_chainid + rf.skip;
			} else if (TEST(rf.flag,REWRITE_FOBIDDEN|REWRITE_GONE)) {
				action << "deny";
			} else if (TEST(rf.flag,REWRITE_REDIRECT)) {
				action << "default";
			}
		}
		//			chain << "'>\n";
		chainStart(htaccess);
		rule << "<rule path='" << item[0] << "' dst='" << item[1]
				<< "' internal='";///>\n";
		//internal='";
		if (TEST(rf.flag,REWRITE_REDIRECT)) {
			rule << "0";
			if (rf.code>0) {
				rule << "' code='" << rf.code;
			}
		} else {
			rule << "1";
		}
		rule << "' nc='";
		if (TEST(rf.flag,REWRITE_NOCASE)) {
			rule << "1";
		} else {
			rule << "0";
		}
		rule << "'";
		if (TEST(rf.flag,REWRITE_PROXY)) {
			rule << " proxy='-'";
		}
		if (TEST(rf.flag,REWRITE_QSA)) {
			rule << " qsa='1'";
		}
		rule << "/>\n";
		if (!TEST(rf.flag,REWRITE_CHAIN_AND)) {
			//last_chainand = true;
			chainEnd();
			//last_chainand = false;
		}
		return true;
	}
	if (strcasecmp(cmd,"Redirect")==0) {
		return true;
	}
	return false;
}
bool KHtRewriteModule::getXml(std::stringstream &s)
{
	chainEnd();
	if(chain.str().size()==0){
		return false;
	}
	s << "<request action='allow'>\n";
	s << "<table name='" << BEGIN_TABLE << "'>\n";
	s << chain.str();
	s << "</table>\n</request>\n";
	return true;
}
void KHtRewriteModule::chainStart(KApacheConfig *htaccess) {
	if (chainStarted) {
		return;
	}
	chainStarted = true;
	lastCondOr = false;
	rule << "<mark_rewritex prefix='" << htaccess->prefix << "/' rewrite_base='" << rewriteBase << "'>\n";
}
void KHtRewriteModule::chainEnd() {
	if (!chainStarted) {
		return;
	}
	chainStarted = false;
	if (action.str().size() == 0) {
		action << "continue";
	}
	chain << "<chain name='" << cur_chainid++ << "' action='" << action.str()
			<< "'>\n";
	chain << rule.str();
	chain << "</mark_rewritex></chain>";
	action.str("");
	rule.str("");
}
