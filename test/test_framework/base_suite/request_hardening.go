package base_suite

import (
	"bufio"
	"bytes"
	"fmt"
	"io"
	"net"
	"net/http"
	"strings"
	"test_framework/common"
	"time"

	"golang.org/x/net/http2"
	"golang.org/x/net/http2/hpack"
)

type h2field struct {
	name  string
	value string
}

func h2_request_fields(extra ...h2field) []h2field {
	fields := []h2field{{":method", "GET"}, {":scheme", "https"}, {":authority", "localhost"}, {":path", "/kangle.status"}}
	return append(fields, extra...)
}

type h2RawSession struct {
	cn  net.Conn
	fr  *http2.Framer
	hb  bytes.Buffer
	enc *hpack.Encoder
	dec *hpack.Decoder
}

func newH2RawSession() (*h2RawSession, error) {
	cn, fr, err := dialHttp2()
	if err != nil {
		return nil, err
	}
	s := &h2RawSession{cn: cn, fr: fr, dec: hpack.NewDecoder(4096, nil)}
	s.enc = hpack.NewEncoder(&s.hb)
	return s, nil
}

func (s *h2RawSession) encode(fields []h2field) []byte {
	s.hb.Reset()
	for _, f := range fields {
		if err := s.enc.WriteField(hpack.HeaderField{Name: f.name, Value: f.value}); err != nil {
			panic(err)
		}
	}
	return s.hb.Bytes()
}

// Consume the entire response so the next request also verifies HPACK table synchronization.
func (s *h2RawSession) readResult(sid uint32) string {
	s.cn.SetReadDeadline(time.Now().Add(5 * time.Second))
	status := ""
	for {
		f, err := s.fr.ReadFrame()
		if err != nil {
			return "read:" + err.Error()
		}
		switch f := f.(type) {
		case *http2.SettingsFrame:
			if !f.IsAck() {
				s.fr.WriteSettingsAck()
			}
		case *http2.HeadersFrame:
			hs, err := s.dec.DecodeFull(f.HeaderBlockFragment())
			if err != nil {
				return "decode:" + err.Error()
			}
			if f.StreamID == sid {
				for _, h := range hs {
					if h.Name == ":status" {
						status = "status:" + h.Value
					}
				}
				if f.StreamEnded() {
					return status
				}
			}
		case *http2.DataFrame:
			if f.StreamID == sid && f.StreamEnded() {
				return status
			}
		case *http2.RSTStreamFrame:
			if f.StreamID == sid {
				return f.ErrCode.String()
			}
		case *http2.GoAwayFrame:
			return "goaway:" + f.ErrCode.String()
		}
	}
}

func (s *h2RawSession) request(sid uint32, fields []h2field) string {
	if err := s.fr.WriteHeaders(http2.HeadersFrameParam{StreamID: sid, BlockFragment: s.encode(fields), EndStream: true, EndHeaders: true}); err != nil {
		return "write:" + err.Error()
	}
	return s.readResult(sid)
}

