/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 *
 * kangle web server              http://www.kangleweb.net/
 * ---------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *  See COPYING file for detail.
 *
 *  Author: KangHongjiu <keengo99@gmail.com>
 */

#ifndef FASTCGI_H_
#define FASTCGI_H_
/*
 * Listening socket file number
 */
#define FCGI_LISTENSOCK_FILENO 0

typedef struct {
	unsigned char version;
	unsigned char type;
	union {
		struct {
			unsigned char requestIdB1;
			unsigned char requestIdB0;
		};
		unsigned short requestId;
		unsigned short id;
	};
	union {
		struct {
			unsigned char contentLengthB1;
			unsigned char contentLengthB0;
		};
		unsigned short contentLength;
	};
	unsigned char paddingLength;
	unsigned char reserved;
} FCGI_Header;

/*
 * Number of bytes in a FCGI_Header.  Future versions of the protocol
 * will not reduce this number.
 */
#define FCGI_HEADER_LEN  8

/*
 * Value for version component of FCGI_Header
 */
#define FCGI_VERSION_1           1

/*
 * Values for type component of FCGI_Header
 */
#define FCGI_BEGIN_REQUEST      1
#define FCGI_ABORT_REQUEST      2
#define FCGI_END_REQUEST        3
#define FCGI_PARAMS             4
#define FCGI_STDIN              5
#define FCGI_STDOUT             6
#define FCGI_STDERR             7
#define FCGI_DATA               8
#define FCGI_GET_VALUES		    9
#define FCGI_GET_VALUES_RESULT  10
#define FCGI_UNKNOWN_TYPE       11
#define FCGI_EXTEND_CMD         100
 
#define API_CHILD_SHUTDOWN        FCGI_EXTEND_CMD+1
#define API_CHILD_LOAD            FCGI_EXTEND_CMD+2
#define API_CHILD_CHROOT          FCGI_EXTEND_CMD+4
#define API_CHILD_SETUID          FCGI_EXTEND_CMD+5
#define API_CHILD_LISTEN          FCGI_EXTEND_CMD+6
#define API_CHILD_LISTEN_UNIX     FCGI_EXTEND_CMD+7

/*
map 一个url到物理地址
*/
#define API_CHILD_MAP_PATH        FCGI_EXTEND_CMD+8
/*
读一次数据
*/
#define API_CHILD_READ_ONE        FCGI_EXTEND_CMD+9
/*
读全部数据
*/
#define API_CHILD_READ_ALL        FCGI_EXTEND_CMD+10
#define API_CHILD_LOGON           FCGI_EXTEND_CMD+11
#define CMD_CREATE_PROCESS        FCGI_EXTEND_CMD+12
#define CMD_CREATE_PROCESS_UNIX   FCGI_EXTEND_CMD+13

#define API_CHILD_LOAD_RESULT     FCGI_EXTEND_CMD+50
#define API_CHILD_SETUID_RESULT   FCGI_EXTEND_CMD+51
#define API_CHILD_CHROOT_RESULT   FCGI_EXTEND_CMD+52
#define API_CHILD_LISTEN_RESULT   FCGI_EXTEND_CMD+53
#define API_CHILD_MAP_PATH_RESULT FCGI_EXTEND_CMD+54
#define API_CHILD_LOGON_RESULT    FCGI_EXTEND_CMD+55
#define CMD_CREATE_PROCESS_RESULT FCGI_EXTEND_CMD+56

#define FCGI_MAXTYPE              (FCGI_UNKNOWN_TYPE)

/*
 * Value for requestId component of FCGI_Header
 */
#define FCGI_NULL_REQUEST_ID     0

typedef struct {
	union {
		struct {
			unsigned char roleB1;
			unsigned char roleB0;
		};
		/*
		 * id只是内部通信用
		 */
		u_short id;
	};
	unsigned char flags;
	unsigned char reserved[5];
} FCGI_BeginRequestBody;

typedef struct {
	FCGI_Header header;
	FCGI_BeginRequestBody body;
} FCGI_BeginRequestRecord;

/*
 * Mask for flags component of FCGI_BeginRequestBody
 */
#define FCGI_KEEP_CONN  1
/*
 * Values for role component of FCGI_BeginRequestBody
 */
#define FCGI_RESPONDER  	1
#define FCGI_AUTHORIZER 2
#define FCGI_FILTER     	3

typedef struct {
	unsigned char appStatusB3;
	unsigned char appStatusB2;
	unsigned char appStatusB1;
	unsigned char appStatusB0;
	unsigned char protocolStatus;
	unsigned char reserved[3];
} FCGI_EndRequestBody;

typedef struct {
	FCGI_Header header;
	FCGI_EndRequestBody body;
} FCGI_EndRequestRecord;

/*
 * Values for protocolStatus component of FCGI_EndRequestBody
 */
#define FCGI_REQUEST_COMPLETE 0
#define FCGI_CANT_MPX_CONN    1
#define FCGI_OVERLOADED       	2
#define FCGI_UNKNOWN_ROLE     3

/*
 * Variable names for FCGI_GET_VALUES / FCGI_GET_VALUES_RESULT records
 */
#define FCGI_MAX_CONNS  	"FCGI_MAX_CONNS"
#define FCGI_MAX_REQS   	"FCGI_MAX_REQS"
#define FCGI_MPXS_CONNS 	"FCGI_MPXS_CONNS"
#define FCGI_MAX_PACKET_SIZE	65543//65535+8
typedef struct {
	unsigned char type;
	unsigned char reserved[7];
} FCGI_UnknownTypeBody;

typedef struct {
	FCGI_Header header;
	FCGI_UnknownTypeBody body;
} FCGI_UnknownTypeRecord;

/*
 * extend
 */
struct api_child_t_uidgid {
	int uid;
	int gid;
};
struct sp_info {
	int result;
	int pid;
	int key;
	int port;
};
/*
 * 在连接上发送
 */
struct sp_connect {
	int key;
};
#endif /* FASTCGI_H_ */
