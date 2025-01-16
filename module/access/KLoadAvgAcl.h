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
#ifndef KLOADAVGACL_H_
#define KLOADAVGACL_H_

#include "KAcl.h"
class KLoadAvgAcl : public KAcl {
public:
	KLoadAvgAcl() {
		loadavg=0;
		maxavg=0;
		lastRead=0;
	}
	virtual ~KLoadAvgAcl() {
	}
	void get_html(KWStream& s) override {
		s << "&gt;<input name=maxavg value='";
		KLoadAvgAcl *acl=(KLoadAvgAcl *)(this);
		if (acl) {
			s << acl->maxavg;
		}
		s << "'>";
	}
	KAcl *new_instance() override {
		return new KLoadAvgAcl();
	}
	const char *get_module() const override {
		return "loadavg";
	}
	bool match(KHttpRequest* rq, KHttpObject* obj) override {
		readAvg();
		if(loadavg>maxavg){
			return true;
		}
		return false;
	}
	void get_display(KWStream& s) override {
		s << "&gt;" << maxavg << "(cur:" << loadavg << ")";
	}
	void parse_config(const khttpd::KXmlNodeBody* xml) override {
		auto attribute = xml->attr();
		maxavg = atoi(attribute["maxavg"].c_str());
	}
	void readAvg()
	{
		time_t curTime=kgl_current_sec;
		if(curTime-lastRead>5){
			lastRead=curTime;
			FILE *fp=fopen("/proc/loadavg","rt");
			if(fp==NULL){
				fprintf(stderr,"cann't open /proc/loadavg file");
				return;
			}
			char buf[32];
			memset(buf,0,sizeof(buf));
			fread(buf,1,sizeof(buf)-1,fp);
			loadavg=atoi(buf);
			fclose(fp);
		}
	}
private:
	int loadavg;
	int maxavg;
	time_t lastRead;
};


#endif /*KLOADAVGACL_H_*/
