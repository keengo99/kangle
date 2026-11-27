package server

import (
	"fmt"
	"io"
	"net/http"
	"test_framework/config"
)

func startHostMarkOrigin() {
	err := http.ListenAndServeTLS("127.0.0.2:9988",
		fmt.Sprintf("%s/etc/server.crt", config.Cfg.BasePath),
		fmt.Sprintf("%s/etc/server.key", config.Cfg.BasePath),
		http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			w.Header().Set("Cache-Control", "no-store")
			_, _ = io.WriteString(w, fmt.Sprintf("host=%s;sni=%s", r.Host, r.TLS.ServerName))
		}))
	if err != nil {
		panic(err)
	}
}
