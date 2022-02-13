#include "KIpMap.h"
#include "log.h"
#include "katom.h"
#include "kmalloc.h"
#include <sstream>
static int ip_addr_cmp(ip_addr *a1,ip_addr *a2)
{
#ifdef KSOCKET_IPV6
	int ret = (int)a1->sin_family - (int)a2->sin_family;
	if (ret!=0) {
		return ret;
	}	
	if (a1->sin_family == PF_INET) {
		if (a1->addr32[0] == a2->addr32[0]) {
			return 0;
		} else if(a1->addr32[0]>a2->addr32[0]) {
			return 1;
		}
		return -1;
	}
	for (int i=0;i<4;i++) {
		if (a1->addr32[i] > a2->addr32[i]) {
			return 1;
		} else if (a1->addr32[i] <a2->addr32[i]) {
			return -1;
		}
	}
	return 0;
#else
	if (*a1 == *a2) {
		return 0;
	} else if(*a1>*a2) {
		return 1;
	}
	return -1;	
#endif
}
/* ipv4的插入比较函数 */
static int range_addr_find_cmp(void *k1,void *k2)
{
	ip_addr *addr = (ip_addr *)k1;
	struct dns_range_addr *a2 = (struct dns_range_addr *)k2;
	int ret = ip_addr_cmp(addr,&a2->min_addr);
	if (ret<0) {
		return ret;
	}
	ret = ip_addr_cmp(addr,&a2->max_addr);
	if (ret>0) {
		return ret;
	}
	return 0;
}
/* ipv4的插入比较函数 */
static int range_addr_insert_cmp(void *k1,void *k2)
{
	struct dns_range_addr *a1 = (struct dns_range_addr *)k1;
	struct dns_range_addr *a2 = (struct dns_range_addr *)k2;
	return ip_addr_cmp(&a1->min_addr,&a2->min_addr);
}
void hton_addr(ip_addr *addr)
{
#ifdef KSOCKET_IPV6
	if (addr->sin_family==PF_INET) {
		addr->addr32[0] = htonl(addr->addr32[0]);
		return;
	}
	for (int i=0;i<4;i++) {
		addr->addr32[i] = htonl(addr->addr32[i]);
	}
#else
	*addr = htonl(*addr);
#endif
}
void ntoh_addr(ip_addr *addr)
{
#ifdef KSOCKET_IPV6
	if (addr->sin_family==PF_INET) {
		addr->addr32[0] = ntohl(addr->addr32[0]);
		return;
	}
	for (int i=0;i<4;i++) {
		addr->addr32[i] = ntohl(addr->addr32[i]);
	}
#else
	*addr = ntohl(*addr);
#endif
}
bool get_local_addr(const char *value,ip_addr *addr)
{
	
	if (!ksocket_get_ipaddr(value,addr)) {
		return false;
	}
	ntoh_addr(addr);
	return true;
}

