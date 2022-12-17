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
#include <vector>
#include <assert.h>
#include <string.h>
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
#include "KChunked.h"
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
//{{ent
#include "KIpMap.h"
#include "KSimulateRequest.h"
#ifdef ENABLE_ATOM
#include "KAtomLock.h"
#endif
#include "KWhiteList.h"
//}}
#ifdef ENABLE_INPUT_FILTER
#include "KMultiPartInputFilter.h"
#endif
#include "KVirtualHostContainer.h"
#ifndef NDEBUG
using namespace std;
char *getString(char *str, char **substr);

void test_file()
{
	const char *test_file = "c:\\windows\\temp\\test.txt";
	KFile file;
	assert(file.open(test_file,fileWrite));
	assert(file.write("test",4));
	file.close();
	assert(file.open(test_file,fileRead));
	char buf[8];
	int len = file.read(buf,8);
	assert(len==4);
	assert(strncmp(buf,"test",4)==0);
	file.seek(1,seekBegin);
	len = file.read(buf,8);
	assert(len==3);
	file.close();
	file.open(test_file,fileAppend);
	file.write("t2",2);
	file.close();
	file.open(test_file,fileRead);
	assert(file.read(buf,8)==6);
	file.close();
}
bool test_pipe() {
	return true;
}
void test_vh_container()
{
	KDomainMap *vhc = new KDomainMap;
	vhc->bind("*.a.com", (void *)1, kgl_bind_unique);
	vhc->bind("a.com", (void *)2, kgl_bind_unique);
	void *result = vhc->find("*.a.com");
	intptr_t ret = (intptr_t)result;
	kassert(ret == 1);
	delete vhc;
}
void test_regex() {
	KReg reg;
	reg.setModel("s",0);
	int ovector[6];
	int ret = reg.match("sjj",-1,PCRE_PARTIAL,ovector,6);
	//printf("ret=%d\n",ret);
	//KRegSubString *ss = reg.matchSubString("t", 1, 0);
	//assert(ss);
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
	//static const char *file = "/home/keengo/httpd.conf";
	//KHtAccess htaccess;
	//KFileName file;
	//file.setName("/","/home/keengo/");
	//printf("result=%d\n",htaccess.load("/","/home/keengo/"));
}
void test_file(const char *path)
{
#if 0
	KFileName file;
	std::string doc_root = "d:\\project\\kangle\\www\\";
	bool result = file.setName(doc_root.c_str(), path, FOLLOW_LINK_NO|FOLLOW_PATH_INFO);
	printf("triped_path=[%s],result=%d\n",path,result);
	if(result){
		printf("pre_dir=%d,is_dir=%d,path_info=[%d],filename=[%s]\n",file.isPrevDirectory(),file.isDirectory(),file.getPathInfoLength(),file.getName());
	}
#endif
}
void test_files()
{
	test_file("/test.php");
	test_file("/test.php/a/b");
	test_file("/");
	test_file("/a/");
	test_file("/a");
	test_file("/b");

}
void test_timematch()
{
	KTimeMatch *t = new KTimeMatch;
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
//{{ent
#ifdef ENABLE_INPUT_FILTER
void test_multipart_filter()
{
	const char *str = "--AaB03x\r\nContent-Disposition: form-data; name=\"name\"\r\n\r\nLy\r\n--AaB03x\r\nContent-Disposition: form-data; name=\"files\"; filename=\"file1.txt\"\r\nContent-Type: text/plain\r\n\r\n... contents of file1.txt ...\r\n--AaB03x--";
	KInputFilterContext ctx(NULL);
	ctx.parse_boundary(_KS("multipart/form-data; boundary=AaB03x"));
	ctx.filter = new KMultiPartInputFilter;
	int len = (int)strlen(str);
	for (int i=0;i<len;i++) {
		ctx.check(str+i,1,false);
	}
}
#endif
kev_result test_kgl_addr_timer(KOPAQUE data, void *arg, int got) {
	printf("test timer..\n");
	kselector *selector = (kselector *)arg;
	kselector_add_timer(selector,test_kgl_addr_timer, arg, 2000, NULL);
	return kev_ok;
}
void WINAPI when_selector_manager_ready(void *arg) {
	kselector *selector = get_perfect_selector();
	kgl_selector_module.next(selector,NULL, test_kgl_addr_timer,selector,0);
}
void test_map()
{
	std::map<char *,bool,lessp> m;
	for(int i=0;i<100;i++){
		std::stringstream s;
		s.str("");
		s << i;
		m.insert(std::pair<char *,bool>(strdup(s.str().c_str()),true));
	}

	std::map<char *,bool,lessp>::iterator it;
	for(it=m.begin();it!=m.end();it++){
		free((*it).first);
	}
}
void test_bigobject()
{
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
void test_atom()
{
#ifdef ENABLE_ATOM
	KAtomLock lock;
	assert(lock.lock());
	assert(!lock.lock());
	assert(lock.unlock());
	assert(!lock.unlock());	
#endif
}
void test_xml()
{
	char *s = strdup("&&&");
	int len = (int)strlen(s);
	char *dst = KXml::htmlEncode(s,len,NULL);
	printf("len=%d,s=[%s]\n",len,dst);
	s = strdup("&lt;!-- JiaThis Button BEGIN --&gt;&#13;&#10;&lt;script type=&quot;text/javascript&quot; src=&quot;http://v3.jiathis.com/code/jiathis_r.js?uid=1355687902433269&amp;move=0&quot; charset=&quot;utf-8&quot;&gt;&lt;/script&gt;&#13;&#10;&lt;!-- JiaThis Button END --&gt;&#10;&lt;/html&gt;");
	len = 0;
	dst = KXml::htmlDecode(s,len);
	printf("dst=[%s]\n",dst);
}
void test_config()
{
	KConfig c;
	//printf("sizeof = %d\n",sizeof(c.a));
}
//}}
void test_url_decode() {
	char buf2[5];
    //strcpy(buf2,"test");
    kgl_memcpy(buf2,"test",4);
    buf2[4] = '*';
    url_decode(buf2, 4,NULL,true);
   // printf("buf=[%s]\n",buf2);
    assert(buf2[4]=='*');
    //strcpy(buf2,"test");
    kgl_memcpy(buf2,"%20t",4);
    url_decode(buf2, 4,NULL,true);
    //printf("buf=[%s]\n",buf2);
    assert(buf2[2]=='\0');
}
static void test_dechunk2()
{
	//printf("sizeof(KDechunkEngine2)=[%d],sizeof(KDechunkEngine)=[%d]\n", sizeof(KDechunkEngine2), sizeof(KDechunkEngine));
	KDechunkEngine engine;
	ks_buffer* buffer = ks_buffer_new(512);
	ks_write_str(buffer, kgl_expand_string("9\n123456789\n0\n\n"));
	const char* hot = buffer->buf;
	int len = buffer->used;
	for (;;) {
		const char* piece = NULL;
		int piece_len = 5;
		if (engine.dechunk(&hot, len, &piece, &piece_len) == KDechunkResult::End) {
			assert(hot == buffer->buf + len);
			break;
		}
		printf("piece_len=[%d]\n", piece_len);
		fwrite(piece, 1, piece_len, stdout);
		printf("\n");
	}

}
#ifdef ENABLE_INPUT_FILTER
void test_dechunk() {
	KDechunkEngine *engine = new KDechunkEngine;
	const char *piece;
	int piece_length = KHTTPD_MAX_CHUNK_SIZE;
	/*
	* 1\r\n
	* a\r\n
	* 10\r\n
	* 0123456789abcdef\r\n
	* 10\raaa\n
	* x
	*/
	const char *buf = "1\r\na\r\n";
	const char* end = buf + strlen(buf);
	assert(engine->dechunk(&buf, end, &piece, &piece_length) == dechunk_success);
	assert(strncmp(piece, "a",piece_length) == 0);
	piece_length = KHTTPD_MAX_CHUNK_SIZE;
	assert(engine->dechunk(&buf, end, &piece, &piece_length) == dechunk_continue);
	buf = "1";
	end = buf + strlen(buf);
	piece_length = KHTTPD_MAX_CHUNK_SIZE;
	assert(engine->dechunk(&buf, end, &piece, &piece_length) == dechunk_continue);
	buf = "0\r\n012345";
	end = buf + strlen(buf);
	piece_length = KHTTPD_MAX_CHUNK_SIZE;
	assert(engine->dechunk(&buf, end, &piece, &piece_length) == dechunk_success);
	assert(piece_length==6 && strncmp(piece, "012345", piece_length) == 0);
	buf = "6789abcdef";
	end = buf + strlen(buf);
	piece_length = KHTTPD_MAX_CHUNK_SIZE;
	assert(engine->dechunk(&buf, end, &piece, &piece_length) == dechunk_success);
	assert(piece_length==10 && strncmp(piece, "6789abcdef", piece_length) == 0);
	buf = "\r\na\r";
	end = buf + strlen(buf);
	assert(engine->dechunk(&buf, end, &piece, &piece_length) == dechunk_continue);
	buf = "aaa\nx";
	end = buf + strlen(buf);
	assert(engine->dechunk(&buf, end, &piece, &piece_length) == dechunk_success);
	assert(piece_length == 1 && strncmp(piece, "x", piece_length) == 0);
	delete engine;
}
#endif
void test_white_list() {
#ifdef ENABLE_BLACK_LIST
	kgl_current_sec = time(NULL);
	wlm.add(".", NULL, "1");
	wlm.add(".", NULL, "2");
	wlm.add(".", NULL, "3");
	wlm.flush(time(NULL)+100, 10);
#endif
}
void test_line_file()
{
	KStreamFile lf;
	lf.open("d:\\line.txt");
	for (;;) {
		char *line = lf.read();
		if (line == NULL) {
			break;
		}
		printf("line=[%s]\n", line);
	}
}
void test_suffix_corrupt() {
        char *str = new char[4];
        kgl_memcpy(str,"test1",5);
	delete[] str;
}
void test_prefix_corrupt() {
        char *str = (char *)malloc(4);
	void *pp = str - 1;
	kgl_memcpy(pp,"test",4);
        free(str);
}
void test_freed_memory() {
        char *str = (char *)malloc(4);
        free(str);
        kgl_memcpy(str,"test",4);
}
void test_http_parser()
{
	char *buf = strdup("HTTP/1.1 200 OK\r\na: bsssssssssssssddddd\r\n ttt");
	char *hot = buf;
	int len = (int)strlen("HTTP/1.1 200 OK\r\na: bsssssssssssssddddd\r\n ttt");
	khttp_parser parser;
	khttp_parse_result rs;
	memset(&parser, 0, sizeof(parser));	
	char* end = hot + len;
	kassert(kgl_parse_continue == khttp_parse(&parser, &hot, end, &rs));
	kassert(kgl_parse_continue == khttp_parse(&parser, &hot, end, &rs));
}
void test_mem_function()
{
	assert(kgl_ncasecmp(_KS("AABB"), _KS("aA")) == 0);
	assert(kgl_ncasecmp( _KS("aA"), _KS("AABB")) != 0);
	assert(kgl_ncmp(_KS("AABB"), _KS("AA")) == 0);
	assert(kgl_ncmp(_KS("AA"), _KS("AABB")) != 0);
	assert(kgl_casecmp("aaa", _KS("aAA")) == 0);
}
bool test() {	
	test_mem_function();
	//printf("offsetof st_flags=[%d] %d %d %d\n", offsetof(kselectable, st_flags), offsetof(kselectable, tmo_left), offsetof(kselectable, tmo), offsetof(kselectable, fd));
	//printf("size=[%d]\n", kgl_align(1, 1024));
	//test_freed_memory();
	//test_suffix_corrupt();
	//test_prefix_corrupt();
	//printf("sizeof(kconnection) = %d\n",sizeof(kconnection));
	//printf("sizeof(KHttpSink)=%d\n",sizeof(KHttpSink));
	//printf("sizeof(KHttpRequest) = %d\n",sizeof(KHttpRequest));
	//printf("sizeof(pthread_mutex_t)=%d\n",sizeof(pthread_mutex_t));
	//printf("sizeof(lock)=%d\n",sizeof(KMutex));
	//test_cache();
	//test_file();
	//test_timematch();
	//test_xml();
	//printf("sizeof(kgl_str_t)=%d\n",sizeof(kgl_str_t));
	//test_ip_map();
	//test_line_file();
#ifdef ENABLE_HTTP2
	test_http2();
#endif
	//selectorManager.onReady(when_selector_manager_ready, NULL);
	kbuf b;
	b.flags = 0;
	test_url_decode();
	test_regex();
	test_htaccess();
	test_container();
	test_vh_container();
#ifdef ENABLE_INPUT_FILTER
	test_dechunk();
#endif
	test_dechunk2();
	test_white_list();
	//test_http_parser();
	//printf("sizeof(KHttpRequest)=%d\n",sizeof(KHttpRequest));
	//	test_pipe();
	printf("sizeof(obj)=%d,%d\n", (int)sizeof(KHttpObject), (int)sizeof(HttpObjectIndex));
	printf("sizeof(selectable)=[%d]\n", (int)sizeof(kselectable));
	time_t nowTime = time(NULL);
	char timeStr[41];
	mk1123time(nowTime, timeStr, sizeof(timeStr) - 1);
	//printf("parse1123=%d\n",parse1123time(timeStr));
	assert(kgl_parse_http_time((u_char *)timeStr,40)==nowTime);
	INT64 t = 123;
	char buf[INT2STRING_LEN];
	int2string(t, buf);
	//printf("sizeof(sockaddr_i)=%d\n",sizeof(sockaddr_i));
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
