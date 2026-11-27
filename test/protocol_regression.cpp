#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include "KDechunkEngine.h"
#include "KHttpParser.h"
#include "KStringBuf.h"
#include "KXml.h"
#include "KXmlDocument.h"
#include "KSockPoolHelper.h"
#include "KHttpField.h"
#include "KFileName.h"
#include "KHttpLib.h"
#include "KHttpHeaderManager.h"
#include "KHttpValidation.h"
#include "KUrl.h"
#include <errno.h>
#include "kselectable.h"

#ifdef ENABLE_KSSL_BIO
static int tls_retries;
static int tls_completions;
static kev_result count_tls_retry(kselector*, kselectable*, result_callback, kgl_iovec*, void* arg) {
    ++tls_retries;
    xfree(arg); // selectable_ssl_read allocated this BIO read buffer.
    return kev_ok;
}
static kev_result complete_tls_error(KOPAQUE, void*, int got) {
    assert(got < 0);
    ++tls_completions;
    return kev_ok;
}
static void check_tls_stale_errno() {
    kgl_bio_init_method();
    SSL_CTX* ctx = SSL_CTX_new(TLS_method());
    assert(ctx);
    kssl_session session = {};
    session.ssl = SSL_new(ctx);
    assert(session.ssl);
    BIO* input = BIO_new(BIO_kgl_method());
    BIO* output = BIO_new(BIO_kgl_method());
    assert(input && output);
    SSL_set_bio(session.ssl, input, output);
    SSL_set_accept_state(session.ssl);
    session.handshake = 1;
    session.bio[OP_READ].bio = input;
    session.bio[OP_WRITE].bio = output;
    // A malformed TLS record must terminate even if a previous BIO left EAGAIN.
    const char invalid[] = "HTTP/1.1 200 OK\r\n\r\n";
    assert(BIO_write(input, invalid, sizeof(invalid) - 1) == sizeof(invalid) - 1);
    kselectable st = {};
    st.ssl = &session;
    char data[16];
    kgl_iovec bufs[2] = {};
    bufs[0].iov_base = (char*)&bufs[1];
    bufs[0].iov_len = 1;
    bufs[1].iov_base = data;
    bufs[1].iov_len = sizeof(data);
    selector_read saved_read = kgl_selector_module.read;
    kgl_selector_module.read = count_tls_retry;
    ERR_clear_error();
    errno = EAGAIN;
    selectable_event_read(&st, complete_tls_error, bufs, NULL);
    kgl_selector_module.read = saved_read;
    assert(tls_retries == 0 && tls_completions == 1);
    SSL_free(session.ssl);
    SSL_CTX_free(ctx);
    ERR_clear_error();
    puts("TLS terminal error with stale EAGAIN passed");
}
#endif

static void check_header_tail() {
    KHttpHeaderManager headers = {};
    assert(headers.add_header("A", 1, "1", 1));
    assert(headers.add_header("B", 1, "2", 1));
    assert(headers.add_header("C", 1, "3", 1));
    KHttpHeader* removed = headers.remove("B", 1);
    assert(removed && removed->next == NULL);
    xfree_header(removed);
    assert(headers.add_header("D", 1, "4", 1));
    removed = headers.remove("D", 1);
    assert(removed && removed->next == NULL);
    xfree_header(removed);
    assert(headers.add_header("E", 1, "5", 1));
    assert(headers.remove("missing", 7) == NULL);
    const char* names[] = {"A", "C", "E"};
    KHttpHeader* h = headers.get_header();
    for (const char* name : names) {
        assert(h && kgl_is_attr(h, name, 1));
        h = h->next;
    }
    assert(!h);
    for (const char* name : names) {
        removed = headers.remove(name, 1);
        assert(removed && removed->next == NULL);
        xfree_header(removed);
    }
    assert(headers.get_header() == NULL);
    assert(headers.add_header("Z", 1, "6", 1));
    assert(headers.get_header() && !headers.get_header()->next);
    free_header_list(headers.steal_header());
}

