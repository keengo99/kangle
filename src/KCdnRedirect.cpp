#include "KCdnRedirect.h"
#include "KCdnContainer.h"
#include "KVirtualHost.h"
#include "KExtendProgram.h"
/*
KFetchObject *KCdnRedirect::makeFetchObject(KHttpRequest *rq, KFileName *file)
{
	if(rq->svh==NULL || rq->svh->vh==NULL){
		return NULL;
	}
	KExtendProgramString ds(NULL, rq->svh->vh);
	char *host = ds.parseString(name.c_str());
	if(host==NULL){
		return NULL;
	}
	char *port = strchr(host,':');
	if (port) {
		*port = '\0';
		port++;
	}
	KFetchObject *fo = cdnContainer.get(NULL,host,(port?atoi(port):80),NULL,0);
	xfree(host);
	return fo;
}
*/