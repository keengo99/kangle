/*
 * KPoolableStreamContainer.h
 *
 *  Created on: 2010-8-18
 *      Author: keengo
 */

#ifndef KPOOLABLESTREAMCONTAINER_H_
#define KPOOLABLESTREAMCONTAINER_H_
#include <list>
#include "global.h"
#include "KUpstream.h"
#include "KStringBuf.h"
#include "KMutex.h"
#include "KCountable.h"
#include "time_utils.h"
#include "KHttpEnv.h"
void SafeDestroyUpstream(KUpstream *st);
/*
 * 连接池容器类
 */
class KPoolableSocketContainerImp {
public:
	KPoolableSocketContainerImp();
	~KPoolableSocketContainerImp();
	void refresh(bool clean);
	void refreshList(kgl_list *l, bool clean);
	kgl_list *GetList()
	{
		return &head;
	}	
	unsigned size;
protected:
	kgl_list head;
};
class KPoolableSocketContainer: public KCountableEx {
public:
	KPoolableSocketContainer();
	virtual ~KPoolableSocketContainer();
	KUpstream *GetPoolSocket(KHttpRequest *rq);
	/*
	回收连接
	close,是否关闭
	lifeTime 连接时间
	*/
	virtual void gcSocket(KUpstream *st,int lifeTime, time_t base_time);
	void bind(KUpstream *st);
	void unbind(KUpstream *st);
	int getLifeTime() {
		return lifeTime;
	}
	kgl_refs_string *GetParam();
	void SetParam(const char *param);
	/*
	 * 设置连接超时时间
	 */
	void setLifeTime(int lifeTime);
	/*
	 * 定期刷新删除过期连接
	 */
	virtual void refresh(time_t nowTime);
	/*
	 * 清除所有连接
	 */
	void clean();
	/*
	 * 得到连接数
	 */
	unsigned getSize() {
		unsigned size = 0;
		lock.Lock();
		if (imp) {
			size = imp->size;
		}
		lock.Unlock();
		return size;
	}
	//isBad,isGood用于监控连接情况
	virtual void isBad(KUpstream *st,BadStage stage)
	{
	}
	virtual void isGood(KUpstream *st)
	{
	}

#ifdef HTTP_PROXY
	virtual void addHeader(KHttpRequest *rq,KHttpEnv *s)
	{
	}
#endif
protected:
	/*
	 * 把连接真正放入池中
	 */
	void PutPoolSocket(KUpstream *st);
	kgl_refs_string *param;
	int lifeTime;	
	KMutex lock;
private:
	KUpstream *internalGetPoolSocket(KHttpRequest *rq);
	time_t getHttp2ExpireTime()
	{
		int lifeTime = this->lifeTime;
		if (lifeTime <= 10) {
			//http2最少10秒连接时间
			lifeTime = 10;
		}
		return kgl_current_sec + lifeTime;
	}
	KPoolableSocketContainerImp *imp;
};
#endif /* KPOOLABLESTREAMCONTAINER_H_ */
