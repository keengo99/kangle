#ifndef KUPLOADPROGRESSMARK_H
#define KUPLOADPROGRESSMARK_H
#include "KInputFilter.h"
#include "utils.h"
class KUploadProgressItem : public KRawInputFilter,public KDynamicString,public KCountableEx
{
public:
	KUploadProgressItem()
	{
		lnext = NULL;
		lprev = NULL;
		status = 0;
		complete = 0;
		total = 0;
		lastReportTime = 0;
	}
	void addRefFilter()
	{
		addRef();
	}
	void releaseFilter()
	{
		release();
	}
	bool buildValue(const char *name,KStringBuf *s)
	{
		if (strcasecmp(name,"status")==0) {
			*s << status;
			return true;
		}
		if (strcasecmp(name,"complete")==0) {
			*s << complete;
			return true;
		}
		if (strcasecmp(name,"total")==0) {
			*s << total;
			return true;
		}
		if (strcasecmp(name,"progress")==0) {
			if (total==0) {
				*s << 0;
				return true;
			}
			int progress = (int)((complete * 100) / total);
			*s << progress;
			return true;
		}
		if (strcasecmp(name,"ugid")==0) {
			*s << ugid.c_str();
			return true;
		}
		return false;
	}
	int check(KInputFilterContext *rq,const char *str,int len,bool isLast)
	{
		complete += len;
		if (isLast) {
			status = 3;
		} else {
			status = 2;
		}
		return JUMP_ALLOW;
	}
	time_t lastReportTime;
	std::string ugid;
	INT64 total;
	INT64 complete;
	int status;
	KUploadProgressItem *lprev;
	KUploadProgressItem *lnext;
};
class KUploadProgressId : public KParamFilterHook
{
public:
	bool matchFilter(KInputFilterContext *rq,const char *name,int name_len,const char *value,int value_len)
	{
		if ((int)this->name.size()!=name_len) {
			return false;
		}
		if (memcmp(this->name.c_str(),name,name_len)==0) {
			id = value;
			return true;
		}
		return false;
	}
	std::string name;
	std::string id;
};
/**
* 上传进度
*/
#if 0
class KUploadProgressMark :public KMark
{
public:
	KUploadProgressMark()
	{
		cleanTime = 30;
		head = NULL;
		end = NULL;
	}
	~KUploadProgressMark()
	{
		while (head) {
			KUploadProgressItem *item = head;
			head = head->lnext;
			item->release();
		}
	}
	bool supportRuntime()
	{
		return true;
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj,
			const int chainJumpType, int &jumpType)
	{
		if (rq->sink->data.meth == METH_GET && strcmp(rq->sink->data.url->path,reportPath.c_str())==0) {
			//report
			return reportProgress(rq,jumpType);
		}
		if (KBIT_TEST(rq->sink->data.flags,RQ_POST_UPLOAD)) {
			return trackProgress(rq);
		}
		return true;
	}
	bool trackProgress(KHttpRequest *rq)
	{
		KUploadProgressId *u = getUploadProgressId(rq);
		if (u==NULL) {
			return false;
		}
		KUploadProgressItem *item = new KUploadProgressItem;
		item->ugid = u->id;
		item->total = rq->sink->data.content_length;
		item->complete = 0;
		lock.Lock();
		bool result = add(item);
		lock.Unlock();
		if (result) {
			KInputFilterContext *if_ctx = rq->getInputFilterContext();
			if (if_ctx) {
				if_ctx->registerRawFilter(item);
			}
		}
		item->release();
		delete u;
		return result;
	}
	bool reportProgressItem(KHttpRequest *rq,KUploadProgressItem *item,int &jumpType)
	{
		char *buf = item->parseString(report.c_str());
		if (buf) {
			KBuffer s;
			int body_length = strlen(buf);
			KBIT_SET(rq->sink->data.flags,RQ_HAS_SEND_HEADER);
			s << getRequestLine(200);
			//{{ent
#ifdef KANGLE_ENT
			s.WSTR("Server: ");
			s.write_all(conf.serverName,conf.serverNameLength);
			s.WSTR("\r\nDate: ");
#else
			//}}
			s.WSTR("Server: " PROGRAM_NAME "/" VERSION "\r\nDate: ");
			//{{ent
#endif
			//}}
			timeLock.Lock();
			s.write_all((char *)cachedDateTime,29);
			timeLock.Unlock();
			s << "\r\n";
			s << "Cache-Control: no-cache,no-store\r\n";
			if (contentType.size()>0) {
				s << "Content-Type: " << contentType.c_str() << "\r\n";
			}
			s << "Connection: ";
			if (KBIT_TEST(rq->sink->data.flags,RQ_CONNECTION_CLOSE) || !KBIT_TEST(rq->sink->data.flags,RQ_HAS_KEEP_CONNECTION)) {
				s << "close\r\n";
			} else {
				s << "keep-alive\r\n";
				s << "Keep-Alive: timeout=" << conf.keep_alive << "\r\n";
				s << "Content-Length: " << body_length << "\r\n";				
			}
			s << "\r\n";
			rq->addSendHeader(&s);
			rq->buffer.write_direct(buf,body_length);
			jumpType = JUMP_DENY;
			return true;
		}
		return false;
	}
	bool reportProgress(KHttpRequest *rq,int &jumpType)
	{
		KUploadProgressItem *item = NULL;
		KUploadProgressId *u = getUploadProgressId(rq);
		if (u==NULL) {
			item = new KUploadProgressItem;
			bool result = reportProgressItem(rq,item,jumpType);
			item->release();
			return result;
		}
		lock.Lock();
		std::map<std::string,KUploadProgressItem *>::iterator it;
		it = ugs.find(u->id);
		if (it!=ugs.end()) {
			item = (*it).second;
			if (item->status!=3) {
				removeList(item);
				addList(item);
			}
			item->addRef();
		}
		flush();
		lock.Unlock();
		if (item == NULL) {
			item = new KUploadProgressItem;
			item->ugid = u->id;
		}
		bool result = reportProgressItem(rq,item,jumpType);
		item->release();
		delete u;
		return result;
	}
	KUploadProgressId *getUploadProgressId(KHttpRequest *rq)
	{
		KUploadProgressId *u = new KUploadProgressId;
		u->name = ugid;
		KInputFilterContext *if_ctx = rq->getInputFilterContext();
		if (if_ctx) {
			if_ctx->checkGetParam(u);
		}
		if (u->id.size()>0) {
			return u;
		}
		delete u;
		return NULL;
	}
	KMark *newInstance()
	{
		return new KUploadProgressMark;
	}
	const char *getName()
	{
		return "upload_progress";
	}
	std::string getDisplay()
	{
		std::stringstream s;
		s << "ugid:" << ugid << " report_path:" << reportPath;
		return s.str();
	}
	std::string getHtml(KModel *model)
	{
		std::stringstream s;
		KUploadProgressMark *m = (KUploadProgressMark *)model;
		s << "ugid:<input name='ugid' value='" << (m?m->ugid:"") << "'><br>";
		s << "clean time:<input name='clean_time' value='" << (m?m->cleanTime:30) << "'><br>";
		s << "report path:<input name='report_path' value='" << (m?m->reportPath:"") << "'><br>";
		s << "content type:<input name='content_type' value='" << (m?m->contentType:"") << "'><br>";
		s << "report: <textarea name='report'>" << (m?m->report:"") << "</textarea>";
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute,bool html)
			
