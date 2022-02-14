#ifndef KTCPSINK_H_99
#define KTCPSINK_H_99
#include "KSink.h"
#include "kconnection.h"
#include "kfiber.h"

class KTcpSink : public KSink {
public:
	KTcpSink(kconnection *cn);
	~KTcpSink();
	bool IsLocked()
	{
		return TEST(cn->st.st_flags, STF_LOCK);
	}
	void SetTimeOut(int tmo)
	{
		cn->st.tmo = tmo;
		cn->st.tmo_left = tmo;
	}
	int GetTimeOut()
	{
		return cn->st.tmo;
	}
	bool ReadHup(void *arg, result_callback result)
	{
		return selectable_readhup(&cn->st, result, arg);
	}
	void RemoveReadHup()
	{
		selectable_remove_readhup(&cn->st);
	}
	void Shutdown()
	{
		selectable_shutdown(&cn->st);
	}
	int Read(char *buf, int len)
	{
		return kfiber_net_read(cn, buf, len);
	}
	int Write(LPWSABUF buf, int bc)
	{
		return kfiber_net_writev(cn, buf, bc);
	}
	void AddSync()
	{
		selectable_add_sync(&cn->st);
	}
	void RemoveSync()
	{
		selectable_remove_sync(&cn->st);
	}
	kconnection *GetConnection()
	{
		return cn;
	}
	kev_result StartRequest(KHttpRequest *rq);
	int EndRequest(KHttpRequest *rq);
	bool ResponseStatus(KHttpRequest *rq, uint16_t status_code)
	{
		return false;
	}
	bool ResponseHeader(const char *name, int name_len, const char *val, int val_len)
	{
		return false;
	}
	bool ResponseConnection(const char *val, int val_len)
	{
		return false;
	}
	int StartResponseBody(KHttpRequest *rq, int64_t body_size)
	{
		return 0;
	}
	void SetDelay()
	{
		ksocket_delay(cn->st.fd);
	}
	void SetNoDelay(bool forever)
	{
		ksocket_no_delay(cn->st.fd, forever);
	}
	kconnection *cn;
};
#endif

