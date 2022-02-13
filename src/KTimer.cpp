#include "kselector_manager.h"
#include "KTimer.h"
struct timer_run_param
{
	kselector *selector;
	timer_func func;
	void *arg;
};
kev_result result_timer_call_back(KOPAQUE data, void *arg, int got)
{
	timer_run_param *param = (timer_run_param *)arg;
	param->func(param->arg);
	delete param;
	return kev_ok;
}
kev_result next_timer_call_back(KOPAQUE data, void *arg, int got)
{
	timer_run_param *param = (timer_run_param *)arg;
	kselector_add_timer(param->selector,result_timer_call_back, param,got,NULL);
	return kev_ok;
}
void timer_run(timer_func func,void *arg,int msec,unsigned short selector)
{
	timer_run_param *param = new timer_run_param;
	param->selector = (selector == 0 ? get_perfect_selector() : get_selector_by_index(selector));
	param->func = func;
	param->arg = arg;
	kgl_selector_module.next(param->selector,NULL, next_timer_call_back, param,msec);
}
bool test_timer()
{
	return true;
}
