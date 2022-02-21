package config

type Config struct {
	UrlPrefix     string
	Http2         bool
	UpstreamSsl   bool
	UpstreamHttp2 bool
	Force         bool
	BasePath      string
}

var HttpsUrlPrefix string
var HttpUrlPrefix string
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
	Cfg.Http2 = true
}
func init() {
	HttpUrlPrefix = "http://127.0.0.1:9999"
	HttpsUrlPrefix = "https://127.0.0.1:9943"
}
