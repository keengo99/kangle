#ifndef KDiskCache_h_0s9dflk1j231hhhh1
#define KDiskCache_h_0s9dflk1j231hhhh1
#include "global.h"
#include "kforwin32.h"
#include "KHttpRequest.h"
#include "KFileName.h"
#include <stdio.h>
#include "kasync_file.h"
#include <list>
#define CACHE_DISK_VERSION  4
#define CACHE_FIX_STR      "HXJW"
#pragma pack(push,4)
struct KHttpObjectKey
{
	unsigned filename1;//从kgl_current_sec得到
	unsigned filename2;//每次累加
};
struct HttpObjectIndex
{
	uint32_t head_size;
	uint32_t flags;
	int64_t  content_length; /* obj content_length */
	union {
		/*
		* last_verified used in cache object.
		* content-range never in cache object. 
		*/
		time_t last_verified;
		int64_t content_range_length;
	};
};
struct KHttpObjectDbIndex
{
	uint32_t url_flag_encoding;	
	HttpObjectIndex index;
};
struct KHttpObjectBodyData
{
	time_t last_modified;	
	uint32_t max_age;
	uint16_t status_code;
	uint16_t type;
};
struct KHttpObjectFileHeader
{
	u_char fix_str[4];	
	uint8_t version;
	uint8_t body_complete;
	u_char reverv[2];
	KHttpObjectDbIndex dbi;
	KHttpObjectBodyData body;
};
#pragma pack(pop)
struct index_scan_state_t
{
	int first_index;
	int second_index;
	int need_index_progress;
	time_t last_scan_time;
};
inline bool is_valide_dc_head_size(unsigned head_size)
{
	return head_size >= sizeof(KHttpObjectFileHeader) && head_size < 4048576;
}
inline bool is_valide_dc_sign(KHttpObjectFileHeader *header)
{
	return memcmp(header->fix_str, CACHE_FIX_STR, sizeof(header->fix_str)) == 0 &&
		header->version == CACHE_DISK_VERSION;
}
bool kgl_dc_skip_string(char **hot,int &hotlen);
char *kgl_dc_read_string(char **hot,int &hotlen,int &len);
bool kgl_dc_skip_string(KFile *file);
int kgl_dc_write_string(KBufferFile *fp,const char *str,int len=0);
int kgl_dc_write_string(char *hot, const char *str, int len);
char *getCacheIndexFile();
class KHttpObjectBody;
bool read_obj_head(KHttpObjectBody *data,KFile *fp);
bool read_obj_head(KHttpObjectBody *data,char **hot,int &hotlen);
void scan_disk_cache();
void get_disk_base_dir(KStringBuf &s);
bool get_disk_size(INT64 &total_size, INT64 &free_size);
INT64 get_need_free_disk_size(int used_radio);
void init_disk_cache(bool firstTime);
int get_index_scan_progress();
bool save_index_scan_state();
bool load_index_scan_state();
void rescan_disk_cache();
enum cor_result
{
	cor_failed,
	cor_success,
	cor_incache,
};
bool obj_can_disk_cache(KHttpRequest *rq, KHttpObject *obj);
cor_result create_http_object(KHttpObject *obj,const char *url, uint32_t flag_encoding,const char *verified_filename=NULL);
cor_result create_http_object2(KHttpObject *obj, char *url, uint32_t flag_encoding, const char *verified_filename = NULL);
extern volatile bool index_progress;
extern index_scan_state_t index_scan_state;
#endif
