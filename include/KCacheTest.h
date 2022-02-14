#ifndef KCACHETEST_H
#define KCACHETEST_H
class KHttpRequest;
class KCacheTest
{
public:
	KCacheTest();
	virtual ~KCacheTest()
	{
	}
	bool test(KHttpRequest *rq);
	bool parse(const char *str);
	KCacheTestItem *head;
};
#endif
