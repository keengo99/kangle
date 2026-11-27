#include "KIpList.h"
#include "KDynamicString.h"
#include "WhmContext.h"
#include "kfile.h"
kasync_worker *run_fw_worker = NULL;
struct run_fw_worker_arg
{
	char *cmd;
	char *ip;
};
kev_result run_fw_call_back(void *data,int msec)
{
	run_fw_worker_arg *worker_arg = (run_fw_worker_arg *)data;
	char *buf = worker_arg->cmd;
	std::vector<char *> args;
	explode_cmd(buf,args);
	KDynamicString ds;
	if (worker_arg->ip) {
		ds.addEnv("ip",worker_arg->ip);
	}
	char **arg = new char *[args.size()+1];
	size_t i = 0;
	size_t j = 0;
	for (;i<args.size();i++) {
		if (args[i]==NULL) {
			continue;
		}
		arg[j] = ds.parseString(args[i]).release();
		j++;
	}
	arg[j] = NULL;
	PIPE_T fd[2];
	if (KPipeStream::create(fd)) {
		pid_t pid;
		if (createProcess(NULL,arg,NULL,NULL,INVALIDE_PIPE,fd[WRITE_PIPE],fd[WRITE_PIPE],pid)) {
			kfclose(fd[WRITE_PIPE]);
			char tbuf[512];
			//wait for process
			for (;;) {
				int len = kfread(fd[READ_PIPE],tbuf,sizeof(tbuf)-1);
				if (len<=0) {
					break;
				}
				tbuf[len] = '\0';
				klog(KLOG_ERR,"%s\n",tbuf);
			}
	#ifdef _WIN32
			CloseHandle(pid);
	#endif
		} else {
			kfclose(fd[WRITE_PIPE]);
		}
		kfclose(fd[READ_PIPE]);
	}
	for (i=0;;i++) {
		if (arg[i]==NULL) {
			break;
		}
		free(arg[i]);
	}
	delete[] arg;
	free(worker_arg->cmd);
	if (worker_arg->ip) {
		free(worker_arg->ip);
	}
	delete worker_arg;
	return kev_ok;
}
void init_run_fw_cmd()
{
	run_fw_worker = kasync_worker_init(1,0);
}
void run_fw_cmd(const char *cmd,const char *ip)
{
	run_fw_worker_arg *arg = new run_fw_worker_arg;
	memset(arg,0,sizeof(run_fw_worker_arg));
	arg->cmd = strdup(cmd);
	if (ip) {
		arg->ip = strdup(ip);
	}
	kasync_worker_start(run_fw_worker,arg,run_fw_call_back);
}
//{{ent
#ifdef ENABLE_BLACK_LIST
size_t KIpList::getDynamicCount()
{
	KLocker locker(&lock);
	size_t count = 0;
	for (auto& item : wls) {
		if (!item.second->IsStatic()) {
			++count;
		}
	}
	return count;
}
size_t KIpList::clearDynamic()
{
	lock.Lock();
	KIpListItem* removed = head;
	head = end = NULL;
	size_t count = 0;
	bool unblock = run_fw_cmd && *conf.unblock_ip_cmd;
	for (auto item = removed; item; item = item->next) {
		// Converted/removed entries remain on this queue until GC. Their map
		// entry can now be a fixed allow/deny entry and must be preserved.
		if (!item->IsConvertToStatic()) {
			wls.erase(item->ip);
			++count;
		}
	}
	lock.Unlock();
	while (removed) {
		KIpListItem* next = removed->next;
		if (unblock && !removed->IsConvertToStatic()) {
			::run_fw_cmd(conf.unblock_ip_cmd, removed->ip);
		}
		delete removed;
		removed = next;
	}
	return count;
}
void KIpList::clearBlackList()
{
	lock.Lock();
	clearStatic();
	while (head) {
		end = head->next;
		delete head;
		head = end;
	}
	end = NULL;
	lock.Unlock();
}
//得到黑名单
void KIpList::getBlackList(WhmContext *ctx)
{
	lock.Lock();
	KIpListItem *ip_item = head;
	while (ip_item) {
		ctx->add("ip",ip_item->ip);
		ip_item=ip_item->next;
	}
	lock.Unlock();
}
#endif
//}}
