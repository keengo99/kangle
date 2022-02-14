#ifndef KGL_HTTP2_H
#define KGL_HTTP2_H
#include "global.h"
#include "KReadWriteBuffer.h"
#include "KBuffer.h"
#include "kforwin32.h"
#include "klist.h"
#include "KSendBuffer.h"
#include "KCondWait.h"
#include "KResponseContext.h"
#include "KHttp2WriteBuffer.h"
#include "KHttpHeader.h"
#include "KMutex.h"
#include "kconnection.h"
#include "kmalloc.h"
#ifdef ENABLE_HTTP2
#define MAX_HTTP2_BUFFER_SIZE 16
#define  KGL_SUCCESS     0
#define  KGL_ERROR      -1
#define  KGL_AGAIN      -2
#define  KGL_BUSY       -3
#define  KGL_DONE       -4
#define  KGL_DECLINED   -5
#define  KGL_ABORT      -6
class KHttpRequest;
class KAsyncFetchObject;
#ifdef TLSEXT_TYPE_next_proto_neg
#define KGL_HTTP_V2_NPN_NEGOTIATED       "h2"
#define KGL_HTTP_V2_NPN_ADVERTISE        "\x02h2"
#endif
#define KGL_HTTP_V2_STATE_BUFFER_SIZE    16

#define KGL_HTTP_V2_MAX_FRAME_SIZE       ((1 << 24) - 1)

#define KGL_HTTP_V2_INT_OCTETS           4
#define KGL_HTTP_V2_MAX_FIELD                                                 \
    (127 + (1 << (KGL_HTTP_V2_INT_OCTETS - 1) * 7) - 1)

#define KGL_HTTP_V2_DATA_DISCARD         1
#define KGL_HTTP_V2_DATA_ERROR           2
#define KGL_HTTP_V2_DATA_INTERNAL_ERROR  3

#define KGL_HTTP_V2_FRAME_HEADER_SIZE    9

/* frame types */
#define KGL_HTTP_V2_DATA_FRAME           0x0
#define KGL_HTTP_V2_HEADERS_FRAME        0x1
#define KGL_HTTP_V2_PRIORITY_FRAME       0x2
#define KGL_HTTP_V2_RST_STREAM_FRAME     0x3
#define KGL_HTTP_V2_SETTINGS_FRAME       0x4
#define KGL_HTTP_V2_PUSH_PROMISE_FRAME   0x5
#define KGL_HTTP_V2_PING_FRAME           0x6
#define KGL_HTTP_V2_GOAWAY_FRAME         0x7
#define KGL_HTTP_V2_WINDOW_UPDATE_FRAME  0x8
#define KGL_HTTP_V2_CONTINUATION_FRAME   0x9

/* frame flags */
#define KGL_HTTP_V2_NO_FLAG              0x00
#define KGL_HTTP_V2_ACK_FLAG             0x01
#define KGL_HTTP_V2_END_STREAM_FLAG      0x01
#define KGL_HTTP_V2_END_HEADERS_FLAG     0x04
#define KGL_HTTP_V2_PADDED_FLAG          0x08
#define KGL_HTTP_V2_PRIORITY_FLAG        0x20

#define KGL_HTTP_V2_DEFAULT_MAX_STREAM   64
#define kgl_http_v2_parse_uint16(p)  ntohs(*(uint16_t *) (p))
#define kgl_http_v2_parse_uint32(p)  ntohl(*(uint32_t *) (p))
#define kgl_http_v2_prefix(bits)  ((1 << (bits)) - 1)
#define KGL_HTTP_V2_STREAM_INDEX_MASK 0xF
#define kgl_http_v2_index_size() (KGL_HTTP_V2_STREAM_INDEX_MASK+1)
#define kgl_http_v2_index(sid)  ((sid >> 1) & KGL_HTTP_V2_STREAM_INDEX_MASK)

