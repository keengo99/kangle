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
	bool supportRuntime() override
	{
		return true;
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
	std::string getHtml(KModel *model) override
	{
		std::stringstream s;
		s << "<input type=checkbox name='reset' value='1'>reset";
		return s.str();
	}
	std::string getDisplay() override
	{
		std::stringstream s;
		s << counter;
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html)  override
	{
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
	void buildXML(std::stringstream &s,int flag) override
	{
		s << ">";
	}
private:
	KMutex lock;
	INT64 counter;
};
#endif
