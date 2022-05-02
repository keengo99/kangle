#ifndef KHTTPOBJECT_H_
#define KHTTPOBJECT_H_

#include "KMutex.h"
#include "kforwin32.h"
#include "KBuffer.h"
#include "log.h"
#include "KHttpKeyValue.h"
#include "KDiskCache.h"
#include "KUrl.h"
#include "KHttpHeader.h"
#include "KHttpRequest.h"
#include "KHttpObjectSwaping.h"
#include "KVary.h"
//{{ent
#ifdef ENABLE_BIG_OBJECT_206
#include "KSharedBigObject.h"
#endif
//}}
#include "KHttpKeyValue.h"
#include "time_utils.h"
#include "KFileName.h"
#include "KBuffer.h"
#include "khttp.h"

#define   LIST_IN_MEM   0
#define   LIST_IN_DISK  1
#define   LIST_IN_NONE  2

extern KMutex obj_lock[HASH_SIZE+2];

class KHttpObjectHash;


#define MEMORY_OBJECT       0
#define BIG_OBJECT          1
#ifdef ENABLE_BIG_OBJECT_206
#define BIG_OBJECT_PROGRESS 2
#endif
#define SWAPING_OBJECT      3
/**
 * httpobject的信息主体
 */
class KHttpObjectBody {
public:
	KHttpObjectBody() {
		memset(this, 0, sizeof(KHttpObjectBody));
	}
	KHttpObjectBody(KHttpObjectBody *data);
	~KHttpObjectBody() {
		if (headers) {
			free_header_list(headers);
		}
		switch(type){
		case MEMORY_OBJECT:
			if (bodys) {
				destroy_kbuf(bodys);
			}
			break;
#ifdef ENABLE_DISK_CACHE
		case SWAPING_OBJECT:
			if (os) {
				delete os;
			}
			break;
#endif
		//{{ent
#ifdef ENABLE_BIG_OBJECT_206
		case BIG_OBJECT_PROGRESS:
			if (sbo) {
				delete sbo;
			}
			break;
#endif//}}
		case BIG_OBJECT:
			assert(bodys == NULL);
			break;
		default:
			assert(bodys == NULL);
			break;
		}
	}
#ifdef ENABLE_DISK_CACHE
	bool restore_header(KHttpObject *obj,char *buf, int len);
	void create_type(HttpObjectIndex *index);
#endif
	unsigned short status_code;
	unsigned short type;
	KHttpHeader *headers; /* headers */
	union {
		kbuf *bodys;
#ifdef ENABLE_DISK_CACHE
		KHttpObjectSwaping *os;
#endif
		//{{ent
#ifdef ENABLE_BIG_OBJECT_206
		KSharedBigObject *sbo;
#endif//}}
	};
};
inline bool is_status_code_no_body(int status_code) {
		if (status_code == 100
			|| status_code == STATUS_NOT_MODIFIED
			|| status_code == STATUS_NO_CONTENT) {
			//no content,see rfc2616.
			return true;
		}
		return false;
}
inline bool status_code_can_cache(u_short code) {
	switch (code) {
	case STATUS_OK:
			//目前仅支持200
			return true;
	default:
			return false;
	}
}
/**
 * http物件。例如网页之类,缓存对象
 */