#define kgl_http_v2_parse_length(p)  ((p) >> 8)
#define kgl_http_v2_parse_type(p)    ((p) & 0xff)
#define kgl_http_v2_parse_sid(p)     (kgl_http_v2_parse_uint32(p) & 0x7fffffff)
#define kgl_http_v2_parse_window(p)  (kgl_http_v2_parse_uint32(p) & 0x7fffffff)
bool kgl_http_v2_huff_decode(u_char *state, u_char *src, size_t len, u_char **dst, uintptr_t last);

//about h2 write
#define kgl_http_v2_indexed(i)      (128 + (i))
#define kgl_http_v2_inc_indexed(i)  (64 + (i))
#define KGL_HTTP_V2_ENCODE_RAW            0
#define KGL_HTTP_V2_ENCODE_HUFF           0x80

#define KGL_HTTP_V2_STATUS_INDEX          8
#define KGL_HTTP_V2_STATUS_200_INDEX      8
#define KGL_HTTP_V2_STATUS_204_INDEX      9
#define KGL_HTTP_V2_STATUS_206_INDEX      10
#define KGL_HTTP_V2_STATUS_304_INDEX      11
#define KGL_HTTP_V2_STATUS_400_INDEX      12
#define KGL_HTTP_V2_STATUS_404_INDEX      13
#define KGL_HTTP_V2_STATUS_500_INDEX      14

#define KGL_HTTP_V2_CONTENT_LENGTH_INDEX  28
#define KGL_HTTP_V2_CONTENT_TYPE_INDEX    31
#define KGL_HTTP_V2_DATE_INDEX            33
#define KGL_HTTP_V2_LAST_MODIFIED_INDEX   44
#define KGL_HTTP_V2_LOCATION_INDEX        46
#define KGL_HTTP_V2_SERVER_INDEX          54
#define KGL_HTTP_V2_VARY_INDEX            59


/* errors */
#define KGL_HTTP_V2_NO_ERROR                     0x0
#define KGL_HTTP_V2_PROTOCOL_ERROR               0x1
#define KGL_HTTP_V2_INTERNAL_ERROR               0x2
#define KGL_HTTP_V2_FLOW_CTRL_ERROR              0x3
#define KGL_HTTP_V2_SETTINGS_TIMEOUT             0x4
#define KGL_HTTP_V2_STREAM_CLOSED                0x5
#define KGL_HTTP_V2_SIZE_ERROR                   0x6
#define KGL_HTTP_V2_REFUSED_STREAM               0x7
#define KGL_HTTP_V2_CANCEL                       0x8
#define KGL_HTTP_V2_COMP_ERROR                   0x9
#define KGL_HTTP_V2_CONNECT_ERROR                0xa
#define KGL_HTTP_V2_ENHANCE_YOUR_CALM            0xb
#define KGL_HTTP_V2_INADEQUATE_SECURITY          0xc
#define KGL_HTTP_V2_HTTP_1_1_REQUIRED            0xd

/* frame sizes */
#define KGL_HTTP_V2_RST_STREAM_SIZE              4
#define KGL_HTTP_V2_PRIORITY_SIZE                5
#define KGL_HTTP_V2_PING_SIZE                    8
#define KGL_HTTP_V2_GOAWAY_SIZE                  8
#define KGL_HTTP_V2_WINDOW_UPDATE_SIZE           4

#define KGL_HTTP_V2_STREAM_ID_SIZE               4

#define KGL_HTTP_V2_SETTINGS_PARAM_SIZE          6

/* settings fields */
#define KGL_HTTP_V2_HEADER_TABLE_SIZE_SETTING    0x1
#define KGL_HTTP_V2_MAX_STREAMS_SETTING          0x3
#define KGL_HTTP_V2_INIT_WINDOW_SIZE_SETTING     0x4
#define KGL_HTTP_V2_MAX_FRAME_SIZE_SETTING       0x5

#define KGL_HTTP_V2_FRAME_BUFFER_SIZE            24

#define KGL_HTTP_V2_DEFAULT_FRAME_SIZE           (1 << 14)

#define KGL_HTTP_V2_MAX_WINDOW                   ((1U << 31) - 1)
#define KGL_HTTP_V2_DEFAULT_WINDOW               65535