static void check_node_recovery() {
    const time_t saved_time = kgl_current_sec;
    kgl_current_sec = 1000;
    KSockPoolHelper node;
    node.setErrorTryTime(1, 5);
    node.health(NULL, HealthStatus::Err);
    assert(!node.is_enabled() && !node.is_available());
    kgl_current_sec = 1005;
    std::atomic<int> probes(0);
    std::vector<std::thread> threads;
    for (int i = 0; i < 32; ++i) {
        threads.emplace_back([&]() { if (node.is_available()) ++probes; });
    }
    for (auto& thread : threads) thread.join();
    assert(probes == 1 && !node.is_enabled());
    node.health(NULL, HealthStatus::Success);
    assert(node.is_enabled() && node.is_available());
    kgl_current_sec = saved_time;
}

static void check_fields_and_dates() {
    char input[] = "bare ; next = value, third=last";
    http_field_t field;
    char* next = field.parse(input, ';');
    assert(std::string(field.attr) == "bare" && field.val == NULL);
    field.parse(next, ';');
    assert(std::string(field.attr) == "next" && std::string(field.val) == "value");
    KHttpHeader* header = new_http_know_header(kgl_header_content_type, "", 0);
    assert(header && header->val_len == 0);
    free_header_list(header);
    char date[32];
    memset(date, 'x', sizeof(date));
    mk1123time(0, date, 30);
    assert(std::string(date) == "Thu, 01 Jan 1970 00:00:00 GMT" && date[30] == 'x');
    mk1123time(0, date, 1);
    assert(date[0] == '\0');
    time_t parsed = 123;
    for (const char* invalid : {"", "invalid", "0", "-1",
            "Thu, 32 Jan 2026 00:00:00 GMT", "Thu, 01 Jan 2026 25:00:00 GMT"}) {
        assert(kgl_parse_value_time(invalid, (int)strlen(invalid), &parsed) != 0);
        assert(parsed == 123); // Failure must not publish the -1 sentinel.
    }
    time_t invalid_time = -1;
    assert(kgl_parse_value_time((const char*)&invalid_time, KGL_HEADER_VALUE_TIME, &parsed) != 0);
    assert(parsed == 123);
    assert(kgl_parse_value_time("Thu, 01 Jan 1970 00:00:00 GMT", 29, &parsed) == 0 && parsed == 0);
    assert(kgl_parse_value_time("Sun, 06 Nov 1994 08:49:37 GMT", 29, &parsed) == 0 && parsed == 784111777);
    time_t valid_time = 784111777;
    assert(kgl_parse_value_time((const char*)&valid_time, KGL_HEADER_VALUE_TIME, &parsed) == 0 && parsed == valid_time);
}

static void check_cache_flush_failure() {
#ifdef __linux__
    KBufferFile file;
    assert(file.open("/dev/full", fileWrite));
    int len;
    char* buffer = file.get_buffer(&len);
    memset(buffer, 0, len);
    // write_success flushes implicitly and has no result parameter.
    file.write_success(len);
    assert(file.write("x", 1) == -1);
    assert(!file.close());
#endif
}

static KDechunkResult decode(const std::string& input) {
    KDechunkEngine engine;
    const char* hot = input.data();
    const char* end = hot + input.size();
    for (;;) {
        const char* piece = NULL;
        int len = KHTTPD_MAX_CHUNK_SIZE;
        KDechunkResult result = engine.dechunk(&hot, end, &piece, &len);
        if (result != KDechunkResult::Success && result != KDechunkResult::Trailer) {
            return result;
        }
    }
}