func check_http2_invalid_request() {
	s, err := newH2RawSession()
	common.Assert("http2 invalid request dial", err == nil)
	if err != nil {
		return
	}
	defer s.cn.Close()
	bad_method := h2_request_fields()
	bad_method[0].value = "FOOBAR"
	// the stream is released before the request starts, this crashed the worker before.
	common.AssertSame(s.request(1, bad_method), "PROTOCOL_ERROR")
	sid := uint32(3)
	for _, field := range []h2field{
		{"x-a", "a\r\nTransfer-Encoding: chunked"}, {"x-a", "a\n"}, {"x-a", "a\x00b"},
		{"X-Upper", "a"}, {"x-\xff", "a"}, {"x/name", "a"}, {":", "a"},
		{"x-a", " a"}, {"x-a", "a\t"}, {":path", "/b"},
	} {
		common.AssertSame(s.request(sid, h2_request_fields(field)), "PROTOCOL_ERROR")
		sid += 2
	}
	for _, path := range []string{"", "http://other.example/kangle.status", "*bad"} {
		fields := h2_request_fields()
		fields[3].value = path
		common.AssertSame(s.request(sid, fields), "PROTOCOL_ERROR")
		sid += 2
		fields[2], fields[3] = fields[3], fields[2]
		common.AssertSame(s.request(sid, fields), "PROTOCOL_ERROR")
		sid += 2
	}
	common.AssertSame(s.request(sid, h2_request_fields(h2field{"x-after-invalid", "table-is-still-synced"})), "status:200")
	common.AssertPort(9943, true)

	// Malformed HPACK and an abandoned CONTINUATION must release an unstarted sink safely.
	for _, block := range [][]byte{{0xff, 0x7f}, {0x40, 0x01, 'x', 0xff}} {
		probe, err := newH2RawSession()
		common.Assert("h2 incomplete stream dial", err == nil)
		if err != nil {
			continue
		}
		common.Assert("h2 malformed header write", probe.fr.WriteHeaders(http2.HeadersFrameParam{StreamID: 1, BlockFragment: block, EndStream: true, EndHeaders: true}) == nil)
		result := probe.readResult(1)
		common.Assert("h2 malformed HPACK rejected: "+result, strings.HasPrefix(result, "goaway:") ||
			(strings.HasPrefix(result, "read:") && !strings.Contains(result, "timeout")))
		probe.cn.Close()
	}
	probe, err := newH2RawSession()
	common.Assert("h2 abandoned headers dial", err == nil)
	if err == nil {
		probe.fr.WriteHeaders(http2.HeadersFrameParam{StreamID: 1, BlockFragment: []byte{0x82}, EndHeaders: false})
		probe.cn.Close()
	}
	common.AssertPort(9943, true)
}

// send raw bytes and return the response and whether the server closed the connection itself.
func http1_raw_request(str string) (string, bool) {
	cn, err := net.Dial("tcp", "127.0.0.1:9999")
	common.Assert("http1 raw dial", err == nil)
	if err != nil {
		return "", false
	}
	defer cn.Close()
	cn.Write([]byte(str))
	cn.SetReadDeadline(time.Now().Add(3 * time.Second))
	buf, err := io.ReadAll(cn)
	if ne, ok := err.(net.Error); ok && ne.Timeout() {
		return string(buf), false
	}
	return string(buf), true
}

