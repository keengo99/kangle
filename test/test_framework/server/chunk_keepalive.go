package server

import (
	"bufio"
	"fmt"
	"net"
	"strings"
	"sync/atomic"
)

func handleChunkKeepAliveConnection(conn net.Conn, connectionNumber int64) {
	defer conn.Close()
	r := bufio.NewReader(conn)
	for {
		requestLine, err := r.ReadString('\n')
		if err != nil {
			return
		}
		if err := readRequestHeader(r); err != nil {
			return
		}
		if strings.Contains(requestLine, "?http10") {
			header := fmt.Sprintf("HTTP/1.0 200 OK\r\nContent-Length: 5\r\nCache-Control: no-store\r\nX-Upstream-Conn: %d\r\n", connectionNumber)
			if strings.Contains(requestLine, "?http10ka") {
				header += "Connection: keep-alive\r\n"
			}
			if err := WriteAll(conn, []byte(header+"\r\nhello")); err != nil {
				return
			}
			continue
		}
		header := fmt.Sprintf("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nCache-Control: no-store\r\nX-Upstream-Conn: %d\r\n", connectionNumber)
		if strings.Contains(requestLine, "?cl0") {
			// A zero Content-Length must not suppress a chunked body either.
			header += "Content-Length: 0\r\n"
		} else if strings.Contains(requestLine, "?cl") {
			// Transfer-Encoding must override the conflicting Content-Length.
			header += "Content-Length: 3\r\n"
		}
		if err := WriteAll(conn, []byte(header+"\r\n5\r\nhello\r\n0\r\n\r\n")); err != nil {
			return
		}
	}
}

func startChunkKeepAliveOrigin() {
	listener, err := net.Listen("tcp", "127.0.0.1:4415")
	if err != nil {
		panic(err)
	}
	var connectionNumber int64
	for {
		conn, err := listener.Accept()
		if err != nil {
			fmt.Printf("chunk-keepalive origin accept error: %v\n", err)
			continue
		}
		go handleChunkKeepAliveConnection(conn, atomic.AddInt64(&connectionNumber, 1))
	}
}