#define KGL_HTTP_V2_STREAM_RECV_WINDOW          262143
#define KGL_HTTP_V2_CONNECTION_RECV_WINDOW      KGL_HTTP_V2_MAX_WINDOW
#define KGL_HTTP_V2_ROOT                         (KHttp2Node *) -1
#pragma pack(push,1)
#define IS_WRITE_WAIT_FOR_WINDOW(we)	(we->len<0 && we->buf!=NULL)
#define IS_WRITE_WAIT_FOR_HUP(we)		(we->buf==NULL)
#define IS_WRITE_WAIT_FOR_WRITING(we)	(we->len>=0 && we->buf!=NULL)


struct http2_frame_header
{
	uint32_t length_type;
	uint8_t flags;
	uint32_t stream_id;
	int get_type()
	{
		return (length_type & 0xff);
	}
	int get_length()
	{
		return (length_type >> 8);
	}
	void set_length_type(int length,int type) {
		length_type = (length << 8) | type;
		assert(get_type() == type);
		assert(get_length() == length);
		length_type = htonl(length_type);
	}
};
struct http2_frame_setting {
	uint16_t id;
	uint32_t value;
};
struct http2_frame_window_update {
	uint32_t inc_size;
};
struct http2_frame_rst_stream {
	uint32_t status;
};
struct http2_frame_ping {
	uint64_t opaque;
};
struct http2_frame_goaway {
	uint32_t last_stream_id;
	uint32_t error_code;
};
#pragma pack(pop)
class KHttp2Context;
class KHttp2;
bool test_http2();
typedef int (*http2_header_parser_pt)(KHttp2Context *ctx, kgl_str_t *name, kgl_str_t *value);
typedef int (*http2_accept_handler_pt)(KHttp2Context *ctx);
typedef u_char *(KHttp2::*kgl_http_v2_handler_pt) (u_char *pos, u_char *end);
typedef struct {
	uintptr_t        hash;
	kgl_str_t         key;
	kgl_str_t         value;
	u_char           *lowcase_key;
} kgl_table_elt_t;
#if 0
class kgl_sync_result
{
public:
	KHttp2 *http2;
	KHttp2Context *ctx;
	KCondWait cond;
	LPWSABUF buf;
	int bufCount;
	int got;
};
#endif
typedef struct {
	kgl_str_t                        name;
	kgl_str_t                        value;
} kgl_http_v2_header_t;

struct kgl_http_v2_state_t {
	uint32_t					     sid;
	size_t                           length;
	size_t                           padding;
	unsigned                         flags : 8;
	unsigned                         incomplete : 1;
	/* HPACK */
	unsigned                         parse_name : 1;
	unsigned                         parse_value : 1;
	unsigned                         index : 1;
	unsigned						keep_pool : 1;
	kgl_pool_t						 *pool;
	kgl_http_v2_header_t             header;
	size_t                           header_length;
	u_char                           field_state;
	u_char                          *field_start;
	u_char                          *field_end;
	size_t                           field_rest;
	KHttp2Context					*stream;
	u_char                           buffer[8192];
	size_t                           buffer_used;
	kgl_http_v2_handler_pt           handler;
};

typedef struct {
	kgl_http_v2_header_t           **entries;
	uintptr_t                       added;
	uintptr_t                       deleted;
	uintptr_t                       reused;
	uintptr_t                       allocated;

	size_t                           size;
	size_t                           free;
	u_char                          *storage;
	u_char                          *pos;
} kgl_http_v2_hpack_t;