void addr_add(ip_addr *addr,uint32_t value)
{
#ifdef KSOCKET_IPV6
	if (addr->sin_family==PF_INET) {
		addr->addr32[0] += value;
		return;
	}
	addr->addr32[3] += value;
	if (addr->addr32[3] < value) {		
		if (++addr->addr32[2]==0) {			
			if (++addr->addr32[1]==0) {
				++addr->addr32[0];
			}
		}
	}
#else
	*addr += value;
#endif
}
void addr_sub(ip_addr *addr,uint32_t value)
{
#ifdef KSOCKET_IPV6
	if (addr->sin_family==PF_INET) {
		addr->addr32[0] -= value;
		return;
	}
	if (addr->addr32[3] < value) {		
		if (addr->addr32[2]--==0) {			
			if (addr->addr32[1]--==0) {
				addr->addr32[0]--;
			}
		}
	}
	addr->addr32[3] -= value;
#else
	*addr -= value;
#endif
}
bool make_local_ip(ip_addr *addr,char *ips,int ips_len)
{
	ip_addr net_addr;
	kgl_memcpy(&net_addr,addr,sizeof(ip_addr));
	hton_addr(&net_addr);	
	return ksocket_ipaddr_ip(&net_addr,ips,ips_len);
}
inline void get_ipv4_cidr_max_addr(uint32_t *min_addr,uint32_t *max_addr,int prefix)
{
	*min_addr &= ((uint32_t)~0 << (32 - prefix));
	if (prefix<32) {
		*max_addr = *min_addr | ((uint32_t)~0 >> prefix);
	} else {
		*max_addr = *min_addr;
	}
}
bool build_cidr_addr(const char *addr,int prefix,ip_addr *min_addr,ip_addr *max_addr)
{
	if (!get_local_addr(addr,min_addr)) {
		return false;
	}
#ifndef KSOCKET_IPV6
	get_ipv4_cidr_max_addr(min_addr,max_addr,prefix);
#else
	max_addr->sin_family = min_addr->sin_family;
	if (min_addr->sin_family==PF_INET) {		
		get_ipv4_cidr_max_addr(&min_addr->addr32[0],&max_addr->addr32[0],prefix);		
	} else {
		int i;
		int a,b;
		a = prefix/32;
		if (a>4 || a<0) {
			return 1;
		}
		for (i=0;i<a;i++) {
			max_addr->addr32[i] = min_addr->addr32[i];
		}
		if (a<4) {		
			b = prefix - a * 32;
			if (b>0) {
				min_addr->addr32[a] &= ((uint32_t)~0 << (32 - b));
			} else {
				min_addr->addr32[a] = 0;
			}
			max_addr->addr32[a] =  min_addr->addr32[a] | ((uint32_t)~0 >> b);						
			for (i=a+1;i<4;i++) {
				min_addr->addr32[i] = 0;
				max_addr->addr32[i] = (uint32_t)~0;
			}
		}
	}
#endif
	return true;
}
static iterator_ret del_all_ip_iterator(void *data,void *arg)
{
	dns_range_addr *addr = (dns_range_addr *)data;
	free(addr);
	return iterator_remove_continue;
}
void KIpMap::clear()
{
	rbtree_iterator(ip, del_all_ip_iterator, NULL);
	item_count = 0;
	assert(ip->root.rb_node == NULL);
}
int KIpMap::add_multi_addr(const char *ips,const char split_char,void *bind_data)
{
	char *buf = strdup(ips);
	if (buf==NULL) {
		klog(KLOG_ERR,"no memory to alloc for add_view_ips\n");
		return 0;
	}
	char *hot = buf;
	int success_count = 0;
	for (;;) {
		char *next_line = strchr(hot,split_char);
		if (next_line) {
			char *end_str = next_line;
			while (end_str>hot && isspace((unsigned char)*end_str)) {
				*end_str = '\0';
				end_str--;
			}
			*next_line = '\0';
		}
		//printf("try add_addr=[%s]\n",hot);
		if (add_addr(hot,bind_data)) {
			success_count++;
			//printf("success\n");
		} else {
			//printf("failed=[%s]\n",hot);
		}
		if (next_line==NULL) {
			break;
		}
		hot = next_line + 1;
	}
	free(buf);
	return success_count;
}
bool KIpMap::add_addr(char *addr,void *bind_data)
{
	char *prefix = strchr(addr,'/');
	if (prefix) {
		*prefix = '\0';
		return add_cidr_addr(addr,atoi(prefix+1),bind_data);
	}
	prefix = strchr(addr,'-');
	if (prefix) {
		*prefix = '\0';
		return add_min_max_addr(addr,prefix+1,bind_data);
	}			
	return add_min_max_addr(addr, addr, bind_data);	
}
bool KIpMap::add_range_addr(struct dns_range_addr *range_addr,void *bind_data)
{
	char ips[MAXIPLEN];
	int new_flag;
	struct krb_node *node;
	if (ip_addr_cmp(&range_addr->min_addr,&range_addr->max_addr)>0) {
		make_local_ip(&range_addr->min_addr, ips, MAXIPLEN);
		klog(KLOG_ERR,"ip [%s] is error,min>max\n",ips);
		return false;
	}
	node = rbtree_insert(ip,range_addr,&new_flag,range_addr_insert_cmp);	
	if (new_flag==0) {		
		make_local_ip(&range_addr->min_addr, ips, MAXIPLEN);
		klog(KLOG_ERR,"ip [%s] is duplicate\n",ips);
		return false;
	}
	assert(node);
	struct dns_range_addr *addr = (struct dns_range_addr *)malloc(sizeof(struct dns_range_addr));
	kgl_memcpy(addr,range_addr,sizeof(struct dns_range_addr));
	node->data = addr;
	addr->bind_data = bind_data;
	//处理前面的ip覆盖
	struct krb_node *pre_node = rb_prev(node);
	if (pre_node) {
		struct dns_range_addr *pre_addr = (struct dns_range_addr *)pre_node->data;
		if (ip_addr_cmp(&pre_addr->max_addr,&range_addr->max_addr)>0) {
			make_local_ip(&range_addr->min_addr, ips, MAXIPLEN);
			//klog(KLOG_WARNING,"IP [%s] is pre-covered max,view = [%d]\n",ips,view->id);
			//产生新的range ip插入
			struct dns_range_addr new_addr;
			memset(&new_addr,0,sizeof(new_addr));
			kgl_memcpy(&new_addr.min_addr,&range_addr->max_addr,sizeof(new_addr.min_addr));
			kgl_memcpy(&new_addr.max_addr,&pre_addr->max_addr,sizeof(new_addr.max_addr));
			addr_add(&new_addr.min_addr,1);
			//重新调整之前的range ip
			kgl_memcpy(&pre_addr->max_addr,&range_addr->min_addr,sizeof(ip_addr));
			addr_sub(&pre_addr->max_addr,1);
			add_range_addr(&new_addr,pre_addr->bind_data);
			//printf("ret=%d\n",ret);
		} else if (ip_addr_cmp(&pre_addr->max_addr,&range_addr->min_addr)>=0) {
			make_local_ip(&range_addr->min_addr, ips, MAXIPLEN);
			//klog(KLOG_WARNING,"IP [%s] is pre-covered min,view = [%d]\n",ips,view->id);
			//重新调整之前的range ip
			kgl_memcpy(&pre_addr->max_addr,&range_addr->min_addr,sizeof(ip_addr));
			addr_sub(&pre_addr->max_addr,1);
		}
	}
	//处理后面的ip覆盖
	struct krb_node *next_node = rb_next(node);
	if (next_node) {
		struct dns_range_addr *next_addr = (struct dns_range_addr *)next_node->data;
		if (ip_addr_cmp(&range_addr->max_addr,&next_addr->max_addr)>0) {
			make_local_ip(&range_addr->min_addr, ips, MAXIPLEN);
			//klog(KLOG_WARNING,"IP [%s] is next-covered max,,view = [%d]\n",ips,view->id);
			struct dns_range_addr new_addr;
			memset(&new_addr,0,sizeof(new_addr));
			kgl_memcpy(&new_addr.min_addr,&next_addr->max_addr,sizeof(new_addr.min_addr));
			kgl_memcpy(&new_addr.max_addr,&range_addr->max_addr,sizeof(new_addr.max_addr));
			addr_add(&new_addr.min_addr,1);
			kgl_memcpy(&range_addr->max_addr,&next_addr->min_addr,sizeof(ip_addr));
			addr_sub(&range_addr->max_addr,1);
			add_range_addr(&new_addr,bind_data);
		} else if (ip_addr_cmp(&range_addr->max_addr,&next_addr->min_addr)>=0) {
			make_local_ip(&range_addr->min_addr, ips, MAXIPLEN);
			//klog(KLOG_WARNING,"IP [%s] is next-covered min,view = [%d]\n",ips,view->id);
			kgl_memcpy(&range_addr->max_addr,&next_addr->min_addr,sizeof(ip_addr));
			addr_sub(&range_addr->max_addr,1);
		}
	}
	return true;
}

