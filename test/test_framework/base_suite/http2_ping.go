package base_suite

import (
	"crypto/tls"
	"fmt"
	"io"
	"net"
	"test_framework/common"
	"time"

	"golang.org/x/net/http2"
)

func dialHttp2() (*tls.Conn, *http2.Framer, error) {
	dialer := &net.Dialer{Timeout: 5 * time.Second}
	cn, err := tls.DialWithDialer(dialer, "tcp", "127.0.0.1:9943", &tls.Config{
		InsecureSkipVerify: true,
		NextProtos:         []string{"h2"},
	})
	if err != nil {
		return nil, nil, err
	}
	if cn.ConnectionState().NegotiatedProtocol != "h2" {
		cn.Close()
		return nil, nil, fmt.Errorf("alpn=%s", cn.ConnectionState().NegotiatedProtocol)
	}
	if _, err = io.WriteString(cn, http2.ClientPreface); err != nil {
		cn.Close()
		return nil, nil, err
	}
	fr := http2.NewFramer(cn, cn)
	if err = fr.WriteSettings(); err != nil {
		cn.Close()
		return nil, nil, err
	}
	cn.SetReadDeadline(time.Now().Add(5 * time.Second))
	gotSettings := false
	for !gotSettings {
		f, err := fr.ReadFrame()
		if err != nil {
			cn.Close()
			return nil, nil, err
		}
		if sf, ok := f.(*http2.SettingsFrame); ok && !sf.IsAck() {
			if err = fr.WriteSettingsAck(); err != nil {
				cn.Close()
				return nil, nil, err
			}
			gotSettings = true
		}
	}
	return cn, fr, nil
}

func check_http2_ping_ack() {
	cn, fr, err := dialHttp2()
	common.Assert("http2 ping dial", err == nil)
	if err != nil {
		return
	}
	defer cn.Close()

	payload := [8]byte{0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef}
	common.Assert("write ping", fr.WritePing(false, payload) == nil)

	cn.SetReadDeadline(time.Now().Add(5 * time.Second))
	for {
		f, err := fr.ReadFrame()
		common.Assert("read ping ack", err == nil)
		if err != nil {
			return
		}
		if sf, ok := f.(*http2.SettingsFrame); ok && !sf.IsAck() {
			fr.WriteSettingsAck()
			continue
		}
		pf, ok := f.(*http2.PingFrame)
		if !ok {
			continue
		}
		common.Assert("ping must be ack", pf.IsAck())
		common.AssertByteSame(pf.Data[:], payload[:])
		return
	}
}

func check_http2_ping_flood() {
	cn, fr, err := dialHttp2()
	common.Assert("http2 ping flood dial", err == nil)
	if err != nil {
		return
	}
	defer cn.Close()
	if tcp, ok := cn.NetConn().(*net.TCPConn); ok {
		_ = tcp.SetReadBuffer(1024)
	}

	payload := [8]byte{0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10}
	closed := false
	cn.SetWriteDeadline(time.Now().Add(10 * time.Second))
	for i := 0; i < 20000; i++ {
		if err = fr.WritePing(false, payload); err != nil {
			closed = true
			break
		}
	}
	if !closed {
		cn.SetReadDeadline(time.Now().Add(3 * time.Second))
		_, err = fr.ReadFrame()
		if err == nil {
			closed = false
		} else if ne, ok := err.(net.Error); ok && ne.Timeout() {
			closed = false
		} else {
			closed = true
		}
	}
	common.Assert("ping flood must not grow unbounded", closed)
	common.AssertPort(9943, true)
}
