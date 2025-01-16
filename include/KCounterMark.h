#ifndef KCOUNTER_MARK_H
#define KCOUNTER_MARK_H
class KCounterMark : public KMark
{
public:
	KCounterMark()
	{
		counter = 0;
	}
	~KCounterMark()
	{
	}
	uint32_t process(KHttpRequest* rq, KHttpObject* obj, KSafeSource& fo) override
	{
		lock.Lock();
		counter++;
		lock.Unlock();
		return KF_STATUS_REQ_TRUE;
	}
	KMark * new_instance() override
	{
		return new KCounterMark();
	}
	const char *get_module() const override
	{
		return "counter";
	}
	void get_html(KWStream& s) override {
		s << "<input type=checkbox name='reset' value='1'>reset";
	}
	void get_display(KWStream& s) override {
		s << counter;
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
	}
private:
	KMutex lock;
	INT64 counter;
};
#endif
