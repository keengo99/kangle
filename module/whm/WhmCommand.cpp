/*
 * WhmCommand.cpp
 *
 *  Created on: 2010-8-31
 *      Author: keengo
 */

#include "WhmCommand.h"
#include "KVirtualHost.h"
#include "KXml.h"

using namespace std;
WhmCommand::WhmCommand(std::string &file) {	
	explode(file.c_str(),' ',args);
	this->file = args[0];
#ifdef _WIN32
	char *buf = strdup(this->file.c_str());
	char *e = buf + strlen(buf) - 1;
	while(e>buf){
		if( *e=='/' || *e=='\\'){		
			break;
		}
		e--;
	}
	char *p = strchr(e,'.');
	if(p==NULL){
		this->file = this->file + ".exe";
		args[0] = this->file;
	}
	free(buf);
#endif
}

WhmCommand::~WhmCommand() {
}
bool WhmCommand::init(std::string &whmFile)
{
	bool result = WhmExtend::init(whmFile);
	args[0] = file;
	return result;
}
int WhmCommand::call(const char *callName, const char *eventType,
		WhmContext *context) {
	int ret = WHM_OK;
	KWStream *out = context->getOutputStream();
	Token_t token = NULL;
	if(runAsUser){
		bool result = false;
		token = context->getToken(result);
		if (!result) {
			klog(KLOG_ERR,"cann't get vh token\n");
			return WHM_CALL_FAILED;
		}
	}
	int len = this->args.size();
	char **arg = new char *[len+1];
	arg[0] = (char *)file.c_str();
	for(int i=1;i<len;i++){		
		arg[i] = context->parseString(args[i].c_str());
	}
	arg[len] = NULL;
	KPipeStream *st = createProcess(token, arg, NULL, true);
	for(int i=1;i<len;i++){
		free(arg[i]);
	}
	delete[] arg;
	if (token) {
		KVirtualHost::closeToken(token);
	}
	if (st == NULL) {
		ret = WHM_COMMAND_FAILED;
		*out << "<result status=\"" << ret << "\"/>";
	} else {
		*out << "<raw_result extend='" << name.c_str() << "'>\n";
		*out << CDATA_START;
		char buf[512];
		for (;;) {
			int len = st->read(buf, sizeof(buf));
			if (len <= 0) {
				break;
			}
			if (out->write_all(buf, len) != STREAM_WRITE_SUCCESS) {
				continue;
				//break;
			}
		}
		*out << CDATA_END;
		*out << "</raw_result>\n";
	}
	delete st;
	return ret;
}
