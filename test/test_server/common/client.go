package common

import (
	"crypto/tls"
	"errors"
	"fmt"
	"net"
	"net/http"
	"strconv"
	"strings"
	"test_server/config"
	"time"

	"github.com/lucas-clemente/quic-go"
	"github.com/lucas-clemente/quic-go/http3"
	"golang.org/x/net/http2"
)

var ErrNoRedirect = errors.New("no redirect")
var Http1Client *http.Client
var Http2Client *http.Client
var Http3Client *http.Client
var HttpAutoClient *http.Client
var H2cClient *http.Client

const (
	HTTP_TIME_OUT = 30
)

func InitClient(keepAlive bool) {

	Http1Client = &http.Client{}
	Http1Client.Timeout = time.Second * HTTP_TIME_OUT
	Http1Client.CheckRedirect = detectorCheckRedirect

	HttpAutoClient = &http.Client{}
	HttpAutoClient.Timeout = time.Second * HTTP_TIME_OUT
	HttpAutoClient.CheckRedirect = detectorCheckRedirect
	h2c_ptr := &http2.Transport{
		AllowHTTP: true, //充许非加密的链接
		DialTLS: func(network, addr string, cfg *tls.Config) (net.Conn, error) {
			return net.Dial(network, addr)
		},
	}
	http2_tr := &http2.Transport{
		AllowHTTP: true, //充许非加密的链接
		TLSClientConfig: &tls.Config{
			InsecureSkipVerify: true,
		},
	}
	http_tr := &http.Transport{
		Dial: (&net.Dialer{
			Timeout:   HTTP_TIME_OUT * time.Second,
			KeepAlive: HTTP_TIME_OUT * time.Second,
		}).Dial,
		TLSHandshakeTimeout:   HTTP_TIME_OUT * time.Second,
		ExpectContinueTimeout: HTTP_TIME_OUT * time.Second,
		TLSClientConfig:       &tls.Config{InsecureSkipVerify: true},
		DisableKeepAlives:     !keepAlive,
	}
	http_auto_tr := &http.Transport{
		Dial: (&net.Dialer{
			Timeout:   HTTP_TIME_OUT * time.Second,
			KeepAlive: HTTP_TIME_OUT * time.Second,
		}).Dial,
		TLSHandshakeTimeout:   HTTP_TIME_OUT * time.Second,
		ExpectContinueTimeout: HTTP_TIME_OUT * time.Second,
		TLSClientConfig:       &tls.Config{InsecureSkipVerify: true},
		DisableKeepAlives:     !keepAlive,
	}
	var qconf quic.Config
	http3_pr := &http3.RoundTripper{
		TLSClientConfig: &tls.Config{
			InsecureSkipVerify: true,
		},
		QuicConfig: &qconf,
	}
	http2.ConfigureTransport(http_auto_tr)

	Http1Client.Transport = http_tr
	Http2Client = &http.Client{Transport: http2_tr}
	Http3Client = &http.Client{Transport: http3_pr}
	H2cClient = &http.Client{Transport: h2c_ptr}
	HttpAutoClient.Transport = http_auto_tr
}
func detectorCheckRedirect(req *http.Request, via []*http.Request) error {
	return ErrNoRedirect
}

type ClientCheckBack func(resp *http.Response, err error)