bool KIpMap::add_min_max_addr(const char *min_addr,const char *max_addr,void *view)
{
	struct dns_range_addr range_addr;
	memset(&range_addr,0,sizeof(range_addr));
	bool include_min_addr = true;
	if (*min_addr == '_') {
		include_min_addr = false;
		min_addr++;
	}
	bool include_max_addr = true;
	if (*max_addr == '_') {
		include_max_addr = false;
		max_addr++;
	}
	if (!get_local_addr(min_addr,&range_addr.min_addr)) {
		return false;
	}
	if (!include_min_addr) {
		addr_add(&range_addr.min_addr, 1);
	}
	if (!get_local_addr(max_addr,&range_addr.max_addr)) {
		return false;
	}
	if (!include_max_addr) {
		addr_sub(&range_addr.max_addr, 1);
	}
	return add_range_addr(&range_addr,view);
}
bool KIpMap::add_cidr_addr(const char *min_addr,int prefix,void *bind_data)
{
	struct dns_range_addr range_addr;
	memset(&range_addr,0,sizeof(range_addr));
	if (!build_cidr_addr(min_addr,prefix,&range_addr.min_addr,&range_addr.max_addr)) {
		klog(KLOG_ERR,"cann't build cidr_addr [%s/%d]\n",min_addr,prefix);
		return false;
	}
	return add_range_addr(&range_addr,bind_data);
}
void *KIpMap::find(const char *ip)
{
	ip_addr addr;
	if (!get_local_addr(ip,&addr)) {
		return NULL;
	}
	return find(&addr);
}
void *KIpMap::find(ip_addr *local_addr)
{
	struct krb_node *node = rbtree_find(ip,local_addr,range_addr_find_cmp);	
	if (node==NULL) {
		return NULL;
	}
	struct dns_range_addr *range_addr = (struct dns_range_addr *)node->data;
	assert(range_addr);
	return range_addr->bind_data;
}
typedef struct {
	std::stringstream *s;
	char split;
}dump_addr_arg ;
static iterator_ret dump_addr_iterator(void *data,void *arg)
{
	dump_addr_arg *da = (dump_addr_arg *)arg;
	dns_range_addr *addr = (dns_range_addr *)data;
	char ips[MAXIPLEN],ips2[MAXIPLEN];
	make_local_ip(&addr->min_addr,ips,MAXIPLEN);
	if (!da->s->str().empty()) {
		*(da->s) << da->split;
	}
	*(da->s) << ips;
	make_local_ip(&addr->max_addr,ips2,MAXIPLEN);
	if (strcmp(ips2, ips) != 0) {
		*(da->s) << "-" << ips2;
	}	
	return iterator_continue;
}

void KIpMap::dump_addr(std::stringstream &s, char split)
{
	dump_addr_arg arg;
	arg.s = &s;
	arg.split = split;
	rbtree_iterator(ip, dump_addr_iterator, &arg);
}
void KIpMap::dump_addr(iteratorbt iterator, void *arg)
{
	rbtree_iterator(ip, iterator,arg);
}
bool test_ip_map()
{
	//printf("ips=[%s]\n",ips);
	KIpMap ipmap;
	ipmap.add_multi_addr("192.168.1.1-250.168.1.255",'|',(void *)1);
	//printf("success added item=[%d]\n",ipmap.add_multi_addr("::1-::255|2:4::/64",'|',(void *)2));
	//ipmap.dump_addr();
	//void *data = ipmap.find("192.168.2.3");
	//assert(data!=NULL);
	assert(ipmap.find("10.1.1.1")==NULL);
	assert(ipmap.find("::5")!=NULL);
	return true;
}
