/*
 * KTemplateVirtualHost.h
 *
 *  Created on: 2010-11-19
 *      Author: keengo
 */

#ifndef KTEMPLETEVIRTUALHOST_H_
#define KTEMPLETEVIRTUALHOST_H_
#include <vector>
#include <string.h>
#include "KVirtualHost.h"
#include "KExtendProgram.h"

class KTempleteVirtualHost: public KVirtualHost {
public:
	KTempleteVirtualHost();
	virtual ~KTempleteVirtualHost();
	bool isTemplete()
	{
		return true;
	}
	bool updateEvent(KVirtualHostEvent *ctx);
	bool initEvent(KVirtualHostEvent *ctx);
	bool destroyEvent(KVirtualHostEvent *ctx);
	void addEvent(const char *etag,std::map<std::string,std::string>&attribute);
	void buildXML(std::stringstream &s);
	std::string initEvents;
	std::string destroyEvents;
	std::string updateEvents;
};
/*
Ä£°å×é
*/
class KGTempleteVirtualHost
{
public:
	KGTempleteVirtualHost();
	~KGTempleteVirtualHost();
	KTempleteVirtualHost *findTemplete(const char *subname,bool remove);
	void add(const char *subname,KTempleteVirtualHost *tvh,bool defaultFlag = false);
	bool isEmpty()
	{
		if(tvhs.size()>0){
			return false;
		}
		return true;
	}
	void getAllTemplete(std::list<std::string> &vhs)
	{
		std::map<std::string,KTempleteVirtualHost *>::iterator it;
		for(it=tvhs.begin();it!=tvhs.end();it++){
			vhs.push_back((*it).first);
		}
	}
	bool del(const char *subname)
	{
		std::map<std::string,KTempleteVirtualHost *>::iterator it;
		it = tvhs.find(subname);
		if (it==tvhs.end()) {
			return false;
		}
		KTempleteVirtualHost *tvh = (*it).second;
		tvhs.erase(it);
		if (tvh==defaultTemplete) {
			if(tvhs.size()>0){
				defaultTemplete = (*tvhs.begin()).second;
			} else {
				defaultTemplete = NULL;
			}
		}
		tvh->destroy();
		return true;
	}
	std::map<std::string,KTempleteVirtualHost *> tvhs;
private:
	KTempleteVirtualHost *getDefaultTemplete();
	KTempleteVirtualHost *defaultTemplete;
};
#endif /* KTEMPLATEVIRTUALHOST_H_ */
