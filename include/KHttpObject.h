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
#include "KHttpFieldValue.h"

#ifdef ENABLE_BIG_OBJECT_206
#include "KSharedBigObject.h"
#endif

#include "KHttpKeyValue.h"
#include "KHttpLib.h"
#include "time_utils.h"
#include "KFileName.h"
#include "KBuffer.h"
#include "khttp.h"

#define   LIST_IN_MEM   0
#define   LIST_IN_DISK  1
#define   LIST_IN_NONE  2

extern KMutex obj_lock[HASH_SIZE + 2];

class KHttpObjectHash;


#define MEMORY_OBJECT       0
#define BIG_OBJECT          1
#ifdef ENABLE_BIG_OBJECT_206
#define BIG_OBJECT_PROGRESS 2
#endif
#define SWAPING_OBJECT      3
inline void kgl_safe_copy_body_data(KHttpObjectBodyData* dst, KHttpObjectBodyData* src) {
	//must not override dst->type
	memcpy(dst, src, offsetof(KHttpObjectBodyData, type));
}
/**
 * httpobject的信息主体
 */
class KHttpObjectBody
{
public:
	KHttpObjectBody() {
		memset(this, 0, sizeof(KHttpObjectBody));
	}
	KHttpObjectBody(KHttpObjectBody* data);
	~KHttpObjectBody() {
		if (etag && !i.condition_is_time) {
			xfree(etag);
		}
		if (headers) {
			free_header_list(headers);
		}
		switch (i.type) {
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
#ifdef ENABLE_BIG_OBJECT_206
		case BIG_OBJECT_PROGRESS:
			if (sbo) {
				delete sbo;
			}
			break;
#endif
		case BIG_OBJECT:
			assert(bodys == NULL);
			break;
		default:
			assert(bodys == NULL);
			break;
		}
	}
#ifdef ENABLE_DISK_CACHE
	bool restore_header(KHttpObject* obj, char* buf, int len);
	void create_type(HttpObjectIndex* index);
#endif
	time_t get_last_modified() {
		if (!i.condition_is_time) {
			return 0;
		}
		return last_modified;
	}
	kgl_len_str_t* get_etag() {
		if (i.condition_is_time) {
			return nullptr;
		}
		return etag;
	}
	void set_last_modified(time_t last_modified);
	void set_etag(const char* val, size_t len);
	KHttpObjectBodyData i;
	union {
		kgl_len_str_t* etag;  /* etag为主，有etag就忽略last-modified */
		time_t last_modified;
	};
	KHttpHeader* headers; /* headers */
	union
	{
		kbuf* bodys;
#ifdef ENABLE_DISK_CACHE
		KHttpObjectSwaping* os;
#endif
#ifdef ENABLE_BIG_OBJECT_206
		KSharedBigObject* sbo;
#endif
	};
};
/**
 * http物件。例如网页之类,缓存对象
 */
