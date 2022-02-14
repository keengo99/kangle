#ifndef KFLOWINFO_H
#define KFLOWINFO_H
#include "global.h"
#include "KCountable.h"
/**
* 流量统计类
*/
class KFlowData {
public:
	//总流量
	INT64 flow;
	//缓存的流量
	INT64 cache;
	//post上行流量
	volatile int64_t post;
	INT64 last_flow;
	INT64 last_time;
};
class KFlowInfo : public KCountableEx,public KFlowData
{
public:
	KFlowInfo()
	{
		KFlowData *data = static_cast<KFlowData *>(this);
		memset(data,0,sizeof(KFlowData));
		last_time = kgl_current_msec;
	}
	void AddUpFlow(INT64 flow)
	{
		katom_add64((void *)&post, flow);
	}
	void AddDownFlow(INT64 flow,bool cache_hit)
	{
		refsLock.Lock();
		if (cache_hit) {
			cache += flow;
		}
		this->flow += flow;
		refsLock.Unlock();
	}
	void reset()
	{
		refsLock.Lock();
		last_flow -= flow;
		flow = 0;
		cache = 0;
		refsLock.Unlock();
	}
	int dump(char *buf,int len,int64_t &post)
	{
#ifdef _WIN32
		const char *formatString="%I64d\t%I64d";
#else
		const char *formatString = "%lld\t%lld";
#endif
		refsLock.Lock();
		len = snprintf(buf,len,formatString,flow,cache);
		last_flow -= flow;
		flow = 0;
		cache = 0;
		refsLock.Unlock();
		post = katom_set64((void *)&this->post, 0);
		return len;
	}
	INT64 get_speed(bool reset)
	{
		INT64 current_msec = kgl_current_msec;	
		refsLock.Lock();
		INT64 inc_time = current_msec - last_time;
		if (inc_time<=0) {
			refsLock.Unlock();
			return 0;
		}
		INT64 inc_flow = flow - last_flow;	
		if (reset) {
			last_flow = flow;
			last_time = current_msec;
		}
		refsLock.Unlock();
		return (INT64)((inc_flow*1000)/inc_time);
	}

};
class KFlowInfoHelper
{
public:
	KFlowInfoHelper(KFlowInfo *fi)
	{
		next = NULL;
		this->fi = fi;
	}
	~KFlowInfoHelper()
	{
		if (fi) {
			fi->release();
		}
	}
	KFlowInfoHelper *next;
	KFlowInfo *fi;
};
#endif
