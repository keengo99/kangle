package base_suite

import (
	"context"
	"crypto/tls"
	"net"
	"net/http"
	"test_framework/common"
	"time"
)

func check_host_mark_proxy() {
	req, err := http.NewRequest(http.MethodGet, "https://127.0.0.1:9988/host_mark_proxy", nil)
	common.Assert("create host mark proxy request", err == nil)
	req.Host = "proxy-rewrite.test:9988"
	resp, err := common.Http1Client.Do(req)
	common.Assert("host mark proxy request", err == nil && resp.StatusCode == http.StatusOK)
	common.AssertSame(common.Read(resp), "host=127.0.0.2:9988;sni=")
}

func check_host_rewrite_https_vhost() {
	httpReq, err := http.NewRequest(http.MethodGet, "http://127.0.0.1:9999/index.html", nil)
	common.Assert("create host rewrite HTTP request", err == nil)
	httpReq.Host = "rewrite-source.test:9999"
	httpResp, err := common.Http1Client.Do(httpReq)
	common.Assert("host rewrite HTTP request", err == nil && httpResp.StatusCode == http.StatusOK)
	common.AssertSame(common.Read(httpResp), "host-rewrite-target")

	dialer := &net.Dialer{Timeout: 5 * time.Second}
	client := &http.Client{
		Timeout: 10 * time.Second,
		Transport: &http.Transport{
			DialContext: func(ctx context.Context, network, _ string) (net.Conn, error) {
				return dialer.DialContext(ctx, network, "127.0.0.1:9988")
			},
			TLSClientConfig: &tls.Config{
				InsecureSkipVerify: true,
				ServerName:         "rewrite-source.test",
			},
		},
	}
	resp, err := client.Get("https://rewrite-source.test:9988/index.html")
	common.Assert("host rewrite HTTPS request", err == nil && resp.StatusCode == http.StatusOK)
	common.AssertSame(common.Read(resp), "host-rewrite-target")
}
