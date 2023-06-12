package common

import (
	"compress/gzip"
	"crypto/md5"
	"fmt"
	"io"
	"math/rand"
	"net/http"
	"os"
	"strings"
	"test_framework/config"
	"time"
)

const (
	TEST_SERVER_NAME = "test_server/1.0"
)

var RequestCount int
var RangeSize, GzRangeSize int
var RangeMd5 string

type RangeCallBackCheck func(from, to, request_count int, r *http.Request, w http.ResponseWriter) bool

var RangeChecker RangeCallBackCheck

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
func HandleRange(w http.ResponseWriter, r *http.Request) {
	var from, to, length int
	var buf []byte
	vary := r.FormValue("vary")
	g := r.FormValue("g")
	if_none_match := r.Header.Get("If-None-Match")
	accept_encoding := r.Header.Get("Accept-Encoding")
	weak_etag := (r.Header.Get("x-weak-etag") == "1")
	gzip := strings.Contains(accept_encoding, "gzip")
	total_content_length := RangeSize
	if g != "1" {
		gzip = false
	}
	if gzip {
		total_content_length = GzRangeSize
	}
	//fmt.Printf("range gzip = [%v]\n", gzip)
	if if_none_match == RangeMd5 || strings.HasPrefix(if_none_match, "W/") {
		//fmt.Printf("response 304\n")
		w.Header().Add("Server", TEST_SERVER_NAME)
		w.WriteHeader(304)
		return
	}
	rv := r.Header.Get("Range")
	if_range := r.Header.Get("If-Range")
	//fmt.Printf("if_range=[%s]\n", if_range)
	if len(if_range) > 0 && if_range != RangeMd5 && !strings.HasPrefix(if_range, "W/") {
		//有变化,回应200
		//fmt.Printf("if_range=[%s] changed\n", if_range)
		rv = ""
	}

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
		if RangeChecker != nil {
			if !RangeChecker(from, to, RequestCount, r, w) {
				return
			}
		}
		if length < 0 {
			status_code = 416
			//w.WriteHeader(416)
			//wb.WriteString("HTTP/1.1 416 Not Satifiy\r\n")
		} else {
			status_code = 206
			//w.WriteHeader(206)
			w.Header().Add("Content-Range", fmt.Sprintf("bytes %d-%d/%d", from, to, total_content_length))
			//wb.WriteString("HTTP/1.1 206 Partcial Content\r\n")
			//wb.WriteString()
		}
	} else {
		//fmt.Printf("rv is empty\n")
		//w.WriteHeader(200)
		//wb.WriteString("HTTP/1.1 200 OK\r\n")
		buf = ReadRange(0, -1, gzip)
		if RangeChecker != nil {
			if !RangeChecker(0, -1, RequestCount, r, w) {
				return
			}
		}
	}
	RequestCount++
	if gzip {
		w.Header().Add("Content-Encoding", "gzip")
	}
	if len(vary) > 0 {
		//fmt.Printf("vary: %s\n", vary)
		w.Header().Add("vary", vary)
		//wb.WriteString(fmt.Sprintf("Vary: %s\r\n", vary))
	}
	w.Header().Add("Server", TEST_SERVER_NAME)
	etag := RangeMd5
	if weak_etag {
		etag = fmt.Sprintf("W/\"%v\"", "hello")
	}
	w.Header().Add("Etag", etag)
	w.Header().Add("Content-Type", "binary/stream")
	//fmt.Printf("response status_code=[%v]\n", status_code)
	if buf != nil {
		w.Header().Add("Content-Length", fmt.Sprintf("%d", len(buf)))
		//fmt.Printf("response len=[%v]\n", len(buf))
	}

	w.WriteHeader(status_code)
	if buf != nil {
		w.Write(buf)
	}
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
		//fmt.Printf("from=[%d] length=[%d],ret=[%d],err=[%s]\n", from, length, ret, err.Error())
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
func CreateRange(size int) {
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
	RangeSize = int(stat.Size())
	stat, err = os.Stat(config.Cfg.BasePath + "/var/range.gz")
	if err != nil {
		panic(err)
	}
	GzRangeSize = int(stat.Size())
	RangeMd5 = fmt.Sprintf("%x", hash.Sum(nil))
}
