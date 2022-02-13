#include "KWhiteList.h"
#include "time_utils.h"
#ifdef ENABLE_BLACK_LIST
using namespace std;
KWhiteListManager wlm;
bool KWhiteList::add(const char *ip,KWhiteListManager *wlm)
{
	std::map<char *,KWhiteListItem *,lessp>::iterator it;
	it = items.find((char *)ip);
	if (it!=items.end()) {
		KWhiteListItem *item = (*it).second;
		item->lastTime = kgl_current_sec;
		klist_remove(item);
		wlm->addItem(item);
		return true;
	}
	KWhiteListItem *item = new KWhiteListItem;
	item->host = host;
	item->ip = strdup(ip);
	item->lastTime = kgl_current_sec;
	items.insert(pair<char *,KWhiteListItem *>(item->ip,item));
	wlm->addItem(item);
	return true;
}
bool KWhiteList::find(const char *ip,KWhiteListManager *wlm)
{
	std::map<char *,KWhiteListItem *,lessp>::iterator it;
	it = items.find((char *)ip);
	if (it != items.end()) {
		if (wlm != NULL) {
			KWhiteListItem *item = (*it).second;
			item->lastTime = kgl_current_sec;
			klist_remove(item);
			wlm->addItem(item);
		}
		return true;
	}
	return false;
}
void KWhiteList::remove(KWhiteListItem *item)
{
	size_t num = items.erase((char *)item->ip);
	assert(num == 1);
}
#endif
