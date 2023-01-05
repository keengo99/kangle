package base_suite

import (
	"crypto/md5"
	"fmt"
	"hash"
	"io"
	"io/ioutil"
	"net"
	"net/http"
	"strconv"
	"test_server/common"
	"test_server/config"

	"strings"
	"time"
)

type headerReader struct {
	reader io.Reader
	md5    hash.Hash
	header http.Header
}

func (r *headerReader) Read(p []byte) (n int, err error) {
	n, err = r.reader.Read(p)
	if n > 0 {
		r.md5.Write(p[:n])
	}
	if err == io.EOF {
		r.header.Set("md5", fmt.Sprintf("%x", r.md5.Sum(nil)))
	}
	return
}
func check_chunk_post() {
	str := "PUT /chunk_post HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n2\r\nte\r\n2\r\nst\r\n0\r\n\r\n"
	cn, err := net.Dial("tcp", "127.0.0.1:9999")
	common.Assert("err", err == nil)
	defer cn.Close()
	cn.Write([]byte(str))
	cn.SetReadDeadline(time.Now().Add(2 * time.Second))
	buf, _ := ioutil.ReadAll(cn)
	str = string(buf)
	//fmt.Printf("resp=[%v]\n", str)
	common.Assert("chunk_post", strings.Index(str, "\ntest") > 0)
}
func HandlePostChunkTrailer(w http.ResponseWriter, r *http.Request) {
	//fmt.Printf("header: %+v\n", r.Header)
	//fmt.Printf("trailer before read body: %+v\n", r.Trailer)
	data, _ := ioutil.ReadAll(r.Body)
	bodyMd5 := fmt.Sprintf("%x", md5.Sum(data))
	//fmt.Printf("body: %v,body md5: %v, err: %v\n", string(data), bodyMd5, err)
	//fmt.Printf("trailer after read body: %+v\n", r.Trailer)
	if r.Trailer.Get("md5") != bodyMd5 {
		w.Write([]byte("body md5 not equal trailer md5 is "))
		w.Write([]byte(r.Trailer.Get("md5")))
		return
	}
	w.Write([]byte("ok"))
}
func HandleChunkUpstreamTrailer(w http.ResponseWriter, r *http.Request) {
	flush, _ := w.(http.Flusher)
	strs := "MozillaDeveloperNetwork"
	w.Header().Add("Trailer", "x-trailer-test")
	w.Header().Add("Content-Type", "text/plain")

	tmp, _ := strconv.ParseInt(r.FormValue("s"), 10, 32)
	s := int(tmp)
	if s == 0 {
		w.Write([]byte(strs))
		flush.Flush()
		//time.Sleep(time.Millisecond * 1)
	} else {
		pos := 0
		length := len(strs)
		for pos < length {
			split_length := length - pos
			if split_length > s {
				split_length = s
			}
			str := strs[pos : pos+split_length]
			w.Write([]byte(str))
			flush.Flush()
			//time.Sleep(time.Millisecond * 1)
			pos += split_length
		}
	}
	w.Header().Add("x-trailer-test", "hello")
}
func HandleHttp1ChunkUpstreamTrailer(w http.ResponseWriter, r *http.Request) {
	hj, _ := w.(http.Hijacker)
	cn, wb, _ := hj.Hijack()
	strs := `HTTP/1.1 200 OK
Content-Type: text/plain
Transfer-Encoding: chunked
Trailer: x-trailer-test

7
Mozilla
9
Developer
7
Network
0
x-trailer-test: hello

`
	tmp, _ := strconv.ParseInt(r.FormValue("s"), 10, 32)
	s := int(tmp)
	if s == 0 {
		wb.WriteString(strs)
		wb.Flush()
	} else {
		pos := 0
		length := len(strs)
		for pos < length {
			split_length := length - pos
			if split_length > s {
				split_length = s
			}
			str := strs[pos : pos+split_length]
			wb.WriteString(str)
			wb.Flush()
			//time.Sleep(time.Millisecond * 1)
			pos += split_length
		}
	}
	cn.Close()
}
func HandleChunkUpstream(w http.ResponseWriter, r *http.Request) {
	hj, _ := w.(http.Hijacker)
	cn, wb, _ := hj.Hijack()
	wb.WriteString("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n2\r\nok\r\n0\r\n\r\n")
	wb.Flush()
	cn.Close()
}
func check_chunk_upstream() {
	if config.Cfg.UpstreamHttp2 {
		fmt.Printf("skip test. this test is not support upstream h2...\n")
		return
	}
	common.Get("/chunk", nil, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "ok")
	})
}
func check_post_chunk_trailer_with_path(path string) {
	h := &headerReader{
		reader: strings.NewReader("body"),
		md5:    md5.New(),
		header: http.Header{"md5": nil, "size": []string{strconv.Itoa(len("body"))}},
	}
	url := config.GetUrl("localhost", path)

	req, err := http.NewRequest("POST", url, h)
	if err != nil {
		panic(err)
	}
	req.ContentLength = -1
	req.Trailer = h.header
	resp, err := common.GetClient(url).Do(req)
	if err != nil {
		panic(err)
	}
	common.AssertSame(common.Read(resp), "ok")
}
func check_split_upstream_chunk_trailer(split int) {
	common.Getx(fmt.Sprintf("/upstream/h2/chunk_trailer?s=%v", split), "localhost", nil, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "MozillaDeveloperNetwork")
		common.AssertSame(resp.Trailer.Get("x-trailer-test"), "hello")
	})

	common.Getx(fmt.Sprintf("/upstream/http/chunk_trailer?s=%v", split), "localhost", nil, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "MozillaDeveloperNetwork")
		common.AssertSame(resp.Trailer.Get("x-trailer-test"), "hello")
	})
	//http1 upstream chunk split response.
	common.Getx(fmt.Sprintf("/upstream/http/chunk_trailer_1?s=%v", split), "localhost", nil, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "MozillaDeveloperNetwork")
		common.AssertSame(resp.Trailer.Get("x-trailer-test"), "hello")
	})
	common.Getx(fmt.Sprintf("/upstream/http/chunk_trailer?s=%v", split), "localhost", nil, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "MozillaDeveloperNetwork")
		common.AssertSame(resp.Trailer.Get("x-trailer-test"), "hello")
	})
	common.Getx(fmt.Sprintf("/upstream/h2/chunk_trailer?s=%v", split), "localhost", nil, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "MozillaDeveloperNetwork")
		common.AssertSame(resp.Trailer.Get("x-trailer-test"), "hello")
	})

}
func check_chunk_trailer() {
	if config.Cfg.Alpn == config.HTTP3 {
		fmt.Printf("http3 current is not support trailer. skip test..\n")
		return
	}
	check_split_upstream_chunk_trailer(0)
	check_split_upstream_chunk_trailer(2)
	check_post_chunk_trailer_with_path("/upstream/http/post_chunk_trailer")
	check_post_chunk_trailer_with_path("/upstream/h2/post_chunk_trailer")
}