static void check_dechunk_limits() {
    assert(decode("100000000\r\n") == KDechunkResult::Failed);
    assert(decode("20000000\r\n") == KDechunkResult::Failed);
    assert(decode("1fffffff\r\n") == KDechunkResult::Continue);
    assert(decode("1\r\nx\r\n0\r\nX-Test: ok\r\n\r\n") == KDechunkResult::End);
    assert(decode("0\r\nX-Test: " + std::string(16385, 'a') + "\r\n\r\n") == KDechunkResult::Failed);
    assert(decode("0\r\n" + std::string(16385, 'a')) == KDechunkResult::Failed);
    std::string trailers;
    for (int i = 0; i < KHTTPD_MAX_TRAILER_COUNT; ++i) {
        trailers += "X-Test: ok\r\n";
    }
    assert(decode("0\r\n" + trailers + "\r\n") == KDechunkResult::End);
    assert(decode("0\r\n" + trailers + "X-Test: extra\r\n\r\n") == KDechunkResult::Failed);
    assert(decode("zz\r\nabc") == KDechunkResult::Failed);
    assert(decode("\r\n") == KDechunkResult::Failed);
    assert(decode("\n") == KDechunkResult::Failed);
    assert(decode("1\r\nx\r\n;ext\r\n") == KDechunkResult::Failed);
    assert(decode("1;ext=1\r\nx\r\n0\r\n\r\n") == KDechunkResult::End);
    assert(decode("0\r\nX\r\n") == KDechunkResult::Failed);
    assert(decode("0\r\nX\r\n\r\n") == KDechunkResult::Failed);
    for (const char* input : {"1\nx\n0\n\n", "1\r\nxBAD\n0\r\n\r\n", "1garbage\r\nx\r\n0\r\n\r\n",
            "0\r\nContent-Length: 0\r\n\r\n", "0\r\nX : bad\r\n\r\n", "1;x=\"unclosed\r\n"}) {
        assert(decode(input) == KDechunkResult::Failed);
    }
    KDechunkEngine fragmented;
    std::string input="1;foo=\"a\\\"b\"\r\nx\r\n0\r\ngrpc-status: 0\r\n\r\nNEXT";
    bool ended=false;
    std::string body, trailer;
    for (size_t i=0; i<input.size() && !ended; ++i) {
        const char* hot=input.data()+i;
        const char* end=hot+1;
        while (hot < end) {
            const char* piece=nullptr;
            int len=32;
            auto result=fragmented.dechunk(&hot,end,&piece,&len);
            assert(result!=KDechunkResult::Failed);
            if (result==KDechunkResult::Success) body.append(piece,len);
            if (result==KDechunkResult::Trailer) trailer.assign(piece,len);
            if (result==KDechunkResult::End) ended=true;
        }
    }
    assert(ended && body=="x" && trailer=="grpc-status: 0");
}

static void check_protocol_validation() {
    KHttpHeaderValidation headers{};
    assert(headers.field(_KS(":method"), _KS("POST"), true, true));
    assert(headers.field(_KS(":scheme"), _KS("https"), true, true));
    assert(headers.field(_KS(":path"), _KS("/grpc.Service/Call"), true, true));
    assert(headers.field(_KS(":authority"), _KS("localhost"), true, true));
    assert(headers.field(_KS("te"), _KS("trailers"), true, true));
    assert(headers.complete(true));
    assert(!headers.field(_KS(":path"), _KS("/duplicate"), true, true));
    for (const char* name : {"connection", "keep-alive", "transfer-encoding", "upgrade", "X-Upper"}) {
        headers.reset();
        assert(!headers.field(name, (int)strlen(name), _KS("bad"), true, true));
    }
    assert(khttp_valid_trailer(_KS("grpc-status"), _KS("0"), true));
    assert(!khttp_valid_trailer(_KS("host"), _KS("evil")));
    int64_t n;
    assert(khttp_parse_length(_KS("9223372036854775807"), &n) && n==INT64_MAX);
    assert(!khttp_parse_length(_KS("9223372036854775808"), &n));
    for (const char* host : {"", "x:0", "x:65536", "x:80garbage", "x:", "x@y", "x/path", "[::1]junk", "::1"}) {
        KUrl url(false);
        assert(!parse_url_host(&url,host,strlen(host)));
    }
    for (const char* host : {"localhost", "example.test:443", "[::1]:80"}) {
        KUrl url(false);
        assert(parse_url_host(&url,host,strlen(host)));
    }
    for (const char* text : {"http://127.0.0.1:0/", "http://-:0/", "http://[::1]:0/", "https://example.test:0/path?q=1"}) {
        KUrl strict(false), upstream(false);
        assert(!parse_url(text, &strict));
        assert(parse_upstream_url(text, &upstream));
        assert(upstream.port == 0 && KBIT_TEST(upstream.flags, KGL_URL_HAS_PORT));
        assert(upstream.host && upstream.path);
    }
    for (const char* text : {"http://x:65536/", "http://x:0garbage/", "http://x:/", "http://x@y:0/"}) {
        KUrl upstream(false);
        assert(!parse_upstream_url(text, &upstream));
    }
}

