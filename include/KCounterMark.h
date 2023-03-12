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
	bool mark(KHttpRequest *rq, KHttpObject *obj, KFetchObject** fo) override
	{
		lock.Lock();
		counter++;
		lock.Unlock();
		return true;
	}
	KMark * new_instance() override
	{
		return new KCounterMark();
	}
	const char *getName() override
	{
		return "counter";
	}
	void get_html(KModel* model, KWStream& s) override {
		s << "<input type=checkbox name='reset' value='1'>reset";
	}
	void get_display(KWStream& s) override {
		s << counter;
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
	}
	int whmCall(WhmContext *ctx) override
	{
		const char *op = ctx->getUrlValue()->getx("op");
		if (op==NULL) {
			ctx->setStatus("op is missing");
			return 404;
		}
		if (strcmp(op,"get")==0) {
			lock.Lock();
			ctx->add("counter",counter);
			lock.Unlock();
			return 200;
		}
		if (strcmp(op,"get_reset")==0) {
			lock.Lock();
			ctx->add("counter",counter);
			counter = 0;
			lock.Unlock();
			return 200;
		}
		return 400;
	}
private:
	KMutex lock;
	INT64 counter;
};
#endif