class KHttpObject {
public:
	friend class KHttpObjectHash;
	KHttpObject() {
		init(NULL);
	}
	KHttpObject(KHttpRequest *rq) {		
		init(rq->sink->data.url);
		data = new KHttpObjectBody();	
		KBIT_SET(index.flags,FLAG_IN_MEM);
	}
	KHttpObject(KHttpRequest *rq,KHttpObject *obj);
	void init(KUrl *url) {
		memset(&index,0,sizeof(index));
		memset(&dk, 0, sizeof(dk));
		list_state = LIST_IN_NONE;
		runtime_flags = 0;
		index.last_verified = kgl_current_sec;
		uk.url = url;
		uk.vary = NULL;
		h = HASH_SIZE;
		refs = 1;
		data = NULL;
	}
	void Dead();
	void UpdateCache(KHttpObject *obj);
	bool IsContentEncoding() {
		return uk.url->encoding > 0;
	}
	void AddContentEncoding(u_char encoding,const char *val, hlen_t val_len)
	{
		uk.url->set_content_encoding(encoding);
		insertHttpHeader(kgl_expand_string("Content-Encoding"), val, val_len);
	}
	bool IsContentRangeComplete(KHttpRequest *rq)
	{
		if (!KBIT_TEST(index.flags, ANSW_HAS_CONTENT_RANGE)) {
			return false;
		}
		return rq->ctx->content_range_length==index.content_length;
	}
	inline char *getCharset()
	{
		if (data==NULL) {
			return NULL;
		}
		KHttpHeader *tmp = data->headers;
		while (tmp){
			if (strcasecmp(tmp->attr, "Content-Type") != 0) {
				tmp = tmp->next;
				continue;
			}
			const char *p = strstr(tmp->val, "charset=");
			if (p == NULL) {
				return NULL;
			}
			p += 8;
			while (*p && IS_SPACE((unsigned char)*p))
				p++;
			const char *charsetend = p;
			while (*charsetend && !IS_SPACE((unsigned char)*charsetend)
					&& *charsetend != ';')
				charsetend++;
			int charset_len = (int)(charsetend - p);
			char *charset = (char *)malloc(charset_len+1);
			kgl_memcpy(charset,p,charset_len);
			charset[charset_len] = '\0';
			return charset;
		}
		return NULL;
	}
	KMutex *getLock()
	{
		return &obj_lock[h];
	}
	int getRefs() {
		u_short hh = h;
		obj_lock[hh].Lock();
		int ret = refs;
		obj_lock[hh].Unlock();
		return ret;
	}
	KHttpHeader *findHeader(const char *attr,int len) {
		KHttpHeader *h = data->headers;
		while (h) {
			if (is_attr(h,attr,len)) {
				return h;
			}
			h = h->next;			
		}
		return NULL;
	}
	bool matchEtag(const char *if_none_match,int len) {
		if (!KBIT_TEST(index.flags,OBJ_HAS_ETAG)) {
			return false;
		}
		if (data==NULL) {
			return false;
		}
		KHttpHeader *h = findHeader("Etag",sizeof("Etag")-1);
		if (h==NULL || len!=h->val_len) {
			return false;
		}
		return memcmp(if_none_match,h->val,len)==0;
	}
	void addRef() {
		u_short hh = h;
		obj_lock[hh].Lock();
		refs++;
		obj_lock[hh].Unlock();
	}
	void release()
	{
		u_short hh = h;
		obj_lock[hh].Lock();
		assert(refs>0);
		refs--;
		if (refs==0) {
			obj_lock[hh].Unlock();
			delete this;
			return;
		}
		obj_lock[hh].Unlock();
	}
	unsigned getCurrentAge(time_t nowTime) {	
		return (unsigned) (nowTime - index.last_verified);
	}
#ifdef ENABLE_FORCE_CACHE
	//强制缓存
	bool force_cache(bool insertLastModified=true)
	{
		if (!status_code_can_cache(data->status_code)) {
			return false;
		}
		KBIT_CLR(index.flags,ANSW_NO_CACHE|OBJ_MUST_REVALIDATE);
		if (!KBIT_TEST(index.flags,ANSW_LAST_MODIFIED|OBJ_HAS_ETAG)) {
			index.last_modified = kgl_current_sec;
			if (insertLastModified) {
				char *tmp_buf = (char *)malloc(41);
				memset(tmp_buf, 0, 41);
				mk1123time(index.last_modified, tmp_buf, 41);
				insertHttpHeader2(strdup("Last-Modified"),sizeof("Last-Modified")-1,tmp_buf,29);
			}
			KBIT_SET(index.flags,ANSW_LAST_MODIFIED);
		}
		KBIT_SET(index.flags,OBJ_IS_STATIC2);
		return true;
	}
#endif
	bool isNoBody(KHttpRequest *rq) {
		if (this->checkNobody()) {
			return true;
		}
		return rq->sink->data.meth == METH_HEAD;
	}
	bool checkNobody() {
		if (is_status_code_no_body(data->status_code)) {
			KBIT_SET(index.flags,FLAG_NO_BODY);
			return true;
		}
		if (KBIT_TEST(index.flags,ANSW_XSENDFILE)) {
			KBIT_SET(index.flags,FLAG_NO_BODY);
			return true;
		}
		return false;
	}
	void CountSize(INT64 &mem_size,INT64 &disk_size,int &mem_count,int &disk_count)
	{
		if (KBIT_TEST(index.flags,FLAG_IN_MEM)) {
			mem_count++;
			mem_size += GetMemorySize();
		}
		if (KBIT_TEST(index.flags,FLAG_IN_DISK)) {
			disk_count++;
			disk_size += GetDiskSize();			
		}
	}
	inline INT64 GetMemorySize()
	{
		INT64 size = GetHeaderSize();
		if (data && data->type == MEMORY_OBJECT) {
			size += index.content_length;
		}
		return size;
	}
	inline INT64 GetDiskSize()
	{
		return GetHeaderSize() + index.content_length;
	}
	int GetHeaderSize(int url_len=0);
#ifdef ENABLE_DISK_CACHE
	bool swapout(KBufferFile *file,bool fast_model);
	//bool swapin(KHttpObjectBody *data);
	//bool swapinBody(KFile *fp, KHttpObjectBody *data);
	void unlinkDiskFile();
	char *getFileName(bool part=false);
	void write_file_header(KHttpObjectFileHeader *fileHeader);
	bool save_header(KBufferFile *fp,const char *url, int url_len);
	char *build_aio_header(int &len);
	bool save_dci_header(KBufferFile *fp);
#endif
	bool removeHttpHeader(const char *attr)
	{
		bool result = false;
		KHttpHeader *h = data->headers;
		KHttpHeader *last = NULL;
		while (h) {
			KHttpHeader *next = h->next;
			if (strcasecmp(h->attr,attr)==0) {
				if (last) {
					last->next = next;
				} else {
					data->headers = next;
				}
				free(h->attr);
				free(h->val);
				free(h);
				h = next;
				result = true;
				continue;
			}
			last = h;
			h = next;
		}
		return result;
	}
	void insertHttpHeader2(char *attr,int attr_len,char *val,int val_len)
	{
		KHttpHeader *new_h = (KHttpHeader *) xmalloc(sizeof(KHttpHeader));
		new_h->attr = attr;
		new_h->attr_len = attr_len;
		new_h->val = val;
		new_h->val_len = val_len;
		new_h->next = data->headers;
		data->headers = new_h;
	}
	void insertHttpHeader(const char *attr,int attr_len, const char *val,int val_len) {
		insertHttpHeader2(xstrdup(attr),attr_len,xstrdup(val),val_len);
	}
	void ResponseVaryHeader(KHttpRequest *rq);
	bool AddVary(KHttpRequest *rq,const char *val,int val_len);
	INT64 getTotalContentSize(KHttpRequest *rq)
	{
		if (KBIT_TEST(index.flags,ANSW_HAS_CONTENT_RANGE)) {
			return rq->ctx->content_range_length;
		}
		return index.content_length;
	}
	KHttpObject *lnext; /* in list */
	KHttpObject *lprev; /* in list */
	KHttpObject *next;  /* in hash */	
	/* list state
	 LIST_IN_NONE
	 LIST_IN_MEM
	 LIST_IN_DISK
	 */
	unsigned char list_state;
	union {
		struct {
			unsigned char in_cache : 1;
			unsigned char dc_index_update : 1;//文件index更新
			unsigned char us_ok_dead : 1;
			unsigned char us_err_dead : 1;
			unsigned char need_compress : 1;
			unsigned char never_compress : 1;
		};
		unsigned char runtime_flags;
	};
	short h; /* hash value */
	int refs;
	KUrlKey uk;
	KHttpObjectBody *data;
	KHttpObjectKey dk;
	HttpObjectIndex index;
private:
	~KHttpObject();
	char *BuildVary(KHttpRequest *rq);
};

#endif /*KHTTPOBJECT_H_*/
