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
	"sync"
	"test_framework/common"
	"test_framework/config"
	"time"

	"golang.org/x/net/http2"
	"golang.org/x/net/http2/h2c"
)

var prepareOnce sync.Once
var requestActivity = struct {
	sync.Mutex
	active     int
	generation uint64
}{}

func requestStarted() {
	requestActivity.Lock()
	requestActivity.active++
	requestActivity.generation++
	requestActivity.Unlock()
}

func requestFinished() {
	requestActivity.Lock()
	requestActivity.active--
	requestActivity.generation++
	requestActivity.Unlock()
}

// WaitIdle waits until no origin request is active and no new request has
// arrived for quietPeriod.  Some cache tests complete the client response
// before kangle finishes its background range fetches.
func WaitIdle(quietPeriod, timeout time.Duration) bool {
	deadline := time.Now().Add(timeout)
	quietSince := time.Now()
	requestActivity.Lock()
	lastGeneration := requestActivity.generation
	requestActivity.Unlock()
	for time.Now().Before(deadline) {
		requestActivity.Lock()
		active := requestActivity.active
		generation := requestActivity.generation
		requestActivity.Unlock()
		if active != 0 || generation != lastGeneration {
			quietSince = time.Now()
			lastGeneration = generation
		}
		if active == 0 && time.Since(quietSince) >= quietPeriod {
			return true
		}
		time.Sleep(10 * time.Millisecond)
	}
	return false
}

func Handle(pattern string, handler func(http.ResponseWriter, *http.Request)) {
	http.HandleFunc(pattern, handler)
	http.HandleFunc("/upstream/http"+pattern, handler)
	http.HandleFunc("/upstream/h2"+pattern, handler)
	http.HandleFunc("/upstream/h2c"+pattern, handler)
	http.HandleFunc("/upstream/ssl"+pattern, handler)
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

	//handlers = make(map[string]func(http.ResponseWriter, *http.Request), 0)

}
func Prepare() {
	prepareOnce.Do(func() {
		common.CreateRange(1024)
		Handle("/range", common.HandleRange)
		Handle("/range/", common.HandleRange)
	})
}

func Start() {
	Prepare()

	handler := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		requestStarted()
		defer requestFinished()
		h, _ := http.DefaultServeMux.Handler(r)
		h.ServeHTTP(w, r)
	})
	h2s := &http2.Server{}
	go func() {
		err := http.ListenAndServeTLS("127.0.0.1:4412", fmt.Sprintf("%s/etc/server.crt", config.Cfg.BasePath), fmt.Sprintf("%s/etc/server.key", config.Cfg.BasePath), handler)
		if err != nil {
			panic(err)
		}
	}()
	go func() {
		err := http.ListenAndServe("0.0.0.0:4411", h2c.NewHandler(handler, h2s))
		if err != nil {
			panic(err)
		}
	}()
	go startStalePoolOrigin()
	go startChunkKeepAliveOrigin()
	go startHostMarkOrigin()
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
}
