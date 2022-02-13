#ifndef KIPMAP_H
#define KIPMAP_H
#include "global.h"
#include "kforwin32.h"
#include "ksocket.h"
#include "krbtree.h"
#include <sstream>
struct dns_range_addr
{
	ip_addr min_addr;
	ip_addr max_addr;
	//ÏßÂ·
	void *bind_data;
};
class KIpMap {
public:
	KIpMap()
	{
		ip = rbtree_create();
		item_count = 0;
	}
	~KIpMap()
	{
		clear();
		rbtree_destroy(ip);
	}
	int getCount()
	{
		return this->item_count;
	}
	void clear();
	void *find(ip_addr *local_addr);
	void *find(const char *addr);
	void dump_addr(iteratorbt iterator, void *arg);
	void dump_addr(std::stringstream &s,char split);
	int add_multi_addr(const char *addr,const char split_char,void *bind_data);
	bool add_min_max_addr(const char *min_addr,const char *max_addr,void *bind_data);
	bool add_cidr_addr(const char *min_addr,int prefix,void *bind_data);
	bool add_addr(char *addr, void *bind_data);
private:	
	bool add_range_addr(struct dns_range_addr *range_addr,void *bind_data);
	krb_tree *ip;
	int item_count;
};
bool test_ip_map();

#endif
