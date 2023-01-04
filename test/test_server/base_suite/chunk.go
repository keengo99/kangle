package base_suite

import (
	"crypto/md5"
	"fmt"
	"hash"
	"io"
	"io/ioutil"
	"net"
	"net/http"
	"os"
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
	str := fmt.Sprintf("PUT /chunk_post HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n2\r\nte\r\n2\r\nst\r\n0\r\n\r\n")
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
func check_http2_post_form() {
	common.Post("/chunk_post", map[string]string{"Transfer-Encoding": "chunked", "Content-Type": "application/x-www-form-urlencoded"}, "a=OK", func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "OK")
	})
}
func check_chunk_post_form() {
	//fmt.Printf("check_chunk_post_form...\n")
	str := "POST /a.php HTTP/1.1\r\nContent-Type: application/x-www-form-urlencoded\r\nTransfer-Encoding: chunked\r\nHost: localhost\r\n\r\n10\r\na=OK0123456789ab\r\n2\r\ncd\r\n0\r\n\r\nGET /kangle.status HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"
	//*
	l := len(str)
	for i := 100; i < l-40; i++ {
		check_split_post(str, i, func(buf string) {
			common.Assert(fmt.Sprintf("chunk_post_form_%d", i), strings.Index(buf, "\nOK0123456789abcd") > 0 && strings.Index(buf, "\nOK") != strings.LastIndex(buf, "\nOK"))
		})

	}
	////*/
	//check_split_post(str, 119)
	/*
		post("/a.php", map[string]string{"Transfer-Encoding": "chunked", "Content-Type": "application/x-www-form-urlencoded"}, "a=b", func(resp *http.Response, err error) {
			AssertSame(read(resp), "b")
		})
		/*
			str := fmt.Sprintf("POST /a.php HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/x-www-form-urlencoded\r\nTransfer-Encoding: chunked\r\n\r\n3\r\na=b\r\n0\r\n\r\n")
			cn, err := net.Dial("tcp", "127.0.0.1:9999")
			Assert("err", err == nil)
			defer cn.Close()
			cn.Write([]byte(str))
			cn.SetReadDeadline(time.Now().Add(2 * time.Second))
			buf, _ := ioutil.ReadAll(cn)
			str = string(buf)
			fmt.Printf("resp=[%v]\n", str)
			Assert("chunk_post", strings.Index(str, "\nb") > 0)
	*/
}
func HandlePostChunkTrailer(w http.ResponseWriter, r *http.Request) {
	fmt.Printf("header: %+v\n", r.Header)
	fmt.Printf("trailer before read body: %+v\n", r.Trailer)
	data, err := ioutil.ReadAll(r.Body)
	bodyMd5 := fmt.Sprintf("%x", md5.Sum(data))
	fmt.Printf("body: %v,body md5: %v, err: %v\n", string(data), bodyMd5, err)
	fmt.Printf("trailer after read body: %+v\n", r.Trailer)
	if r.Trailer.Get("md5") != bodyMd5 {
		panic("body md5 not equal")
	}

}
func HandleChunkUpstreamTrailer(w http.ResponseWriter, r *http.Request) {
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
			time.Sleep(time.Millisecond * 10)
			pos += split_length
		}
	}
	cn.Close()
}
func HandleChunkUpstreamSplitTrailer(w http.ResponseWriter, r *http.Request) {
	hj, _ := w.(http.Hijacker)
	cn, wb, _ := hj.Hijack()

	wb.WriteString(`HTTP/1.1 200 OK
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
x-trailer-`)
	wb.Flush()
	time.Sleep(time.Millisecond * 50)
	wb.WriteString(`test: hello

`)
	wb.Flush()
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
func check_chunk_trailer_with_path(path string) {
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
	fmt.Println(resp.Status)
	_, err = io.Copy(os.Stdout, resp.Body)
	if err != nil {
		panic(err)
	}
}
func check_chunk_trailer() {
	if config.Cfg.UpstreamHttp2 {
		fmt.Printf("skip test. this test is not support upstream h2...\n")
		return
	}
	common.Getx("/upstream/http/chunk_trailer", "localhost", nil, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "MozillaDeveloperNetwork")
		common.AssertSame(resp.Trailer.Get("x-trailer-test"), "hello")
	})
	common.Getx("/upstream/http/split_chunk_trailer", "localhost", nil, func(resp *http.Response, err error) {
		common.AssertSame(common.Read(resp), "MozillaDeveloperNetwork")
		common.AssertSame(resp.Trailer.Get("x-trailer-test"), "hello")
	})
	//check_chunk_trailer_with_path("/upstream/http/post_chunk_trailer")
}