	{
		ugid = attribute["ugid"];
		reportPath = attribute["report_path"];
		contentType = attribute["content_type"];
		cleanTime = atoi(attribute["clean_time"].c_str());
		report = attribute["report"];
	}
	void buildXML(std::stringstream &s)
	{
		s << " ugid='" << ugid << "'";
		s << " report_path='" << reportPath << "'";
		s << " content_type='" << contentType << "'";
		s << " clean_time='" << cleanTime << "'";
		s << " report='" << KXml::param(report.c_str()) << "'";
		s << ">";
	}
private:
	void flush()
	{
		time_t nowTime = kgl_current_sec;
		while (head && nowTime - head->lastReportTime > cleanTime) {
			std::map<std::string,KUploadProgressItem *>::iterator it;
			it = ugs.find(head->ugid);
			assert(it!=ugs.end());
			ugs.erase(it);
			KUploadProgressItem *item = head;
			head = head->lnext;
			item->release();
			if (head==NULL) {
				end = NULL;
			}
		}
	}
	void removeList(KUploadProgressItem *item)
	{
		if (item == head) {
			head = head->lnext;
		}
		if(item == end){
			end = end->lprev;
		}
		if(item->lprev){
			item->lprev->lnext = item->lnext;
		}
		if(item->lnext){
			item->lnext->lprev = item->lprev;
		}
		item->lnext = NULL;
		item->lprev = NULL;
	}
	void addList(KUploadProgressItem *item)
	{
		item->lastReportTime = kgl_current_sec;
		item->lnext = NULL;
		item->lprev = end;
		if (end) {
			end->lnext = item;
		} else {
			head = item;
		}
		end = item;
	}
	bool add(KUploadProgressItem *item)
	{
		item->status = 1;
		std::map<std::string,KUploadProgressItem *>::iterator it;
		it = ugs.find(item->ugid);
		if (it!=ugs.end()) {
			return false;
		}
		ugs.insert(std::pair<std::string,KUploadProgressItem *>(item->ugid,item));
		addList(item);
		item->addRef();
		return true;
	}
	std::string ugid;
	std::string reportPath;
	std::string contentType;
	std::string report;
	int cleanTime;
	std::map<std::string,KUploadProgressItem *> ugs;
	KUploadProgressItem *head;
	KUploadProgressItem *end;
	KMutex lock;
};
#endif
#endif
