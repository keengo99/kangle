package base_suite

import (
	"fmt"
	"math/rand"
	"net/http"
	"strings"
	"test_server/common"
)

var last_dynamic_content string

const (
	TEST_SERVER_NAME = "test_server/1.0"
	SPLIT_RESPONSE   = "HTTP/1.1 200 OK\r\nCache-Control: no-cache,no-store\r\nConnection: close\r\nContent-Length: 2\r\n\r\nOK"
)

func HandleGzip(w http.ResponseWriter, r *http.Request) {
	if_none_match := r.Header.Get("If-None-Match")
	common.RequestCount++
	if if_none_match == "gzip" {
		w.WriteHeader(304)
		w.Header().Add("Server", TEST_SERVER_NAME)
		return
	}
	w.Header().Add("Etag", "gzip")
	w.Header().Add("X-Gzip", "on")
	w.Header().Add("Server", TEST_SERVER_NAME)
	w.Write([]byte("gzipgzipgzip"))
}
func HandleBr(w http.ResponseWriter, r *http.Request) {
	if_none_match := r.Header.Get("If-None-Match")
	common.RequestCount++
	if if_none_match == "br" {
		w.WriteHeader(304)
		w.Header().Add("Server", TEST_SERVER_NAME)
		return
	}
	w.Header().Add("Server", TEST_SERVER_NAME)
	w.Header().Add("Etag", "br")
	accept_encoding := r.Header.Get("Accept-Encoding")
	if strings.Contains(accept_encoding, "br") {
		w.Header().Add("Content-Encoding", "br")
		w.Write([]byte("br"))
	} else if strings.Contains(accept_encoding, "unknow") {
		w.Header().Add("Content-Encoding", "unknow")
		w.Write([]byte("unknow"))
	} else {
		w.Write([]byte("plain"))
	}
	return
}
func HandleGzipBr(w http.ResponseWriter, r *http.Request) {
	if_none_match := r.Header.Get("If-None-Match")
	common.RequestCount++
	if if_none_match == "gzip_br" {
		w.WriteHeader(304)
		w.Header().Add("Server", TEST_SERVER_NAME)
		return
	}
	w.Header().Add("Server", TEST_SERVER_NAME)
	w.Header().Add("Etag", "gzip_br")
	accept_encoding := r.Header.Get("Accept-Encoding")
	//fmt.Printf("accept_encoding=[%s]\n", accept_encoding)
	if strings.Contains(accept_encoding, "br") {
		w.Header().Add("Content-Encoding", "br")
		w.Write([]byte("br"))
	} else if strings.Contains(accept_encoding, "gzip") {
		w.Header().Add("Content-Encoding", "gzip")
		w.Write([]byte("gzip"))
	} else if strings.Contains(accept_encoding, "deflate") {
		w.Header().Add("Content-Encoding", "deflate")
		w.Write([]byte("deflate"))
	} else {
		w.Write([]byte("plain"))
	}
}
func HandleDynamic(w http.ResponseWriter, r *http.Request) {
	last_dynamic_content = fmt.Sprintf("hello_%v", common.RequestCount)
	w.Header().Add("Etag", last_dynamic_content)
	w.Header().Add("Server", TEST_SERVER_NAME)
	w.Write([]byte(last_dynamic_content))
	common.RequestCount++
}
func HandleNoCache(w http.ResponseWriter, r *http.Request) {
	w.Header().Add("Cache-Control", "no-cache,no-store")
	w.Header().Add("Server", TEST_SERVER_NAME)
	w.Write([]byte("OK"))
}
func HandleEtag(w http.ResponseWriter, r *http.Request) {
	if_none_match := r.Header.Get("If-None-Match")
	if if_none_match == "" {
		common.RequestCount++
		w.Header().Add("Server", TEST_SERVER_NAME)
		w.Header().Add("Etag", "hello")
		w.Write([]byte("hello"))
		return
	}
	common.Assert("if-none-match-1", if_none_match == "hello")
	common.RequestCount++
	w.WriteHeader(304)
	w.Header().Add("Server", TEST_SERVER_NAME)
}
func HandleChunkPost(w http.ResponseWriter, r *http.Request) {
	buf := make([]byte, 512)
	for {
		n, _ := r.Body.Read(buf)
		if n > 0 {
			w.Write(buf[0:n])
		} else {
			break
		}
	}
	w.Write([]byte("\n"))
}
func HandleHole(w http.ResponseWriter, r *http.Request) {
	hj, _ := w.(http.Hijacker)
	cn, _, _ := hj.Hijack()
	defer cn.Close()
	for {
		buf := make([]byte, 512)
		n, err := cn.Read(buf)
		if err != nil {
			break
		}
		if n <= 0 {
			break
		}
	}
}

func HandleVary(w http.ResponseWriter, r *http.Request) {
	vary := r.Header.Get("x-set-vary")
	if len(vary) > 0 {
		w.Header().Add("Vary", vary)
		w.Header().Add("Etag", vary)
	} else {
		w.Header().Add("Etag", "no-vary")
	}
	w.Header().Add("Server", TEST_SERVER_NAME)
	if len(vary) > 0 {
		vary_val := r.Header.Get(vary)
		w.Write([]byte(vary_val))
	} else {
		w.Write([]byte("no-vary"))
	}
}
func HandleMissStatusString(w http.ResponseWriter, r *http.Request) {
	hj, _ := w.(http.Hijacker)
	cn, wb, _ := hj.Hijack()
	wb.WriteString("HTTP/1.1 200\r\nConnection: close\r\n\r\nok")
	wb.Flush()
	cn.Close()
}
func randInt(min int, max int) int {
	return min + rand.Intn(max-min)
}
