package common

import (
	"compress/gzip"
	"crypto/md5"
	"fmt"
	"io"
	"net/http"
)

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
func Md5Response(resp *http.Response, decode bool) string {
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
	return result
}
