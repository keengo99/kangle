#include "WhmShellProcess.h"
static int write_pipe(PIPE_T fd,const char *buf,int len)
{		//{{ent
#ifdef _WIN32
	unsigned long bytesRead;
	if(!WriteFile(fd,buf,len,&bytesRead,NULL)) {
		//debug("writePipe error errno=%d\n",GetLastError());
		return -1;
	}
	return bytesRead;
#else
	//}}
	return ::write(fd, buf, len);
//{{ent
#endif
//}}
}
static int read_pipe(PIPE_T fd,char *buf,int len) {
//{{ent
#ifdef _WIN32
	unsigned long bytesRead;
	if(!ReadFile(fd,buf,len,&bytesRead,NULL)) {
		//debug("readPipe error errno=%d\n",GetLastError());
		return -1;
	}
	return bytesRead;

#else
	//}}
	return ::read(fd, buf, len);
//{{ent
#endif
//}}
}
bool WhmShellProcess::run(WhmShellContext *sc)
{
	WhmShellCommand *c = command;
	bool result = true;	
	Token_t token = NULL;
#ifndef HTTP_PROXY
	if (runAsUser) {
		//创建运行身份令牌
		if (sc->vh==NULL) {
			sc->last_error = 128;
			return false;
		}
		token = sc->vh->createToken(result);
		if(!result){
			sc->last_error = 129;
			return false;
		}
	}
#endif
	//the commands stdout pipe
	PIPE_T big_stdout_pipe[2];
	if (!KPipeStream::create(big_stdout_pipe)) {
		if (token) {
			KVirtualHost::closeToken(token);
		}
		return false;
	}
	//the commands stdin pipe
	PIPE_T big_stdin_pipe[2];
	bool big_stdin_pipe_created = false;
	//init the std file
	PIPE_T hstdin = get_stdin(sc);
	if (hstdin==INVALIDE_PIPE && sc->in_buffer.getLen()>0) {
		//如果没有输入文件并且输入有数据。就要创建输入管道
		if (!KPipeStream::create(big_stdin_pipe)) {
			//关闭打开的big_stdout_pipe
			ClosePipe(big_stdout_pipe[0]);
			ClosePipe(big_stdout_pipe[1]);
			if (token) {
				KVirtualHost::closeToken(token);
			}
			return false;
		}
		big_stdin_pipe_created = true;
		hstdin = big_stdin_pipe[READ_PIPE];
	}
	PIPE_T hstdout = get_stdout(sc);
	PIPE_T hstderr = get_stderr(sc);
	PIPE_T in,out,err;
	err = hstderr;
	if (err==INVALIDE_PIPE) {
		err = big_stdout_pipe[WRITE_PIPE];
	}
	PIPE_T pp[2];
	for(int i=0;i<2;i++){
		pp[i] = INVALIDE_PIPE;
	}
	while (c) {
		char **arg = c->makeArgs(sc);
		KCmdEnv *env = c->makeEnv(sc);
		pid_t pid;
		kfinit(pid);
		//set in
		if (c==command) {
			in = hstdin;
		} else {
			assert(pp[READ_PIPE]!=INVALIDE_PIPE);
			in = pp[READ_PIPE];
			pp[READ_PIPE] = INVALIDE_PIPE;
			assert(pp[WRITE_PIPE]!=INVALIDE_PIPE);
			//close the out pipe
			ClosePipe(pp[WRITE_PIPE]);
			pp[WRITE_PIPE] = INVALIDE_PIPE;
		}
		//set out
		if (c->next) {
			//create a new pipe
			if (!KPipeStream::create(pp)) {
				if (c!=command) {
					ClosePipe(in);
				}
				break;
			}
			out = pp[WRITE_PIPE];
		} else {
			//if the command is last.
			//then set the out to big_pipe
			out = hstdout;
			if (out==INVALIDE_PIPE) {
				out = big_stdout_pipe[WRITE_PIPE];
			}
		}
		kgl_auto_cstr curdir2;
		if (curdir) {
			curdir2 = sc->parseString(curdir);
		}
		result = createProcess(token,arg,env,curdir2.get(),in,out,err,pid);
		if (c!=command) {
			//close the in pipe
			ClosePipe(in);
		}
		//free args
		for (int i=0;;i++) {
			if(arg[i]==NULL){
				break;
			}
			free(arg[i]);
		}
		delete[] arg;
		if (env) {
			delete env;
		}	
		sc->setLastPid(pid);
		if (!result) {
			sc->last_error = 127;
			break;
		}	
		c = c->next;
	}
	//关闭输入，输出父进程无用的管道端
	if (kflike(hstdin)) {
		ClosePipe(hstdin);
	}
	ClosePipe(big_stdout_pipe[WRITE_PIPE]);
	//处理输入
	if (big_stdin_pipe_created) {
		if (result) {
			//创建成功才写入数据
			kbuf *buf = sc->in_buffer.getHead();
			while (buf && buf->data) {
				if (write_pipe(big_stdin_pipe[WRITE_PIPE],buf->data,buf->used)!=buf->used) {
					break;
				}
				buf = buf->next;
			}
		}
		//清理输入数据和管道资源
		sc->in_buffer.destroy();
		ClosePipe(big_stdin_pipe[WRITE_PIPE]);
	}
	//处理输出
	if (result && (hstdout==INVALIDE_PIPE || hstderr==INVALIDE_PIPE)) {
		for (;;) {
			char buf[512];
			int len = read_pipe(big_stdout_pipe[READ_PIPE],buf,512);
			if (len<=0) {
				break;
			}
			sc->lock.Lock();
			if (sc->out_buffer.getLen() > 1048576) {
				//the out msg is too large.drop it.
				sc->lock.Unlock();
				fwrite(buf,1,len,stdout);
			} else {
				sc->out_buffer.write_all(buf,len);
				sc->lock.Unlock();
			}
			//fwrite(buf,1,len,stdout);	
		}
	}
	if (kflike(hstdout)) {
		ClosePipe(hstdout);
	}
	if (kflike(hstderr)) {
		ClosePipe(hstderr);
	}
	ClosePipe(big_stdout_pipe[READ_PIPE]);
	if (token) {
		KVirtualHost::closeToken(token);
	}
#ifdef _WIN32
	if (kflike(sc->last_pid)) {
		WaitForSingleObject(sc->last_pid,INFINITE);
	}
#endif
	return result;
}
void WhmShellProcess::addCommand(const char *cmd)
{
	WhmShellCommand *n = new WhmShellCommand;
	n->setCmd(cmd);
	if (last==NULL) {
		command = last = n;
	} else {
		last->next = n;
		last = n;
	}
	assert(last->next == NULL);
}
