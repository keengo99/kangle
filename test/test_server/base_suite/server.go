package base_suite

import (
	"compress/gzip"
	"crypto/md5"
	"fmt"
	"io"
	"math/rand"
	"net/http"
	"os"
	"strings"
	"test_server/common"
	"test_server/config"
	"time"
)

const (
	TEST_SERVER_NAME = "test_server/1.0"
	SPLIT_RESPONSE   = "HTTP/1.1 200 OK\r\nCache-Control: no-cache,no-store\r\nConnection: close\r\nContent-Length: 2\r\n\r\nOK"
)

var request_count int
var range_size, gz_range_size int
var range_md5 string
var last_dynamic_content string

type rangeCallBackCheck func(from, to, request_count int, r *http.Request)

var rangeChecker rangeCallBackCheck

func parseRange(rv string) (from, to int) {
	n, _ := fmt.Sscanf(rv, "bytes=-%d", &to)
	if n == 1 {
		from = -1
		return
	}
	n, _ = fmt.Sscanf(rv, "bytes=%d-%d", &from, &to)
	if n == 1 {
		to = -1
	}
	return
}
func HandleGzip(w http.ResponseWriter, r *http.Request) {
	if_none_match := r.Header.Get("If-None-Match")
	request_count++
	if if_none_match == "gzip" {
		w.WriteHeader(304)
		w.Header().Add("Server", TEST_SERVER_NAME)
		return
	}
	w.Header().Add("Etag", "gzip")
	w.Header().Add("X-Gzip", "on")
	w.Header().Add("Server", TEST_SERVER_NAME)
	w.Write([]byte("gzipgzipgzip"))
	return
}
func HandleBr(w http.ResponseWriter, r *http.Request) {
	if_none_match := r.Header.Get("If-None-Match")
	request_count++
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
	request_count++
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
	return
}
func HandleDynamic(w http.ResponseWriter, r *http.Request) {
	last_dynamic_content = fmt.Sprintf("hello_%v", request_count)
	w.Header().Add("Etag", last_dynamic_content)
	w.Header().Add("Server", TEST_SERVER_NAME)
	w.Write([]byte(last_dynamic_content))
	request_count++
}
func HandleNoCache(w http.ResponseWriter, r *http.Request) {
	w.Header().Add("Cache-Control", "no-cache,no-store")
	w.Header().Add("Server", TEST_SERVER_NAME)
	w.Write([]byte("OK"))
}
func HandleEtag(w http.ResponseWriter, r *http.Request) {
	if_none_match := r.Header.Get("If-None-Match")
	if if_none_match == "" {
		request_count++
		w.Header().Add("Server", TEST_SERVER_NAME)
		w.Header().Add("Etag", "hello")
		w.Write([]byte("hello"))
		return
	}
	common.Assert("if-none-match-1", if_none_match == "hello")
	request_count++
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
func HandleRange(w http.ResponseWriter, r *http.Request) {
	var from, to, length int
	var buf []byte
	vary := r.FormValue("vary")
	g := r.FormValue("g")
	if_none_match := r.Header.Get("If-None-Match")
	accept_encoding := r.Header.Get("Accept-Encoding")
	gzip := strings.Contains(accept_encoding, "gzip")
	total_content_length := range_size
	if g != "1" {
		gzip = false
	}
	if gzip {
		total_content_length = gz_range_size
	}
	//fmt.Printf("range gzip = [%v]\n", gzip)
	if if_none_match == range_md5 {
		fmt.Printf("304\n")
		w.Header().Add("Server", TEST_SERVER_NAME)
		w.WriteHeader(304)
		return
	}
	rv := r.Header.Get("Range")
	if_range := r.Header.Get("If-Range")
	//fmt.Printf("if_range=[%s]\n", if_range)
	if len(if_range) > 0 && if_range != range_md5 {
		//有变化,回应200
		//fmt.Printf("if_range=[%s] changed\n", if_range)
		rv = ""
	}

	//hj, _ := w.(http.Hijacker)

	//cn, wb, _ := hj.Hijack()
	//defer cn.Close()

	//fmt.Printf("rv=[%v]\n", rv)
	status_code := 200
	if len(rv) > 0 {
		from, to = parseRange(rv)
		//fmt.Printf("from=[%d] to=[%d]\n", from, to)
		if from == -1 {
			from = total_content_length - to
			to = total_content_length - 1
		}
		//fmt.Printf("adjust from=[%d] to=[%d],length=[%d]\n", from, to, to-from+1)
		buf = ReadRange(from, to, gzip)
		if rangeChecker != nil {
			rangeChecker(from, to, request_count, r)
		}
		if length < 0 {
			status_code = 416
			//w.WriteHeader(416)
			//wb.WriteString("HTTP/1.1 416 Not Satifiy\r\n")
		} else {
			status_code = 206
			//w.WriteHeader(206)
			w.Header().Add("Content-Range", fmt.Sprintf("%d-%d/%d", from, to, total_content_length))
			//wb.WriteString("HTTP/1.1 206 Partcial Content\r\n")
			//wb.WriteString()
		}
	} else {
		//fmt.Printf("rv is empty\n")
		if rangeChecker != nil {
			rangeChecker(0, -1, request_count, r)
		}
		//w.WriteHeader(200)
		//wb.WriteString("HTTP/1.1 200 OK\r\n")
		buf = ReadRange(0, -1, gzip)
	}
	request_count++
	if gzip {
		w.Header().Add("Content-Encoding", "gzip")
	}
	if len(vary) > 0 {
		fmt.Printf("vary: %s\n", vary)
		w.Header().Add("vary", vary)
		//wb.WriteString(fmt.Sprintf("Vary: %s\r\n", vary))
	}
	w.Header().Add("Server", TEST_SERVER_NAME)
	w.Header().Add("Etag", range_md5)
	w.Header().Add("Content-Type", "binary/stream")
	if buf != nil {
		w.Header().Add("Content-Length", fmt.Sprintf("%d", len(buf)))
	}
	w.WriteHeader(status_code)
	if buf != nil {
		w.Write(buf)
	}
}
func randInt(min int, max int) int {
	return min + rand.Intn(max-min)
}
func ReadFileRange(filename string, from, to int) []byte {
	file, err := os.Open(filename)
	if err != nil {
		panic(err)
	}
	defer file.Close()
	if to == -1 {
		fi, _ := file.Stat()
		to = (int)(fi.Size() - 1)
	}
	length := to - from + 1
	if length < 0 {
		return nil
	}
	//fmt.Printf("length=[%d]\n", length)
	buffer := make([]byte, length)
	file.Seek(int64(from), 0)
	ret, err := io.ReadFull(file, buffer)
	if length != ret {
		fmt.Printf("from=[%d] length=[%d],ret=[%d],err=[%s]\n", from, length, ret, err.Error())
		panic("length error")
	}
	if err != nil {
		panic(err)
	}
	return buffer
}
func ReadRange(from, to int, gzip bool) []byte {
	if gzip {
		return ReadFileRange(config.Cfg.BasePath+"/var/range.gz", from, to)
	}
	return ReadFileRange(config.Cfg.BasePath+"/var/range", from, to)
}

const letterBytes = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"

func RandStringBytes(n int) []byte {
	b := make([]byte, n)
	for i := range b {
		b[i] = letterBytes[rand.Intn(len(letterBytes))]
	}
	return b
}
func createRange(size int) {
	os.Mkdir(config.Cfg.BasePath+"/var", 0755)
	file, err := os.OpenFile(config.Cfg.BasePath+"/var/range",
		os.O_WRONLY|os.O_CREATE|os.O_TRUNC,
		0600)
	if err != nil {
		panic(err)
	}

	gz_fp, err := os.OpenFile(config.Cfg.BasePath+"/var/range.gz",
		os.O_CREATE|os.O_TRUNC|os.O_WRONLY,
		0600)
	if err != nil {
		panic(err)
	}
	gz_stream := gzip.NewWriter(gz_fp)

	rand.Seed(time.Now().UTC().UnixNano())
	//rand.Seed(111)
	hash := md5.New()
	for i := 0; i < size; i++ {
		temp := RandStringBytes(1024)
		hash.Write(temp)
		file.Write(temp)
		gz_stream.Write(temp)
	}
	hash.Write([]byte("EEE"))
	file.WriteString("EEE")
	gz_stream.Write([]byte("EEE"))
	file.Close()
	gz_stream.Close()
	gz_fp.Close()

	stat, err := os.Stat(config.Cfg.BasePath + "/var/range")
	if err != nil {
		panic(err)
	}
	range_size = int(stat.Size())
	stat, err = os.Stat(config.Cfg.BasePath + "/var/range.gz")
	if err != nil {
		panic(err)
	}
	gz_range_size = int(stat.Size())
	range_md5 = fmt.Sprintf("%x", hash.Sum(nil))
}