func skipBody(resp *http.Response, report chan int) (total_read int64) {
	buf := make([]byte, 16384)
	for {
		n, err := resp.Body.Read(buf)
		if n <= 0 {
			fmt.Printf("read length=[%d] err=[%v]\n", n, err)
			return
		}
		total_read += int64(n)
		report <- n
	}
}
func Read(resp *http.Response) string {
	var content string
	buf := make([]byte, 4096)
	for {
		n, _ := resp.Body.Read(buf)
		if n <= 0 {
			return content
		}
		buf = buf[0:n]
		content += string(buf)
	}
}
func Post(path string, header map[string]string, body string, cb ClientCheckBack) {
	url := fmt.Sprintf("%s%s", config.Cfg.UrlPrefix, path)
	request_body := strings.NewReader(body)
	req, err := http.NewRequest("POST", url, request_body)
	if err != nil {
		if cb != nil {
			cb(nil, err)
		}
		return
	}

	for k, v := range header {
		if strings.EqualFold(k, "Content-Length") {
			length, err := strconv.Atoi(v)
			if err != nil {
				req.ContentLength = int64(length)
			}
			continue
		}
		if strings.EqualFold(k, "Transfer-Encoding") {
			req.TransferEncoding = append(req.TransferEncoding, v)
			continue
		}
		req.Header.Set(k, v)
	}
	client := GetClient(url)
	resp, err := client.Do(req)
	if resp != nil {
		defer resp.Body.Close()
	}
	if err != nil {
		fmt.Printf("error get url [%s] error=[%s]\n", url, err.Error())
	}
	//fmt.Printf("age=[%s]\n", resp.Header.Get("Age"))
	if cb != nil {
		cb(resp, err)
	}
	time.Sleep(20 * time.Millisecond)
}
func Request(method string, path string, host string, header map[string]string, cb ClientCheckBack) {
	var url string
	if strings.Contains(path, "://") {
		url = path
	} else if host != "" {
		url = config.GetUrl(host, path)
	} else {
		url = fmt.Sprintf("%s%s", config.Cfg.UrlPrefix, path)
	}
	req, err := http.NewRequest(method, url, nil)
	if err != nil {
		if cb != nil {
			cb(nil, err)
		}
		return
	}

	for k, v := range header {
		req.Header.Add(k, v)
	}

	client := GetClient(url)
	if len(host) > 0 {
		_, port, _ := net.SplitHostPort(req.URL.Host)
		if len(port) > 0 {
			req.URL.Host = fmt.Sprintf("%s:%s", host, port)
		} else {
			req.URL.Host = host
		}
	}
	resp, err := client.Do(req)
	if resp != nil {
		defer resp.Body.Close()
	}
	if err != nil {
		fmt.Printf("error get url [%s] error=[%s]\n", url, err.Error())
	}
	//fmt.Printf("age=[%s]\n", resp.Header.Get("Age"))
	if cb != nil {
		cb(resp, err)
	}
	time.Sleep(20 * time.Millisecond)
}

func Getx(path string, host string, header map[string]string, cb ClientCheckBack) {
	Request("GET", path, host, header, cb)
}
func Options(path string, header map[string]string, cb ClientCheckBack) {
	Request("OPTIONS", path, "", header, cb)
}
func Get(path string, header map[string]string, cb ClientCheckBack) {
	Getx(path, "", header, cb)
}
func Head(path string, header map[string]string, cb ClientCheckBack) {
	Request("HEAD", path, "", header, cb)
}

func Bench(method string, url string, header map[string]string, count int, report chan int) (success_count int) {
	req, err := http.NewRequest(method, url, nil)

	if err != nil {
		return 0
	}
	for k, v := range header {
		req.Header.Add(k, v)
	}

	var client *http.Client
	if strings.HasPrefix(url, "https://") && config.Cfg.Alpn == config.HTTP2 {
		client = Http2Client
	} else if strings.HasPrefix(url, "https://") && config.Cfg.Alpn == config.HTTP3 {
		client = Http3Client
	} else {
		client = Http1Client
	}
	for n := 0; n < count; n++ {
		resp, _ := client.Do(req)
		if resp != nil {
			body_read := skipBody(resp, report)
			if resp.StatusCode >= 200 && resp.StatusCode < 300 {
				if body_read == resp.ContentLength {
					success_count++
				}
			}
			resp.Body.Close()
		}
	}
	return
}
func GetClient(url string) *http.Client {
	if strings.HasPrefix(url, "https://") && config.Cfg.Alpn == config.HTTP2 {
		return Http2Client
	} else if strings.HasPrefix(url, "https://") && config.Cfg.Alpn == config.HTTP3 {
		return Http3Client
	} else if strings.HasPrefix(url, "http://") && config.Cfg.Alpn == config.H2C {
		return H2cClient
	}
	return Http1Client
}