class KHttpObject
{
public:
	friend class KHttpObjectHash;
	KHttpObject() {
		init(NULL);
	}
	KHttpObject(KHttpRequest* rq) {
		init(rq->sink->data.url);
		data = new KHttpObjectBody();
		KBIT_SET(index.flags, FLAG_IN_MEM);
	}
	KHttpObject(KHttpRequest* rq, KHttpObject* obj);
	void init(KUrl* url) {
		memset(&index, 0, sizeof(index));
		memset(&dk, 0, sizeof(dk));
		list_state = LIST_IN_NONE;
		runtime_flags = 0;
		index.last_verified = kgl_current_sec;
		if (url) {
			uk.url = url->refs();
		} else {
			uk.url = nullptr;
		}
		uk.vary = NULL;
		h = HASH_SIZE;
		refs = 1;
		data = NULL;
	}
	void Dead();
	void UpdateCache(KHttpObject* obj);
	bool IsContentEncoding() {
		return uk.url->encoding > 0;
	}
	void AddContentEncoding(u_char encoding, const char* val, hlen_t val_len) {
		uk.url->set_content_encoding(encoding);
		insert_http_header(kgl_header_content_encoding, val, val_len);
	}
	bool IsContentRangeComplete() {
		if (!KBIT_TEST(index.flags, ANSW_HAS_CONTENT_RANGE)) {
			return false;
		}
		assert(data);		
		return index.content_range_length == index.content_length;
	}
	inline char* getCharset() {
		if (data == NULL) {
			return NULL;
		}
		KHttpHeader* tmp = data->headers;
		while (tmp) {
			if (!tmp->name_is_know || tmp->know_header != kgl_header_content_type) {
				tmp = tmp->next;
				continue;
			}
			const char* p = kgl_memstr(tmp->buf + tmp->val_offset, tmp->val_len, _KS("charset="));
			if (p == NULL) {
				return NULL;
			}
			char* end = tmp->buf + tmp->val_offset + tmp->val_len;
			p += 8;
			while (p < end && IS_SPACE((unsigned char)*p)) {
				p++;
			}
			const char* charsetend = p;
			while (charsetend < end && !IS_SPACE((unsigned char)*charsetend) && *charsetend != ';') {
				charsetend++;
			}
			int charset_len = (int)(charsetend - p);
			char* charset = kgl_strndup(p, charset_len);
			return charset;
		}
		return NULL;
	}
	KMutex* getLock() {
		return &obj_lock[h];
	}
	int getRefs() {
		u_short hh = h;
		obj_lock[hh].Lock();
		int ret = refs;
		obj_lock[hh].Unlock();
		return ret;
	}
	KHttpHeader* find_header(const char* attr, int len) {
		KHttpHeader* h = data->headers;
		while (h) {
			if (kgl_is_attr(h, attr, len)) {
				return h;
			}
			h = h->next;
		}
		return NULL;
	}
	bool is_same_precondition(KHttpObject* obj);
	bool precondition_time(time_t time);
	bool precondition_entity(const char* entity, size_t len);
	bool match_if_range(const char* entity, size_t len);
	void addRef() {
		u_short hh = h;
		obj_lock[hh].Lock();
		refs++;
		obj_lock[hh].Unlock();
	}
	void release() {
		u_short hh = h;
		obj_lock[hh].Lock();
		assert(refs > 0);
		refs--;
		if (refs == 0) {
			obj_lock[hh].Unlock();
			delete this;
			return;
		}
		obj_lock[hh].Unlock();
	}
	unsigned get_current_age(time_t now_time) {
		return (unsigned)(now_time - index.last_verified);
	}
#ifdef ENABLE_FORCE_CACHE
	//强制缓存
	bool force_cache(bool static_flag) {
		if (KBIT_TEST(index.flags,FLAG_DEAD)) {
			return false;
		}
		KBIT_CLR(index.flags, ANSW_NO_CACHE);
		if (static_flag) {
			KBIT_CLR(index.flags, OBJ_MUST_REVALIDATE);
			data->set_last_modified(kgl_current_sec);
			KBIT_SET(index.flags, OBJ_IS_STATIC2);
		}
		return true;
	}
#endif
	bool isNoBody(KHttpRequest* rq) {
		if (this->checkNobody()) {
			return true;
		}
		return rq->sink->data.meth == METH_HEAD;
	}
	bool checkNobody() {
		if (is_status_code_no_body(data->i.status_code)) {
			KBIT_SET(index.flags, FLAG_NO_BODY);
			return true;
		}
		if (KBIT_TEST(index.flags, ANSW_XSENDFILE)) {
			KBIT_SET(index.flags, FLAG_NO_BODY);
			return true;
		}
		return false;
	}
	void CountSize(INT64& mem_size, INT64& disk_size, int& mem_count, int& disk_count) {
		if (KBIT_TEST(index.flags, FLAG_IN_MEM)) {
			mem_count++;
			mem_size += GetMemorySize();
		}
		if (KBIT_TEST(index.flags, FLAG_IN_DISK)) {
			disk_count++;
			disk_size += GetDiskSize();
		}
	}
	inline INT64 GetMemorySize() {
		INT64 size = GetHeaderSize();
		if (data && data->i.type == MEMORY_OBJECT) {
			size += index.content_length;
		}
		return size;
	}
	inline INT64 GetDiskSize() {
		return GetHeaderSize() + index.content_length;
	}
	int GetHeaderSize(int url_len = 0);
#ifdef ENABLE_DISK_CACHE
	bool swapout(KBufferFile* file, bool fast_model);
	void unlinkDiskFile();
	kgl_auto_cstr get_filename(bool part = false);
	void write_file_header(KHttpObjectFileHeader* fileHeader);
	bool save_header(KBufferFile* fp, const char* url, int url_len);
	char* build_aio_header(int& len, const char* url, int url_len);
	int build_header(char* pos, char* end, const char* url, int url_len);
#endif
	bool remove_http_header(const char* attr, int attr_len) {
		bool result = false;
		KHttpHeader* h = data->headers;
		KHttpHeader* last = NULL;
		while (h) {
			KHttpHeader* next = h->next;
			if (kgl_is_attr(h, attr, attr_len)) {
				if (last) {
					last->next = next;
				} else {
					data->headers = next;
				}
				xfree_header(h);
				h = next;
				result = true;
				continue;
			}
			last = h;
			h = next;
		}
		return result;
	}
	void insert_http_header(kgl_header_type type, const char* val, int val_len) {
		KHttpHeader* new_h = new_http_know_header(type, val, val_len);
		if (!new_h) {
			return;
		}
		new_h->next = data->headers;
		data->headers = new_h;
	}
	void insert_http_header(const char* attr, int attr_len, const char* val, int val_len) {
		KHttpHeader* new_h = new_http_header(attr, attr_len, val, val_len);
		new_h->next = data->headers;
		data->headers = new_h;
	}
	void ResponseVaryHeader(KHttpRequest* rq);
	bool AddVary(KHttpRequest* rq, const char* val, int val_len);
	int64_t getTotalContentSize() {
		if (KBIT_TEST(index.flags, ANSW_HAS_CONTENT_RANGE)) {
			return index.content_range_length;
		}
		return index.content_length;
	}
	KHttpObject* lnext; /* in list */
	KHttpObject* lprev; /* in list */
	KHttpObject* next;  /* in hash */
	/* list state
	 LIST_IN_NONE
	 LIST_IN_MEM
	 LIST_IN_DISK
	 */
	unsigned char list_state;
	union
	{
		struct
		{
			unsigned char in_cache : 1;
			unsigned char cache_is_ready : 1;
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
	KHttpObjectBody* data;
	KHttpObjectKey dk;
	HttpObjectIndex index;
private:
	~KHttpObject();
	kgl_auto_cstr BuildVary(KHttpRequest* rq);
};

#endif /*KHTTPOBJECT_H_*/
