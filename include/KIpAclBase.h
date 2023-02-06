/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */
#ifndef KIPACLBASE_H_
#define KIPACLBASE_H_
#include "ksocket.h"
#include "KAcl.h"
#include "KTable.h"
#include "utils.h"
struct IP_MODEL {
	ip_addr addr;
	ip_addr mask;
	unsigned char mask_num;
};
class KIpAclBase: public KAcl {
public:
	static bool matchIpModel(IP_MODEL &ip,ip_addr &ipaddr)
	{
		if (ip.mask_num > 0) {
			ip_addr ret;
			ksocket_ipaddr_and(&ipaddr, &ip.mask, &ret);
			return ksocket_ipaddr_compare(&ip.addr, &ret) == 0;
		} else {
			return ksocket_ipaddr_compare(&ip.addr, &ipaddr) == 0;
		}
	}
	static bool addIpModel(const char *ip_model, IP_MODEL &m_ip) {
		memset(&m_ip, 0, sizeof(m_ip));
		std::vector<std::string> ip_mask;
		explode(ip_model, '/', ip_mask);
		if (ip_mask.size() <= 0) {
			//		printf("ip_mask.size=0\n");
			return false;
		}
		m_ip.mask_num = 0;
		bool result;
		if (ip_mask[0].size() > 0){
			sockaddr_i a;
			result = ksocket_getaddr(ip_mask[0].c_str(), 0,AF_UNSPEC,AI_NUMERICHOST,&a);
			ksocket_ipaddr(&a,&m_ip.addr);
		}
		if (ip_mask.size() > 1) {
			unsigned long default_mask = (unsigned long) 0xffffffff;
			memset(&m_ip.mask, 0, sizeof(m_ip.mask));
//			ip_addr *a;
			m_ip.mask_num = atoi(ip_mask[1].c_str());
			if (m_ip.mask_num > 0) {
				int mask_num = m_ip.mask_num;
#ifdef KSOCKET_IPV6
				if(m_ip.addr.sin_family==PF_INET6) {
					for(int i=0;i<4;i++) {
						if(mask_num<=0) {
							break;
						}
						if (mask_num >= 32) {
							m_ip.mask.addr32[i] = default_mask;
						} else {
							m_ip.mask.addr32[i] = ntohl(((default_mask >> (32
									- mask_num)) << (32 - mask_num)));
						}
						mask_num -= 32;
					}
					//printf("mask=%s.\n",KSocket::make_ip(&m_ip.mask,PF_INET6).c_str());
//					a = (ip_addr *) &m_ip.addr.v6.sin6_addr;
				} else {
					m_ip.mask.addr32[0] = ntohl(((default_mask >> (32
							- mask_num)) << (32 - mask_num)));
#else
					m_ip.mask=ntohl(((default_mask>>(32-mask_num))<<(32-mask_num)));
#endif
					//printf("mask=%s.\n",KSocket::make_ip(&m_ip.mask,PF_INET).c_str());
//					a = (ip_addr *) &m_ip.addr.v4.sin_addr;
#ifdef KSOCKET_IPV6
				}
#endif
				ksocket_ipaddr_and(&m_ip.addr, &m_ip.mask, &m_ip.addr);
			}
		}
		return result;
	}
	bool matchIP(ip_addr &ipaddr) {
		return KIpAclBase::matchIpModel(ip,ipaddr);
	}
	KIpAclBase() {
		memset(&ip, 0, sizeof(ip));

	}
	~KIpAclBase() {
	}
	std::string getDisplay() {
		std::stringstream s;
		char ips[MAXIPLEN];
		ksocket_ipaddr_ip(&ip.addr,ips,sizeof(ips));
		s << ips;
		if (ip.mask_num > 0) {
			s << "/" << (int) ip.mask_num;
		}
		return s.str();
	}

	bool startCharacter(KXmlContext *context, char *character, int len) override {
		//		if (context->getParentName()==ACL_CONTEXT && context->qName==getName()) {
		addIpModel(character, ip);
		return true;
		//		}
		//		return false;
	}

	void buildXML(std::stringstream &s) override  {
		s << ">" << getDisplay();
	}
protected:
	IP_MODEL ip;
};
bool addIpModel(const char *ip_model, IP_MODEL &m_ip);
#endif /*KIPACLBASE_H_*/