class KHttp2Node {
public:
	KHttp2Node()
	{
		memset(this, 0, sizeof(KHttp2Node));
	}
	uint32_t                  id;
	KHttp2Node				 *index;
	//uint8_t                   rank;
	//uint8_t                   weight;
	//double                    rel_weight;
	KHttp2Context            *stream;
};
class KHttp2Context
{
public:
	KHttp2Node *node;
	union {
		KHttpRequest *request;
		KAsyncFetchObject *fo;
		KOPAQUE data;
	};
	kgl_array_t  *cookies;
#ifndef NDEBUG
	INT64 orig_content_length;
	//KMD5_CTX md5;
#endif
	INT64 content_left;
	int64_t	active_msec;
	kgl_list queue;
	uint8_t tmo_left;
	uint8_t tmo;
#ifndef NDEBUG
	uint16_t timeout : 1;
#endif
	uint16_t  in_closed : 1;
	uint16_t  out_closed:1;
	uint16_t  rst : 1;
	uint16_t  parsed_header : 1;
	//{{ent
#ifdef ENABLE_UPSTREAM_HTTP2
	uint16_t  admin_stream : 1;
#endif//}}
	uint16_t  destroy_by_http2:1;
	uint16_t  skip_data:1;
	uint16_t  know_content_length:1;
	volatile int send_window;
	size_t recv_window;
	KHttp2HeaderFrame *send_header;
	kgl_http2_event *write_wait;
	kgl_http2_event *read_wait;
	KSendBuffer *read_buffer;
	bool Available()
	{
		if (rst) {
			return false;
		}
		kassert(!destroy_by_http2);
		if (destroy_by_http2) {
			return false;
		}
		return true;
	}
	http2_buff *build_write_buffer(kgl_http2_event *e, int &len)
	{
		http2_buff *buf_out = NULL;
		http2_buff *last = NULL;
		//WSABUF vc[MAX_HTTP2_BUFFER_SIZE];
		int total_len = 0;
		//int bufferCount = e->buffer(data, e->arg, vc, MAX_HTTP2_BUFFER_SIZE);
		for (int i = 0; i<e->bc; i++) {
			if (len <= 0) {
				break;
			}
			http2_buff *new_buf = new http2_buff;
			new_buf->skip_data_free = 1;
			new_buf->data = (char *)e->buf[i].iov_base;
			new_buf->used = (uint16_t)MIN(len, (int)e->buf[i].iov_len);
#ifndef NDEBUG
			//KMD5Update(&md5, (unsigned char *)new_buf->data, new_buf->used);
#endif
			//fwrite(new_buf->data, 1, new_buf->used, stdout);
			len -= new_buf->used;
			total_len += new_buf->used;
			if (last) {
				last->next = new_buf;
			} else {
				buf_out = new_buf;
			}
			last = new_buf;
		}
		assert(last);
		assert(total_len>0);
		e->len = total_len;
		http2_buff *new_buf = new http2_buff;
		http2_frame_header *data = (http2_frame_header *)malloc(sizeof(http2_frame_header));
		if (last) {
			//notice to last
			last->ctx = this;
		} else {
			new_buf->ctx = this;
		}
		new_buf->data = (char *)data;
		new_buf->used = sizeof(http2_frame_header);
		memset(data, 0, sizeof(http2_frame_header));
		data->set_length_type(total_len, KGL_HTTP_V2_DATA_FRAME);
		data->stream_id = ntohl(node->id);
		if (know_content_length) {
			content_left -= total_len;
			if (content_left <= 0) {
				data->flags |= KGL_HTTP_V2_END_STREAM_FLAG;
				out_closed = 1;
				kassert(content_left == 0);
				last->tcp_nodelay = 1;
			}
		}
		kassert(buf_out);
		new_buf->next = buf_out;
		len = total_len;
		return new_buf;
	}
	void CreateWriteWaitWindow(WSABUF* buf, int bc)
	{
		kassert(write_wait == NULL);
		write_wait = new kgl_http2_event;
		write_wait->buf = buf;
		write_wait->bc = bc;
		write_wait->fiber = kfiber_self();
		write_wait->len = -1;
	}
	void SetContentLength (INT64 content_length) {
		if (content_length >= 0) {
			know_content_length = 1;
			this->content_left = content_length;
#ifndef NDEBUG
			this->orig_content_length = content_length;
#endif
		} else {
			know_content_length = 0;
		}
	}
	void RemoveQueue()
	{
		if (queue.next) {
			klist_remove(&queue);
			queue.next = NULL;
		}
	}
	void AddQueue(kgl_list *list)
	{
		tmo_left = tmo;
		active_msec = kgl_current_msec;
		klist_append(list, &queue);
	}
	void Init(int send_window)
	{
		memset(this,0,sizeof(KHttp2Context));
		recv_window = KGL_HTTP_V2_STREAM_RECV_WINDOW;
		this->send_window = send_window;
#ifndef NDEBUG
		//KMD5Init(&md5);
#endif
	}
	void Destroy() {
		assert(read_wait==NULL);
		assert(write_wait == NULL||write_wait->buf==NULL);
		if (write_wait) {
			delete write_wait;
		}
		if (read_buffer) {
			delete read_buffer;
		}
		if (send_header) {
			delete send_header;
		}
		delete this;
	}
};
class KHttp2Upstream;
class KHttp2
{
public:
	KHttp2();
public:
	void server(kconnection *c);
	kselector *getSelector();
	//{{ent
#ifdef ENABLE_UPSTREAM_HTTP2
	KHttp2Upstream *client(kconnection *cn);
	KHttp2Upstream *connect();
	KHttp2Upstream *get_admin_stream();
	bool IsClientModel()
	{
		return client_model;
	}
#endif//}}
public:
	int ReadHeader(KHttp2Context *ctx);
	int read(KHttp2Context *ctx,WSABUF *buf,int bc);
	int write(KHttp2Context *ctx, WSABUF *buf,int bc);
	bool add_status(KHttp2Context *ctx,uint16_t status_code);
	bool add_method(KHttp2Context *ctx, u_char meth);
	bool add_header(KHttp2Context *ctx, kgl_header_type name, const char *val, hlen_t val_len);
	bool add_header(KHttp2Context *ctx,const char *name, hlen_t name_len, const char *val, hlen_t val_len);
	//kev_result read(KHttp2Context *ctx,result_callback result,buffer_callback buffer,void *arg);
	void read_hup(KHttp2Context *ctx, result_callback result, void *arg);
	void remove_read_hup(KHttp2Context *ctx);
	void shutdown(KHttp2Context *ctx);
	KHttp2Node **GetAllStream()
	{
		return streams_index;
	}
	//kev_result write(KHttp2Context *ctx,result_callback result,buffer_callback buffer,void *arg);
	void release(KHttp2Context *ctx);
	void release_stream(KHttp2Context *ctx);
	void release_admin(KHttp2Context *ctx);
	//int sync_send_header(KHttp2Context *ctx,INT64 body_len);
	int send_header(KHttp2Context *ctx);
	void write_end(KHttp2Context *ctx);
public:
	int getReadBuffer(iovec *buf,int bufCount);
	int getWriteBuffer(iovec *buf,int bufCount);
	kev_result resultRead(void *arg, int got);
	kev_result resultWrite(void *arg, int got);
	kev_result NextWrite(int got);
	kev_result NextRead(int got);
	friend class http2_buff;
	friend class KHttp2Sink;
	friend class KHttp2Upstream;
private:
	static kgl_http_v2_handler_pt kgl_http_v2_frame_states[];
	u_char *state_data(u_char *pos, u_char *end);
	u_char *state_headers(u_char *pos, u_char *end);
	u_char *state_priority(u_char *pos, u_char *end);
	u_char *state_rst_stream(u_char *pos, u_char *end);
	u_char *state_settings(u_char *pos, u_char *end);
	u_char *state_push_promise(u_char *pos, u_char *end);
	u_char *state_ping(u_char *pos, u_char *end);
	u_char *state_goaway(u_char *pos, u_char *end);
	u_char *state_window_update(u_char *pos, u_char *end);
	u_char *state_continuation(u_char *pos, u_char *end);
private:
	bool ReadHeaderSuccess(KHttp2Context *stream);
	bool check_recv_window(KHttp2Context *ctx);
	bool add_header_cookie(KHttp2Context *ctx, const char *val, hlen_t val_len);
	KHttp2Node *get_node(uint32_t id, bool alloc);
	int WriteWindowReady(KHttp2Context* ctx);
	u_char *close(bool read, int status);
	void init(kconnection *c);
	~KHttp2();
	kev_result CloseWrite();
	void ReleaseStateStream();
	bool can_destroy() {
		kassert(kselector_is_same_thread(c->st.selector));
		return katom_get((void *)&processing) == 0 && write_processing == 0 && read_processing == 0;
	}
	//void destroy();
	kev_result startRead();
	kev_result startWrite();
	void ping();
	void goaway(int error_code);
	KHttp2Context *create_stream();
	int CopyReadBuffer(KHttp2Context *ctx,WSABUF *buf,int bc);
	void setDependency(KHttp2Node *node, uint32_t depend, bool exclusive);
	intptr_t parse_int(u_char **pos, u_char *end, uintptr_t prefix);
	volatile size_t                  send_window;
	size_t                           recv_window;
	size_t                           init_window;
	size_t                           frame_size;
	size_t							 max_stream;
	KHttp2WriteBuffer write_buffer;
	kgl_list active_queue;
	KHttp2Node **streams_index;
	kgl_http_v2_state_t state;
	kgl_http_v2_hpack_t hpack;
	INT64 last_stream_msec;
	uint32_t last_peer_sid;
	uint32_t last_self_sid;
	uint16_t write_processing : 1;
	uint16_t read_processing : 1;
	uint16_t peer_goaway : 1;
	uint16_t self_goaway : 1;
	uint16_t closed : 1;
	uint16_t pinged : 1;
	uint16_t destroyed : 1;
	//{{ent
#ifdef ENABLE_UPSTREAM_HTTP2
	uint16_t client_model : 1;
	uint16_t has_admin_stream : 1;
	KHttp2Upstream *NewClientStream(bool admin);
#endif//}}
	volatile int32_t processing;
private:
	kconnection *c;
	bool send_settings(bool ack);
	bool send_window_update(uint32_t sid, size_t window);
	u_char *state_settings_params(u_char *pos, u_char *end);
	u_char *state_preface(u_char *pos, u_char *end);
	u_char *state_preface_end(u_char *pos, u_char *end);
	u_char *state_head(u_char *pos, u_char *end);
	u_char *state_skip(u_char *pos, u_char *end);
	u_char *state_complete(u_char *pos, u_char *end);
	u_char *skip_padded(u_char *pos, u_char *end);
	u_char *state_skip_headers(u_char *pos, u_char *end);
	u_char *state_header_block(u_char *pos, u_char *end);
	u_char *state_field_len(u_char *pos, u_char *end);
	u_char *state_skip_padded(u_char *pos, u_char *end);
	u_char *state_header_complete(u_char *pos, u_char *end);
	u_char *state_save(u_char *pos, u_char *end, kgl_http_v2_handler_pt handler);
	u_char *state_process_header(u_char *pos, u_char *end);
	u_char *state_field_skip(u_char *pos, u_char *end);
	u_char *state_field_huff(u_char *pos, u_char *end);
	u_char *state_field_raw(u_char *pos, u_char *end);
	u_char *state_read_data(u_char *pos, u_char *end);
	u_char *handle_continuation(u_char *pos, u_char *end, kgl_http_v2_handler_pt handler);
	void destroy_node(KHttp2Node *node);
	void adjust_windows(size_t window);
	void check_write_wait();
	bool send_rst_stream(uint32_t sid, uint32_t status);
	bool terminate_stream(KHttp2Context *stream, uint32_t status);
private:
	bool get_indexed_header(uintptr_t index, bool name_only);
	void CheckStreamTimeout();
	void AddQueue(KHttp2Context *stream)
	{
		stream->RemoveQueue();
		stream->AddQueue(&active_queue);
	}
	void FlushQueue(KHttp2Context *stream)
	{
		stream->RemoveQueue();
		if (stream->read_wait || (stream->write_wait && IS_WRITE_WAIT_FOR_WINDOW(stream->write_wait))) {
			stream->AddQueue(&active_queue);
		}
	}
	bool table_size(size_t size);
	bool table_account(size_t size);
	bool add_header(kgl_http_v2_header_t *header);
	bool add_cookie(kgl_http_v2_header_t *header);
	bool IsWriteClosed(KHttp2Context *ctx)
	{
		if (read_processing == 0 || closed || ctx->out_closed) {
			return true;
		}
		return false;
	}
};
#endif
#endif

