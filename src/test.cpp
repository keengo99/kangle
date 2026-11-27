/*
 * test.cpp
 *
 *  Created on: 2010-5-31
 *      Author: keengo
 *
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
#include <assert.h>
#include <string.h>
#include <vector>
#include "global.h"
#include "KGzip.h"
#include "do_config.h"
#include "kmalloc.h"
#include "KLineFile.h"
#include "KHttpFieldValue.h"
#include "KHttpField.h"
#include "KPipeStream.h"
#include "KFileName.h"
#include "KVirtualHost.h"
#include "KHtAccess.h"
#include "KTimeMatch.h"
#include "KReg.h"
#include "kfile.h"
#include "KXml.h"
#include "KUrlParser.h"
#include "cache.h"
#include "kconnection.h"
#include "KHttpSink.h"
#include "kselector_manager.h"
#include "KHttpParser.h"
#include "KIpMap.h"
#include "KSimulateRequest.h"
#include "KSockPoolHelper.h"
#include "KAsyncFetchObject.h"
#include "KConfigTree.h"
#include "KAutoArray.h"
#include "serializable.h"
#ifdef ENABLE_ATOM
#include "KAtomLock.h"
#endif
#include "KWhiteList.h"
#include "KVirtualHostContainer.h"
#ifndef NDEBUG
using namespace std;

void test_file() {
	const char* test_file = "c:\\windows\\temp\\test.txt";
	KFile file;
	assert(file.open(test_file, fileWrite));
	assert(file.write("test", 4));
	file.close();
	assert(file.open(test_file, fileRead));
	char buf[8];
	int len = file.read(buf, 8);
	assert(len == 4);
	assert(strncmp(buf, "test", 4) == 0);
	file.seek(1, seekBegin);
	len = file.read(buf, 8);
	assert(len == 3);
	file.close();
	file.open(test_file, fileAppend);
	file.write("t2", 2);
	file.close();
	file.open(test_file, fileRead);
	assert(file.read(buf, 8) == 6);
	file.close();
}
bool test_pipe() {
	return true;
}
void test_vh_container() {
	KDomainMap* vhc = new KDomainMap;
	vhc->bind("*.a.com", (void*)1, kgl_bind_unique);
	vhc->bind("a.com", (void*)2, kgl_bind_unique);
	void* result = vhc->find("*.a.com");
	intptr_t ret = (intptr_t)result;
	kassert(ret == 1);
	delete vhc;
}

void test_regex() {
	KReg reg;
	reg.setModel("s(t)s", 0);
	//int ovector[6];
	//int ret = reg.match("sjj", -1, PCRE_PARTIAL, ovector, 6);
	//printf("ret=%d\n",ret);
	KRegSubString *ss = reg.matchSubString(_KS("AstsB"),0);
	assert(ss);
}
/*
void test_cache()
{
	const char *host = "abcdef";
	for (int i=0;i<100;i++) {
		std::stringstream s;
		int h = rand()%6;
		s << "http://" << host[h] << "/" << rand();
		KHttpObject *obj = new KHttpObject;
		create_http_object(obj,s.str().c_str(),false);
		obj->release();
	}
}
*/
void test_container() {
	/*
	unsigned char buf[18];
	const char *hostname = ".test.com";
	assert(revert_hostname(hostname,strlen(hostname),buf,sizeof(buf)));
	printf("ret=[%s]\n",buf);
	*/
}
void test_htaccess() {
	const char* configs[] = {
		nullptr, // No successful load, hence no EvRemove callbacks.
		"<config/>",
		"<config><request action='allow'/></config>",
		"<config><response action='allow'/></config>",
		"<config><request action='allow'/><response action='allow'/></config>"
	};
	for (auto config : configs) {
		for (bool clear_first : { false, true }) {
			for (bool hold_access : { false, true }) {
				KSafeAccess held[2];
				{
					_KApacheHtaccessContext ctx("unused-htaccess-test", "");
					if (hold_access) {
						for (int i = 0; i < 2; ++i) {
							held[i].reset(ctx.access[i]->add_ref());
						}
					}
					if (config) {
						kgl_auto_cstr xml(xstrdup(config));
						auto nodes = kconfig::parse_xml(xml.get());
						assert(nodes);
						ctx.file->update(std::move(nodes));
					}
					if (clear_first) {
						ctx.file->clear();
					}
				}
				// Context/tree cleanup must preserve independently held references.
				if (hold_access) {
					for (int i = 0; i < 2; ++i) {
						assert(held[i]->get_type() == i);
						assert(!held[i]->isGlobal());
					}
				}
			}
		}
	}
}
void test_file(const char* path) {
#if 0
	KFileName file;
	std::string doc_root = "d:\\project\\kangle\\www\\";
	bool result = file.setName(doc_root.c_str(), path, FOLLOW_LINK_NO | FOLLOW_PATH_INFO);
	printf("triped_path=[%s],result=%d\n", path, result);
	if (result) {
		printf("pre_dir=%d,is_dir=%d,path_info=[%d],filename=[%s]\n", file.isPrevDirectory(), file.isDirectory(), file.getPathInfoLength(), file.getName());
	}
#endif
}
void test_files() {
	test_file("/test.php");
	test_file("/test.php/a/b");
	test_file("/");
	test_file("/a/");
	test_file("/a");
	test_file("/b");

}
void test_timematch() {
	KTimeMatch* t = new KTimeMatch;
	t->set("* * * * *");
	t->Show();
	delete t;
	t = new KTimeMatch;
	t->set("*/5  */5 * * *");
	t->Show();
	delete t;
	t = new KTimeMatch;
	t->set("2-10/3,50  0-6 * * *");
	t->Show();
	delete t;
}
kev_result test_kgl_addr_timer(KOPAQUE data, void* arg, int got) {
	printf("test timer..\n");
	kselector* selector = (kselector*)arg;
	kselector_add_timer(selector, test_kgl_addr_timer, arg, 2000, NULL);
	return kev_ok;
}
void WINAPI when_selector_manager_ready(void* arg) {
	kselector* selector = get_perfect_selector();
	kgl_selector_module.next(selector, NULL, test_kgl_addr_timer, selector, 0);
}
void test_map() {
	std::map<char*, bool, lessp> m;
	for (int i = 0; i < 100; i++) {
		std::stringstream s;
		s.str("");
		s << i;
		m.insert(std::pair<char*, bool>(strdup(s.str().c_str()), true));
	}

	std::map<char*, bool, lessp>::iterator it;
	for (it = m.begin(); it != m.end(); it++) {
		free((*it).first);
	}
}
void test_bigobject() {
	/*
	KSharedBigObject sbo;
	bool newobj;
	sbo.insert(0,newobj);
	assert(newobj);
	sbo.insert(5,newobj);
	assert(newobj);
	sbo.insert(2,newobj);
	assert(newobj);
	sbo.insert(10,newobj);
	assert(newobj);
	sbo.write(2,NULL,3);
	sbo.print();
	for(int i=0;i<13;i++){
		rb_node *node = sbo.findBlock(i);
		printf("from=%d,",i);
		if(node==NULL){
			printf("NULL");
		} else {
			KBigObjectBlock *block = (KBigObjectBlock *)node->data;
			printf("%d",(int)block->from);
		}
		printf("\n");
	}
	*/
}
void test_atom() {
#ifdef ENABLE_ATOM
	KAtomLock lock;
	assert(lock.lock());
	assert(!lock.lock());
	assert(lock.unlock());
	assert(!lock.unlock());
#endif
}
void test_xml() {

	char* s = strdup("&&&");
	int len = (int)strlen(s);
	char* dst = KXml::htmlEncode(s, len, NULL);
	printf("len=%d,s=[%s]\n", len, dst);
	s = strdup("&lt;!-- JiaThis Button BEGIN --&gt;&#13;&#10;&lt;script type=&quot;text/javascript&quot; src=&quot;http://v3.jiathis.com/code/jiathis_r.js?uid=1355687902433269&amp;move=0&quot; charset=&quot;utf-8&quot;&gt;&lt;/script&gt;&#13;&#10;&lt;!-- JiaThis Button END --&gt;&#10;&lt;/html&gt;");
	len = 0;
	dst = KXml::htmlDecode(s, len);
	printf("dst=[%s]\n", dst);
}
void test_config() {
	
}
//}}
void test_url_decode() {
	char buf2[5];
	//strcpy(buf2,"test");
	kgl_memcpy(buf2, "test", 4);
	buf2[4] = '*';
	url_decode(buf2, 4, NULL, true);
	// printf("buf=[%s]\n",buf2);
	assert(buf2[4] == '*');
	//strcpy(buf2,"test");
	kgl_memcpy(buf2, "%20t", 4);
	url_decode(buf2, 4, NULL, true);
	//printf("buf=[%s]\n",buf2);
	assert(buf2[2] == '\0');
}
/* Feed one buffer to the engine and return the result; piece_length starts at max_piece. */
static KDechunkResult dechunk_feed(KDechunkEngine* engine, const char** buf, const char* end, const char** piece, int* piece_length, int max_piece = KHTTPD_MAX_CHUNK_SIZE) {
	*piece_length = max_piece;
	return engine->dechunk(buf, end, piece, piece_length);
}
static void test_dechunk2() {
	// A whole body in one buffer, taken 5 bytes at a time.
	KDechunkEngine engine;
	ks_buffer* buffer = ks_buffer_new(512);
	ks_write_str(buffer, kgl_expand_string("9\r\n123456789\r\n0\r\n\r\n"));
	const char* hot = buffer->buf;
	const char* end = buffer->buf + buffer->used;
	std::string body;
	for (;;) {
		const char* piece = NULL;
		int piece_len;
		KDechunkResult result = dechunk_feed(&engine, &hot, end, &piece, &piece_len, 5);
		if (result == KDechunkResult::End) {
			break;
		}
		assert(result == KDechunkResult::Success);
		if (result != KDechunkResult::Success) {
			break;
		}
		assert(piece_len > 0 && piece_len <= 5);
		body.append(piece, piece_len);
	}
	assert(hot == end);
	assert(body == "123456789");
	ks_buffer_destroy(buffer);
}
static void test_dechunk_rejects(const char* input) {
	// Chunked framing needs CRLF everywhere: bare LF or CR must fail, not be guessed.
	KDechunkEngine engine;
	const char* buf = input;
	const char* end = buf + strlen(buf);
	const char* piece;
	int piece_length;
	KDechunkResult result;
	do {
		result = dechunk_feed(&engine, &buf, end, &piece, &piece_length);
	} while (result == KDechunkResult::Success && buf < end);
	assert(result == KDechunkResult::Failed);
	assert(dechunk_feed(&engine, &buf, end, &piece, &piece_length) == KDechunkResult::Failed);
}
void test_dechunk() {
	KDechunkEngine* engine = new KDechunkEngine;
	const char* piece;
	int piece_length;
	/* The body arrives in pieces:
	* 1\r\n a\r\n               one byte chunk
	* 1 | 0\r\n012345 | 6789abcdef   16 byte chunk, size line split across reads
	* \r\n5\r\nhello            5 byte chunk read 2 bytes at a time
	* \r\n0\r\n\r\n             last chunk
	*/
	const char* buf = "1\r\na\r\n";
	const char* end = buf + strlen(buf);
	assert(dechunk_feed(engine, &buf, end, &piece, &piece_length) == dechunk_success);
	assert(piece_length == 1 && strncmp(piece, "a", piece_length) == 0);
	assert(dechunk_feed(engine, &buf, end, &piece, &piece_length) == dechunk_continue);
	assert(buf == end);
	buf = "1";
	end = buf + strlen(buf);
	assert(dechunk_feed(engine, &buf, end, &piece, &piece_length) == dechunk_continue);
	buf = "0\r\n012345";
	end = buf + strlen(buf);
	assert(dechunk_feed(engine, &buf, end, &piece, &piece_length) == dechunk_success);
	assert(piece_length == 6 && strncmp(piece, "012345", piece_length) == 0);
	buf = "6789abcdef";
	end = buf + strlen(buf);
	assert(dechunk_feed(engine, &buf, end, &piece, &piece_length) == dechunk_success);
	assert(piece_length == 10 && strncmp(piece, "6789abcdef", piece_length) == 0);
	buf = "\r\n5\r\nhello";
	end = buf + strlen(buf);
	assert(dechunk_feed(engine, &buf, end, &piece, &piece_length, 2) == dechunk_success);
	assert(piece_length == 2 && strncmp(piece, "he", piece_length) == 0);
	assert(dechunk_feed(engine, &buf, end, &piece, &piece_length) == dechunk_success);
	assert(piece_length == 3 && strncmp(piece, "llo", piece_length) == 0);
	buf = "\r\n0\r\n\r\n";
	end = buf + strlen(buf);
	assert(dechunk_feed(engine, &buf, end, &piece, &piece_length) == dechunk_end);
	assert(buf == end);
	assert(dechunk_feed(engine, &buf, end, &piece, &piece_length) == dechunk_end);
	delete engine;

	// A trailer field is returned before the end.
	engine = new KDechunkEngine;
	buf = "0\r\nX-Test: v\r\n\r\n";
	end = buf + strlen(buf);
	assert(dechunk_feed(engine, &buf, end, &piece, &piece_length) == KDechunkResult::Trailer);
	assert(piece_length == 9 && strncmp(piece, "X-Test: v", 9) == 0);
	assert(dechunk_feed(engine, &buf, end, &piece, &piece_length) == dechunk_end);
	delete engine;

	test_dechunk_rejects("1\na\r\n");        // bare LF after the size
	test_dechunk_rejects("10\raaa\n");       // CR not followed by LF
	test_dechunk_rejects("1\r\naX");         // data not followed by CRLF
	test_dechunk_rejects("9\n123456789\n0\n\n");
}
/* Feed one buffer completely; returns false once the engine fails. */
static bool dechunk_feed_all(KDechunkEngine* engine, const std::string& input) {
	const char* buf = input.c_str();
	const char* end = buf + input.size();
	while (buf < end) {
		const char* piece;
		int piece_length;
		KDechunkResult result = dechunk_feed(engine, &buf, end, &piece, &piece_length);
		if (result == KDechunkResult::Failed) {
			return false;
		}
		if (result == KDechunkResult::End) {
			break;
		}
	}
	return true;
}
static void test_dechunk_overhead() {
	// 1-byte chunks with 16000 leading zeros: ~16006 bytes of framing per data byte.
	std::string flood_chunk(16000, '0');
	flood_chunk += "1\r\nX\r\n";
	KDechunkEngine engine;
	int chunks = 0;
	while (chunks < 1000 && dechunk_feed_all(&engine, flood_chunk)) {
		chunks++;
	}
	// received / 8 > data + 1MB happens after about 524 such chunks.
	assert(chunks >= 500 && chunks < 530);

	// Plain 1-byte chunks are 6 bytes per data byte, below the 8x limit.
	KDechunkEngine small;
	std::string tiny;
	for (int i = 0; i < 4096; i++) {
		tiny += "1\r\nX\r\n";
	}
	for (int i = 0; i < 64; i++) {
		assert(dechunk_feed_all(&small, tiny));
	}
	assert(dechunk_feed_all(&small, "0\r\n\r\n"));
	assert(small.is_success());
}
static void test_trip_dir() {
	struct {
		const char* in;
		const char* out;
	} cases[] = {
		{ "/dav/../x", "/x" },
		{ "/../../etc/passwd", "/etc/passwd" },
		{ "/a/b/../../../c", "/c" },
		{ "/dav/..\\..\\x", "/x" },
		{ "/a/./b/", "/a/b/" },
		{ "/a//b", "/a/b" },
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		char* path = xstrdup(cases[i].in);
		KFileName::tripDir3(path, '/');
		assert(strcmp(path, cases[i].out) == 0);
		xfree(path);
	}
}
static void test_alias_traversal() {
	// An absolute alias target must not be left through ".." in the rest of the path.
	KVirtualHost* vh = new KVirtualHost("alias_test");
	vh->doc_root = "/www/";
	KString err;
	bool added = vh->addAlias("/dav/", "/data/dav/", vh->doc_root.c_str(), true, -1, err);
	assert(added);
	kgl_auto_cstr mapped = vh->alias(true, "/dav/../../../etc/passwd");
	// Normalized to "/etc/passwd", which no longer matches the "/dav/" alias.
	assert(!mapped);
	mapped = vh->alias(true, "/dav/a/../b");
	std::string expected = "/data/dav/b";
	for (auto& c : expected) {
		if (c == '/') {
			c = PATH_SPLIT_CHAR;
		}
	}
	assert(mapped && expected == mapped.get());
	vh->release();
}
void test_white_list() {
#ifdef ENABLE_BLACK_LIST
	kgl_current_sec = time(NULL);
	wlm.add(".", NULL, "1");
	wlm.add(".", NULL, "2");
	wlm.add(".", NULL, "3");
	wlm.flush(time(NULL) + 100, 10);
#endif
}
void test_string() {
}
void test_line_file() {
	KStreamFile lf;
	lf.open("d:\\line.txt");
	for (;;) {
		char* line = lf.read();
		if (line == NULL) {
			break;
		}
		printf("line=[%s]\n", line);
	}
}
void test_http_parser() {
	char* buf = strdup("HTTP/1.1 200 OK\r\na: bsssssssssssssddddd\r\n ttt");
	char* hot = buf;
	int len = (int)strlen("HTTP/1.1 200 OK\r\na: bsssssssssssssddddd\r\n ttt");
	khttp_parser parser;
	khttp_parse_result rs;
	memset(&parser, 0, sizeof(parser));
	char* end = hot + len;
	kassert(kgl_parse_continue == khttp_parse(&parser, &hot, end, &rs));
	kassert(kgl_parse_continue == khttp_parse(&parser, &hot, end, &rs));
}
void test_mem_function() {
	assert(kgl_ncasecmp(_KS("AABB"), _KS("aA")) == 0);
	assert(kgl_ncasecmp(_KS("aA"), _KS("AABB")) != 0);
	assert(kgl_ncmp(_KS("AABB"), _KS("AA")) == 0);
	assert(kgl_ncmp(_KS("AA"), _KS("AABB")) != 0);
	assert(kgl_casecmp("aaa", _KS("aAA")) == 0);
}
bool test() {
	test_mem_function();
	test_config();
	test_url_decode();
	test_regex();
	test_htaccess();
	test_container();
	test_vh_container();
	test_dechunk();
	test_dechunk2();
	test_dechunk_overhead();
	test_trip_dir();
	test_alias_traversal();
	test_white_list();
	test_string();
	time_t nowTime = time(NULL);
	char timeStr[41];
	mk1123time(nowTime, timeStr, sizeof(timeStr) - 1);
	assert(kgl_parse_http_time((u_char*)timeStr, 40) == nowTime);
	INT64 t = 123;
	char buf[INT2STRING_LEN];
	int2string(t, buf);
	if (strcmp(buf, "123") != 0) {
		fprintf(stderr, "Warning int2string function is not correct\n");
		assert(false);
	} else if (string2int(buf) != 123) {
		fprintf(stderr, "Warning string2int function is not correct\n");
		assert(false);
	}
	KHttpField field;
	//	test_files();
	return true;
}
#endif
