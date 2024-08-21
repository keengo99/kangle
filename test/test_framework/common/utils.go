package common

import (
	"bufio"
	"compress/gzip"
	"crypto/md5"
	"fmt"
	"io"
	"net/http"
	"strings"
)

var SkipCheckRespComplete bool

func md5sum(buf []byte) string {
	hash := md5.New()
	hash.Write(buf)
	return fmt.Sprintf("%x", hash.Sum(nil))
}
func Md5File(from, to int, gzip bool) string {
	buf := ReadRange(from, to, gzip)
	if buf != nil {
		//fmt.Printf("md5File=[%s]\n", string(buf))
		result := md5sum(buf)
		//fmt.Printf("result=[%s]\n", result)
		return result
	}
	return ""
}
func MatchEtag(if_none_match string, etag string) bool {
	strs := strings.Split(if_none_match, ",")
	for _, str := range strs {
		str = strings.TrimSpace(str)
		if str == "*" {
			return true
		}
		if str == etag {
			return true
		}
	}
	return false
}
func AssertResp(buf []byte, resp *http.Response) int {
	md5, length := md5Response(resp, false)
	if !SkipCheckRespComplete {
		AssertSame(int(resp.ContentLength), int(length))
	}
	Assert("length", length <= len(buf))
	if length > 0 {
		AssertSame(md5sum(buf[0:length]), md5)
	}
	return length
}
func md5Response(resp *http.Response, decode bool) (string, int) {
	buf := make([]byte, 1024)
	hash := md5.New()
	var total_read int
	content_encoding := resp.Header.Get("Content-Encoding")
	//fmt.Printf("content_encoding: [%v]\n", content_encoding)
	var reader io.Reader
	reader = resp.Body
	if decode && content_encoding == "gzip" {
		reader, _ = gzip.NewReader(resp.Body)
	}
	//fmt.Printf("\n")
	for {
		n, err := reader.Read(buf)
		/*
			if err != nil && err != io.EOF {
				fmt.Printf("content-length=[%v] content_encoding=[%v]\n", resp.ContentLength, content_encoding)
				fmt.Printf("total_read=[%v]\n", total_read)
				panic(err)
			}
		*/
		if n <= 0 {
			break
		}
		total_read += n
		//fmt.Printf("%s", string(buf[0:n]))
		hash.Write(buf[0:n])
		if err != nil {
			break
		}
	}
	//fmt.Printf("\ntotal_read=[%d]\n", total_read)
	//common.AssertSame(total_read, int(resp.ContentLength))
	result := fmt.Sprintf("%x", hash.Sum(nil))
	//fmt.Printf("md5Respons=[%s]\n", result)
	return result, total_read
}
func Md5Response(resp *http.Response, decode bool) string {
	md5, _ := md5Response(resp, decode)
	return md5
}
func ReadHttpProtocol(reader *bufio.Reader, read_body bool) (map[string]string, string, error) {
	headers := make(map[string]string)
	request_line := true
	for {
		line, _, err := reader.ReadLine()
		if err != nil {
			return headers, "", err
		}
		if len(line) == 0 {
			break
		}
		splitChar := ":"
		if request_line {
			splitChar = " "
		}
		request_line = false
		kvs := strings.SplitN(string(line), splitChar, 2)
		http_attr := strings.ToLower(kvs[0])
		_, ok := headers[http_attr]
		if ok {
			if http_attr == "server" || http_attr == "content-length" {
				panic(fmt.Sprintf("http_attr :[%v] already exsit.\n", http_attr))
			}
		}
		headers[http_attr] = strings.TrimSpace(kvs[1])
	}
	if !read_body {
		return headers, "", nil
	}
	line, _, err := reader.ReadLine()
	return headers, string(line), err
}
