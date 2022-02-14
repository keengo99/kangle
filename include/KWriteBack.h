#ifndef WRITE_BACK_H_DF7S77SJJJJJssJJJJJJJ
#define WRITE_BACK_H_DF7S77SJJJJJssJJJJJJJ
#include <vector>
#include <string.h>
#include <stdlib.h>
#include<list>
#include<string>
#include "global.h"
#include "KMutex.h"
#include "KJump.h"
#include "KXml.h"
#include "kserver.h"
#include "ksapi.h"
#include "KStringBuf.h"
#include "KHttpHeader.h"
#include "KHttpBufferParser.h"
#include "KConfig.h"
#ifdef ENABLE_WRITE_BACK
class KHttpRequest;
class KWriteBack:public KJump
{
public:
	KWriteBack()
	{
		ext = cur_config_ext;
		header = NULL;
	}
	void buildRequest(KHttpRequest *rq);
	std::string getMsg();
	void setMsg(std::string msg);
	void buildXML(std::stringstream &s) {
		s << "\t<writeback name='" << name << "'>";
		s << CDATA_START << KXml::encode(getMsg()) << CDATA_END << "</writeback>\n";
	}
	bool ext;
	bool keep_alive;
	int status_code;
	KHttpHeader *header;
	KStringBuf body;
};
#endif
#endif