func check_http1_request_smuggling() {
	// 9 hex digits wrapped to 0 in uint32 and the body was treated as the next request.
	// kangle.status answers without reading the body, so only the smuggled GET must not be answered.
	buf, closed := http1_raw_request("POST /kangle.status HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n100000000\r\n\r\nGET /kangle.status HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
	common.Assert("chunk size overflow rejected", closed && strings.Count(buf, "\nOK") <= 1)

	for _, name := range []string{"Transfer-Encoding ", "Transfer-Encoding\t", "Transfer\x00-Encoding", ""} {
		buf, closed = http1_raw_request("GET /kangle.status HTTP/1.1\r\nHost: localhost\r\n" + name + ": chunked\r\nConnection: close\r\n\r\n")
		common.Assert("invalid field name rejected", closed && strings.Count(buf, "\nOK") == 0)
	}

	_, closed = http1_raw_request("GET /kangle.status HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n" + strings.Repeat("A", 65536))
	common.Assert("unterminated trailer line rejected", closed)

	buf, _ = http1_raw_request("GET /kangle.status HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
	common.Assert("normal request after smuggling", strings.Contains(buf, "\nOK"))
}

func check_http2_unread_body() {
	s, err := newH2RawSession()
	common.Assert("h2 unread body dial", err == nil)
	if err != nil {
		return
	}
	defer s.cn.Close()
	fields := h2_request_fields(h2field{"content-length", "100"})
	fields[0].value = "POST"
	common.Assert("h2 unread body headers", s.fr.WriteHeaders(http2.HeadersFrameParam{StreamID: 1, BlockFragment: s.encode(fields), EndHeaders: true}) == nil)
	common.AssertSame(s.readResult(1), "status:200")
	common.AssertSame(s.readResult(1), "NO_ERROR")
	common.AssertSame(s.request(3, h2_request_fields()), "status:200")

	// Deliver a trailer in separate HEADERS/CONTINUATION frames, letting the response
	// finish while the parser still holds the trailer's allocation pool.
	for sid := uint32(5); sid < 45; sid += 4 {
		var pending bytes.Buffer
		writer := http2.NewFramer(&pending, nil)
		writer.WriteHeaders(http2.HeadersFrameParam{StreamID: sid, BlockFragment: s.encode(h2_request_fields()), EndHeaders: true})
		trailer := append([]byte(nil), s.encode([]h2field{{"cookie", "late=value"}, {"x-trailer", "late-trailer-value"}})...)
		writer.WriteHeaders(http2.HeadersFrameParam{StreamID: sid, BlockFragment: trailer[:1], EndStream: true})
		_, err := s.cn.Write(pending.Bytes())
		common.Assert("h2 partial trailer write", err == nil)
		common.AssertSame(s.readResult(sid), "status:200")
		common.Assert("h2 trailer continuation", s.fr.WriteContinuation(sid, true, trailer[1:]) == nil)
		common.AssertSame(s.request(sid+2, h2_request_fields()), "status:200")
	}
}

func handle_hardening_upload(w http.ResponseWriter, r *http.Request) {
	_, err := io.Copy(io.Discard, r.Body)
	if err != nil {
		http.Error(w, "invalid body", http.StatusBadRequest)
		return
	}
	w.Write([]byte("hardening-body-ok"))
}

func handle_hardening_origin(w http.ResponseWriter, r *http.Request) {
	hj := w.(http.Hijacker)
	cn, _, err := hj.Hijack()
	if err != nil {
		return
	}
	defer cn.Close()
	trailer := "X-Test: ok\r\n"
	switch r.URL.Query().Get("kind") {
	case "long":
		trailer = "X-Test: " + strings.Repeat("a", 16385) + "\r\n"
	case "count":
		trailer = strings.Repeat("X-Test: ok\r\n", 65)
	}
	io.WriteString(cn, "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nCache-Control: no-store\r\n\r\n1\r\nx\r\n0\r\n"+trailer+"\r\n")
}

func check_http1_trailer_limits() {
	for _, tc := range []struct {
		name, trailer string
		valid         bool
	}{
		{"normal", "X-Test: ok\r\n", true},
		{"long complete line", "X-Test: " + strings.Repeat("a", 16385) + "\r\n", false},
		{"too many", strings.Repeat("X-Test: ok\r\n", 65), false},
	} {
		buf, closed := http1_raw_request("POST /hardening_upload HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n1\r\nx\r\n0\r\n" + tc.trailer + "\r\n")
		common.Assert("request trailer "+tc.name, closed && strings.Contains(buf, "hardening-body-ok") == tc.valid)
	}
	for _, kind := range []string{"normal", "long", "count"} {
		cn, err := net.Dial("tcp", "127.0.0.1:9999")
		common.Assert("origin trailer dial", err == nil)
		if err != nil {
			continue
		}
		cn.SetDeadline(time.Now().Add(5 * time.Second))
		fmt.Fprintf(cn, "GET /upstream/http/hardening_origin?kind=%s HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n", kind)
		resp, err := http.ReadResponse(bufio.NewReader(cn), &http.Request{Method: "GET"})
		common.Assert("origin trailer response", err == nil)
		if err != nil {
			cn.Close()
			continue
		}
		// Parse framing and trailers, not only the final bytes (valid trailers follow the zero chunk).
		body, bodyErr := io.ReadAll(resp.Body)
		resp.Body.Close()
		cn.Close()
		common.Assert("origin trailer "+kind, (bodyErr == nil) == (kind == "normal"))
		if kind == "normal" {
			common.AssertSame(string(body), "x")
			common.AssertSame(resp.Trailer.Get("X-Test"), "ok")
		}
	}
}
