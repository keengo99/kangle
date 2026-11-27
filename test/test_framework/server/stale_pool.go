package server

import (
	"bufio"
	"fmt"
	"net"
)

func readRequestHeader(r *bufio.Reader) error {
	for {
		line, err := r.ReadString('\n')
		if err != nil {
			return err
		}
		if line == "\n" || line == "\r\n" {
			return nil
		}
	}
}

func handleStalePoolConnection(conn net.Conn, failAfterFirstResponse bool) {
	defer conn.Close()
	r := bufio.NewReader(conn)
	if err := readRequestHeader(r); err != nil {
		return
	}
	if failAfterFirstResponse {
		if err := WriteAll(conn, []byte("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nCache-Control: no-store\r\nConnection: keep-alive\r\n\r\nok")); err != nil {
			return
		}
		// Consume the next request successfully, then close without a response.
		// This exercises the read-header retry path rather than the existing
		// send-header retry path.
		_ = readRequestHeader(r)
		return
	}
	_ = WriteAll(conn, []byte("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nCache-Control: no-store\r\nConnection: close\r\n\r\nok"))
}

func startStalePoolOrigin() {
	listener, err := net.Listen("tcp", "127.0.0.1:4414")
	if err != nil {
		panic(err)
	}
	connectionNumber := 0
	for {
		conn, err := listener.Accept()
		if err != nil {
			fmt.Printf("stale-pool origin accept error: %v\n", err)
			continue
		}
		connectionNumber++
		go handleStalePoolConnection(conn, connectionNumber%2 == 1)
	}
}
