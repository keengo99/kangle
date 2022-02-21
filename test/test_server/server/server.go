package server

import (
	"fmt"
	"net/http"
	"test_server/config"
)

var handlers map[string]func(http.ResponseWriter, *http.Request)

func Handle(pattern string, handler func(http.ResponseWriter, *http.Request)) {
	_, ok := handlers[pattern]
	if ok {
		return
	}
	handlers[pattern] = handler
	http.HandleFunc(pattern, handler)
}
func init() {
	handlers = make(map[string]func(http.ResponseWriter, *http.Request), 0)
}
func Start() {
	go func() {
		err := http.ListenAndServeTLS("127.0.0.1:4412", fmt.Sprintf("%s/etc/server.crt", config.Cfg.BasePath), fmt.Sprintf("%s/etc/server.key", config.Cfg.BasePath), nil)
		if err != nil {
			panic(err)
		}
	}()
	go func() {
		err := http.ListenAndServe("127.0.0.1:4411", nil)
		if err != nil {
			panic(err)
		}
	}()
}
