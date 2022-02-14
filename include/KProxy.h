#ifndef KPROXY_H_ASDF0
#define KPROXY_H_ASDF0
#include "global.h"
#include "kconnection.h"
#ifdef ENABLE_PROXY_PROTOCOL
#pragma pack(push,1)
struct kgl_proxy_hdr_v2 {
	uint8_t sig[12];  /* hex 0D 0A 0D 0A 00 0D 0A 51 55 49 54 0A */
	uint8_t ver_cmd;  /* protocol version and command */
	uint8_t fam;      /* protocol family and address */
	uint16_t len;     /* number of following bytes part of the header */
};
struct kgl_proxy_ipv4_addr {        /* for TCP/UDP over IPv4, len = 12 */
	uint32_t src_addr;
	uint32_t dst_addr;
	uint16_t src_port;
	uint16_t dst_port;
};
struct kgl_proxy_ipv6_addr {        /* for TCP/UDP over IPv6, len = 36 */
	uint8_t  src_addr[16];
	uint8_t  dst_addr[16];
	uint16_t src_port;
	uint16_t dst_port;
};
#pragma pack(pop)
class KHttpRequest;
class KReadWriteBuffer;
kev_result handl_proxy_request(kconnection *cn,result_callback cb);
bool build_proxy_header(KReadWriteBuffer *buffer, KHttpRequest *rq);
#endif
#endif