static void check_header_names() {
    const char* invalid[] = {"Transfer-Encoding ", "Transfer-Encoding\t", "x/name", ""};
    for (const char* name : invalid) {
        std::string input = std::string(name) + ": chunked";
        std::vector<char> buffer(input.begin(), input.end());
        buffer.push_back(0);
        khttp_parser parser = {};
        khttp_parse_result rs = {};
        assert(khttp_parse_header(&parser, buffer.data(), buffer.data() + input.size(), &rs) == kgl_parse_success);
        assert(rs.bad_name);
    }
    char embedded_nul[] = "Transfer\0-Encoding: chunked";
    khttp_parser parser = {};
    khttp_parse_result rs = {};
    assert(khttp_parse_header(&parser, embedded_nul, embedded_nul + sizeof(embedded_nul) - 1, &rs) == kgl_parse_success);
    assert(rs.bad_name);
}

static void check_fragmented_fold() {
    std::string input = "GET / HTTP/1.1\r\nX-Test: first";
    for (int i = 0; i < 1000; ++i) {
        input += "\r\n next";
    }
    input += "\r\nHost: localhost\r\n\r\n";
    std::vector<char> buffer(input.begin(), input.end());
    buffer.push_back(0);
    char* hot = buffer.data();
    khttp_parser parser = {};
    int headers = 0;
    for (size_t length = 1; length <= input.size(); ++length) {
        for (;;) {
            khttp_parse_result rs = {};
            kgl_parse_result result = khttp_parse(&parser, &hot, buffer.data() + length, &rs);
            assert(result != kgl_parse_error);
            if (result == kgl_parse_success) {
                assert(!rs.bad_name);
                ++headers;
                if (headers == 2) {
                    assert(rs.attr_len == 6 && std::string(rs.attr) == "X-Test");
                    assert(rs.val_len == 5 + 1000 * 7);
                    assert(memchr(rs.val, '\r', rs.val_len) == NULL);
                    assert(memchr(rs.val, '\n', rs.val_len) == NULL);
                }
                continue;
            }
            break;
        }
    }
    assert(headers == 3 && parser.finished && parser.header_len == (int)input.size());
}

static void check_xml() {
    using namespace khttpd;
    for (const char* text : {"<tag>]]>&amp;&", "plain &amp; &"}) {
        KXmlDocument document;
        char xml[] = "<config><item/></config>";
        KSafeXmlNode root = document.parse(xml);
        assert(root);
        root->find_child("item")->get_first()->set_text(text);
        KStringBuf output;
        assert(root->write(&output) == KGL_OK);
        std::string serialized(output.c_str());
        KXmlDocument again;
        KSafeXmlNode result = again.parse(&serialized[0]);
        assert(result && result->find_child("item")->get_first()->get_text() == KString(text));
        KXmlKey key("item", 4);
        KSafeXmlNode extra(new KXmlNode(&key));
        assert(root->get_first()->add(extra.get(), 1000));
        assert(root->find_child("item")->get_body_count() == 2);
    }
    std::string deep;
    for (int i = 0; i < KXML_MAX_DEPTH + 2; ++i) deep += "<n>";
    for (int i = 0; i < KXML_MAX_DEPTH + 2; ++i) deep += "</n>";
    KXmlDocument document;
    assert(!document.parse(&deep[0]));
    KXmlDocument malformed;
    char xml[] = "<config><!--";
    assert(!malformed.parse(xml));
}

int main() {
#ifdef ENABLE_KSSL_BIO
    check_tls_stale_errno();
#endif
    check_header_tail();
    check_node_recovery();
    check_fields_and_dates();
    check_cache_flush_failure();
    check_dechunk_limits();
    check_protocol_validation();
    check_header_names();
    check_fragmented_fold();
    check_xml();
    puts("protocol regression tests passed");
    return 0;
}
