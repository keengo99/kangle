package server

import (
	"bufio"
	"crypto/rand"
	"crypto/tls"
	"fmt"
	"io"
	"log"
	"net"
	"net/http"
	"test_server/config"
	"time"
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
func WriteAll(conn io.Writer, buf []byte) error {

	length := len(buf)
	hot := buf
	for {
		if length <= 0 {
			return nil
		}
		n, err := conn.Write(hot)
		if err != nil {
			return err
		}
		length -= n
		hot = hot[n:]
	}
}
func HandleTlsClientConnect(conn net.Conn) {
	defer conn.Close()
	tls_conn, ok := conn.(*tls.Conn)
	if !ok {
		panic("error")
	}
	r := bufio.NewReader(conn)
	for {
		msg, err := r.ReadString('\n')
		if err != nil {
			fmt.Printf("cann't read error. %v", err.Error())
			return
		}
		if len(msg) <= 2 {
			break
		}
		//fmt.Printf("msg=[%s] size=[%d]\n", msg, len(msg))
	}
	state := tls_conn.ConnectionState()

	WriteAll(conn, []byte("HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n"))
	buf := fmt.Sprint("resum=[", state.DidResume, "],version=[", state.Version, "],cipher=[", state.CipherSuite, "],protocol=[", state.NegotiatedProtocol, "],sni=[", state.ServerName, "]")
	WriteAll(conn, []byte(buf))

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
	go func() {
		crt, err := tls.LoadX509KeyPair(fmt.Sprintf("%s/etc/server.crt", config.Cfg.BasePath), fmt.Sprintf("%s/etc/server.key", config.Cfg.BasePath))
		if err != nil {
			log.Fatalln(err.Error())
		}
		tlsConfig := &tls.Config{}
		tlsConfig.Certificates = []tls.Certificate{crt}
		tlsConfig.Time = time.Now
		tlsConfig.Rand = rand.Reader
		l, err := tls.Listen("tcp", "127.0.0.1:4413", tlsConfig)
		if err != nil {
			log.Fatalln(err.Error())
		}
		for {
			conn, err := l.Accept()
			if err != nil {
				fmt.Println(err.Error())
				continue
			} else {
				go HandleTlsClientConnect(conn)
			}
		}
	}()
}
