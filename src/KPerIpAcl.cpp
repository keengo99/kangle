#include "KPerIpAcl.h"
#include "KAccess.h"
void per_ip_mark_call_back(void *data)
{
	KPerIpCallBackData *param = (KPerIpCallBackData *)data;
	param->mark->callBack(param);
}
void KPerIpAcl::callBack(KPerIpCallBackData *data) {
	std::map<char *, unsigned,lessp>::iterator it_ip;
	ip_lock.Lock();
	it_ip = ip_map.find(data->ip);
	assert(it_ip!=ip_map.end());
	(*it_ip).second--;
	//printf("-max_per_ip=%d,refs=%d,rq=%x\n",(*it_ip).second,getRef(),rq);
	if ((*it_ip).second == 0) {
		free((*it_ip).first);
		ip_map.erase(it_ip);
	}
	ip_lock.Unlock();
	delete data;
}
