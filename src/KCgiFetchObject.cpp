/*
 * KCgiFetchObject.cpp
 *
 *  Created on: 2010-6-11
 *      Author: keengo
 */
#include "kgl_ssl.h"
#include <openssl/ssl.h>
#ifdef ENABLE_CGI
static int loadStreamHead(KStream *stream, KHttpRequest *rq, KHttpObject *obj,KHttpObjectParserHook &hook) {
	char *answer = (char *) xmalloc(ANSW_SIZE+1);
	int cursize = ANSW_SIZE;
	char *buf = answer;
	int r;
	int len = ANSW_SIZE;
	int head_status = LOAD_HEAD_FAILED;
	KHttpProtocolParser parser;
	if (!answer) {
		obj->data->status_code = STATUS_SERVER_ERROR;
		send_error(rq, NULL, STATUS_SERVER_ERROR, "No memory to alloc.");
		goto error;
	}
	for (;;) {
		r = stream->read(buf, len);
		if (r <= 0) {
			obj->data->status_code = STATUS_BAD_GATEWAY;
			send_error(rq, NULL, STATUS_BAD_GATEWAY,
					"Didn't recv complete http head.");
			goto error;
		}
		buf += r;
		len -= r;
		int result = parser.parse(answer, cursize - len, &hook);
		if (result == 0) {
			obj->data->status_code = STATUS_BAD_GATEWAY;
			send_error(rq, NULL, STATUS_BAD_GATEWAY, "head format is error.");
			klog(KLOG_DEBUG, "check server header failed\n");
			goto error;
		}
		if (result == 1) {
			if (obj->data->status_code == STATUS_NOT_MODIFIED) {
				//				head_status = HEAD_NOT_MODIFIED;
				KBIT_SET(obj->index.flags,FLAG_DEAD|OBJ_INDEX_UPDATE);
				break;
			}
			break;
		}
		assert(len>=0);
		if (len < 1) {
			if (cursize >= MAX_HTTP_HEAD_SIZE) {
				obj->data->status_code = STATUS_BAD_GATEWAY;
				send_error(rq, NULL, STATUS_BAD_GATEWAY,
						"Head size is too big(max is 1M).");
				goto error;
				//head size is too large
			}
			char *nb = (char *) malloc(2*cursize+1);
			kgl_memcpy(nb, answer, cursize + 1);
			free(answer);
			answer = nb;
			buf = answer + cursize;
			len += cursize;
			cursize = cursize * 2;
		}
	}
	assert(obj->data->headers==NULL);
	obj->data->headers = parser.stealHeaders(obj->data->headers);	
	if (obj->index.content_length > (INT64) conf.max_cache_size) {
		//obj is too big
		KBIT_SET(obj->index.flags, FLAG_DEAD|OBJ_INDEX_UPDATE);
	}
	if (parser.bodyLen > 0) {
		rq->buffer.write_all(parser.body, parser.bodyLen);
	}
	xfree(answer);
	return LOAD_HEAD_SUCCESS;
	error: //get head failed
	obj->index.flags |= FLAG_DEAD;
	IF_FREE(answer);
	return head_status;
}
KCgiFetchObject::KCgiFetchObject() {
	cmdModel = false;
}
KCgiFetchObject::~KCgiFetchObject() {
}
bool KCgiFetchObject::writeComplete() {
	if (stream.fd[WRITE_PIPE] != INVALIDE_PIPE) {
		ClosePipe(stream.fd[WRITE_PIPE]);
	}
	stream.fd[WRITE_PIPE] = INVALIDE_PIPE;
	return true;
}
int KCgiFetchObject::read(char *buf, int len) {
	return stream.read(buf, len);
}
bool KCgiFetchObject::write(const char *buf, int len) {
	return stream.write_all(buf, len) == STREAM_WRITE_SUCCESS;
}
void KCgiFetchObject::close(bool realClose) {

}
void KCgiFetchObject::process(KHttpRequest *rq)
{
	KHttpObject *obj = rq->ctx->obj;
	if (!brd->rd->enable) {
		send_error(rq, NULL, STATUS_SERVER_ERROR, "extend is disable");
		return;
	}
	KCgiRedirect *rd = static_cast<KCgiRedirect *>(brd->rd);
#ifdef _WIN32
	KStringBuf arg(256);
	KWinCgiEnv env;
#else
	pid_t cpid;
#endif
	int ret = LOAD_HEAD_SUCCESS;
	KHttpObjectParserHook hook(obj, rq);
	if (obj) {
		KBIT_SET(obj->index.flags,ANSW_LOCAL_SERVER);
	}
	if (!stream.create()) {
		goto error;
		//return SEND_HEAD_FAILED;
	}
	if (rd->cmd == NULL && !rq->file->canExecute()) {
		send_error(rq, NULL, STATUS_SERVER_ERROR,
				"cann't run cgi it have no excute right\n");
		goto error;
	}
	//obj->data->status_code = STATUS_OK;

#ifndef _WIN32
	cpid = fork();
	if (cpid == -1) {
		send_error(rq, NULL, STATUS_SERVER_ERROR,
				"Happen error when fork to excute cgi");
		return ;
	}
	stream.setPipe(cpid);
	if (cpid == 0) {
		signal(SIGTERM, SIG_DFL);
		KCgiEnv env;
		addCurrentEnv(&env);
		for (size_t i = 0; i < rd->envs.size(); i++) {
			env.addEnv(rd->envs[i].c_str());
		}
		if (!cmdModel) {
			make_http_env(rq, brd,rq->sink->data.if_modified_since, rq->file, &env);
		}
		stream.closeAllOtherFile();
		char **arg = NULL;
		size_t arg_size = rd->args.size();
		arg = (char **) xmalloc(arg_size * (sizeof(char *) + 2));
		arg[0] = xstrdup(rd->cmd?rd->cmd:rq->file->getName());
		size_t i = 0;
		for (; i < arg_size; i++) {
			if(strcmp(rd->args[i].c_str(),"%s")==0){
				arg[i + 1] = (char *)rq->file->getName();
			}else{
				arg[i + 1] = xstrdup(rd->args[i].c_str());
			}

		}
		arg[i + 1] = NULL;
		KCgi cgi;
		cgi.cmdModel = cmdModel;
#ifdef ENABLE_VH_RUN_AS
		if (my_uid > 0 && rq->svh && rq->svh->vh->id[0] > 0) {
			/*
			 * 改变用户身份
			 */
			setgid(rq->svh->vh->id[1]);
			setuid(rq->svh->vh->id[0]);
		}
#endif
		cgi.redirectIO(stream.fd[READ_PIPE], stream.fd[WRITE_PIPE],
				stream.fd[WRITE_PIPE]);
		cgi.run((rd->cmd ? rd->cmd : rq->file->getName()), arg, &env);
		exit(0);
	}
#else

	Token_t token = NULL;
#ifdef ENABLE_VH_RUN_AS	
	bool result;
	token = rq->svh->vh->createToken(result);
	if(!result) {
		send_error(rq,NULL,STATUS_SERVER_ERROR,"cann't logon");
		goto error;
	}
#endif
	arg << "\"" << (rd->cmd?rd->cmd:rq->file->getName()) << "\"";
	for(size_t i=0;i<rd->args.size();i++) {
		arg << " ";
		arg << "\"" ;
		if(strcmp(rd->args[i].c_str(),"%s")==0){
			arg << rq->file->getName();
		}else{
			arg << rd->args[i].c_str();
		}
		arg << "\"";
	}
	addCurrentEnv(&env);
	for (size_t i = 0; i < rd->envs.size(); i++) {
		env.addEnv(rd->envs[i].c_str());
	}
	if(!cmdModel) {
		make_http_env(rq,brd, rq->sink->data.if_modified_since, rq->file, &env);
	}
	if(!StartInteractiveClientProcess (token,rd->cmd,arg.getString(),&stream,RDSTD_ALL,env.dump_env())) {
#ifdef ENABLE_VH_RUN_AS		
		rq->svh->vh->closeToken(token);
#endif
		goto error;
	}
#ifdef ENABLE_VH_RUN_AS	
	rq->svh->vh->closeToken(token);
#endif
#endif
	if (!cmdModel && rq->sink->data.content_length > 0) {
		if (!readPostData(rq)) {
			klog(KLOG_DEBUG, "send post data error!");
			return ;
		}
	}
	/*
	 * we close the write pipe
	 */
	writeComplete();
	hook.setProto(Proto_fcgi);
	if (!cmdModel) {
		ret = loadStreamHead(&stream, rq, obj, hook);
	}
	if (ret == LOAD_HEAD_SUCCESS) {
		handleUpstreamRecvedHead(rq);
		return ;
	}
	return ;
	error:
#ifdef _WIN32
	send_error(rq,NULL,STATUS_SERVER_ERROR,"Cann't run cgi");
#endif
	return;
}
bool KCgiFetchObject::readPostData(KHttpRequest *rq)
{
#if 0
	if (rq->pre_post_length > 0) {
		if (!this->write(rq->parser.body, rq->pre_post_length)){
			goto error;
		}
		rq->sink->data.left_read -= rq->parser.bodyLen;
		rq->parser.body += rq->pre_post_length;
		rq->parser.bodyLen -= rq->pre_post_length;
		rq->pre_post_length = 0;
	}
#endif
	char buf[1024];
	for (;;) {
		int rest = rq->sink->Read(buf, sizeof(buf));
		if (rest == 0) {
			break;
		}
		if (rest<0) {
			goto error;
		}
		if (!this->write(buf, rest)) {
			goto error;
		}
	}
	return this->writeComplete();
	error: return false;
}
#endif