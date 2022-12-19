package config

import "fmt"

const (
	HTTP1 = 0
	HTTP2 = 1
	HTTP3 = 2
)

type Config struct {
	UrlPrefix     string
	Alpn          int
	UpstreamSsl   bool
	UpstreamHttp2 bool
	Force         bool
	BasePath      string
	Kangle        string
}

var ports = []int{9999, 9943, 9943}
var schemas = []string{"http://", "https://", "https://"}

var HttpsUrlPrefix string
var HttpUrlPrefix string
var Http3UrlPrefix string
var Cfg Config

var saved_config Config

func Push() {
	saved_config = Cfg
}
func Pop() {
	Cfg = saved_config
}
func UseHttp2Client() {
	Cfg.UrlPrefix = HttpsUrlPrefix
	Cfg.Alpn = HTTP2
}
func UseHttp3Client() {
	Cfg.UrlPrefix = Http3UrlPrefix
	Cfg.Alpn = HTTP3
}
func UseHttpClient(alpn int) {
	Cfg.Alpn = alpn
	switch alpn {
	case HTTP2:
		Cfg.UrlPrefix = HttpsUrlPrefix
	case HTTP3:
		Cfg.UrlPrefix = Http3UrlPrefix
	default:
		Cfg.Alpn = HTTP1
		Cfg.UrlPrefix = HttpUrlPrefix
	}
}
func GetUrl(host string, path string) string {
	return fmt.Sprintf("%s%s:%d%s", schemas[Cfg.Alpn], host, ports[Cfg.Alpn], path)
}
func init() {
	HttpUrlPrefix = "http://127.0.0.1:9999"
	HttpsUrlPrefix = "https://127.0.0.1:9943"
	Http3UrlPrefix = "https://localhost:9943"
}
