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

	"golang.org/x/net/http2"
)

var ERROR_NO_REDIRECT = errors.New("no redirect")
var Http1Client *http.Client
var Http2Client *http.Client
var HttpAutoClient *http.Client

const (
	HTTP_TIME_OUT = 5
)

func InitClient() {
	Http1Client = &http.Client{}
	Http1Client.Timeout = time.Second * HTTP_TIME_OUT
	Http1Client.CheckRedirect = detectorCheckRedirect

	HttpAutoClient = &http.Client{}
	HttpAutoClient.Timeout = time.Second * HTTP_TIME_OUT
	HttpAutoClient.CheckRedirect = detectorCheckRedirect

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
	}
	http_auto_tr := &http.Transport{
		Dial: (&net.Dialer{
			Timeout:   HTTP_TIME_OUT * time.Second,
			KeepAlive: HTTP_TIME_OUT * time.Second,
		}).Dial,
		TLSHandshakeTimeout:   HTTP_TIME_OUT * time.Second,
		ExpectContinueTimeout: HTTP_TIME_OUT * time.Second,
		TLSClientConfig:       &tls.Config{InsecureSkipVerify: true},
	}
	http2.ConfigureTransport(http_auto_tr)

	Http1Client.Transport = http_tr
	Http2Client = &http.Client{Transport: http2_tr}
	HttpAutoClient.Transport = http_auto_tr
}
func detectorCheckRedirect(req *http.Request, via []*http.Request) error {
	return ERROR_NO_REDIRECT
}

type ClientCheckBack func(resp *http.Response, err error)

func Read(resp *http.Response) string {
	var content string
	for {
		buf := make([]byte, 4096)
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
	if header != nil {
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
	}
	var client *http.Client
	if strings.HasPrefix(config.Cfg.UrlPrefix, "https://") && config.Cfg.Http2 {
		client = Http2Client
	} else {
		client = Http1Client
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
func Request(method string, path string, host string, header map[string]string, cb ClientCheckBack) {
	url := fmt.Sprintf("%s%s", config.Cfg.UrlPrefix, path)
	req, err := http.NewRequest(method, url, nil)
	if err != nil {
		if cb != nil {
			cb(nil, err)
		}
		return
	}
	if header != nil {
		for k, v := range header {
			req.Header.Add(k, v)
		}
	}
	var client *http.Client
	if strings.HasPrefix(config.Cfg.UrlPrefix, "https://") && config.Cfg.Http2 {
		client = Http2Client
	} else {
		client = Http1Client
	}
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

func Get(path string, header map[string]string, cb ClientCheckBack) {
	Getx(path, "", header, cb)
}
func Head(path string, header map[string]string, cb ClientCheckBack) {
	Request("HEAD", path, "", header, cb)
}
