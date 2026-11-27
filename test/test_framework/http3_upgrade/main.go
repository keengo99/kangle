// Wire-level H3 regressions. Use the Python harness to create the origins.
package main

import (
	"bytes"
	"context"
	"crypto/sha256"
	"crypto/tls"
	"errors"
	"fmt"
	"io"
	"os"
	"strings"
	"sync"
	"time"

	"github.com/quic-go/qpack"
	"github.com/quic-go/quic-go"
	"github.com/quic-go/quic-go/quicvarint"
)

type field = qpack.HeaderField

var total int

func check(ok bool, message string) {
	if !ok {
		panic(message)
	}
	total++
	fmt.Println("PASS:", message)
}
func frame(stream quic.SendStream, typ uint64, payload []byte) {
	data := quicvarint.Append(nil, typ)
	data = quicvarint.Append(data, uint64(len(payload)))
	data = append(data, payload...)
	if _, err := stream.Write(data); err != nil {
		panic(err)
	}
}
func headers(stream quic.SendStream, values []field) {
	var buf bytes.Buffer
	encoder := qpack.NewEncoder(&buf)
	for _, h := range values {
		if err := encoder.WriteField(h); err != nil {
			panic(err)
		}
	}
	frame(stream, 1, buf.Bytes())
}
func dial(addr string) quic.Connection {
	return dialName(addr, "h1.on.test")
}
func dialName(addr, name string) quic.Connection {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	cn, err := quic.DialAddr(ctx, addr, &tls.Config{InsecureSkipVerify: true, ServerName: name, NextProtos: []string{"h3"}}, &quic.Config{MaxIdleTimeout: 15 * time.Second})
	if err != nil {
		panic(err)
	}
	go func() {
		for {
			s, err := cn.AcceptUniStream(context.Background())
			if err != nil {
				return
			}
			go io.Copy(io.Discard, s)
		}
	}()
	control, err := cn.OpenUniStreamSync(ctx)
	if err != nil {
		panic(err)
	}
	control.Write([]byte{0}) // HTTP/3 control stream
	frame(control, 4, nil)   // SETTINGS: dynamic table capacity remains zero
	return cn
}
func base(host, path, method string) []field {
	return []field{{Name: ":method", Value: method}, {Name: ":scheme", Value: "https"}, {Name: ":authority", Value: host}, {Name: ":path", Value: path}}
}
func open(cn quic.Connection, values []field, body []byte, tail []field, finish bool) quic.Stream {
	s, err := cn.OpenStreamSync(context.Background())
	if err != nil {
		panic(err)
	}
	s.SetDeadline(time.Now().Add(10 * time.Second))
	headers(s, values)
	if len(body) > 0 {
		frame(s, 0, body)
	}
	if tail != nil {
		headers(s, tail)
	}
	if finish {
		s.Close()
	}
	return s
}

type reply struct {
	status, hints int
	body          []byte
	fields, tail  []field
	info          [][]field
	err           error
}

func readBlock(s quic.Stream) (uint64, []byte, error) {
	reader := quicvarint.NewReader(s)
	typ, err := quicvarint.Read(reader)
	if err != nil {
		return 0, nil, err
	}
	size, err := quicvarint.Read(reader)
	if err != nil {
		return 0, nil, err
	}
	if size > 1024*1024 {
		return 0, nil, fmt.Errorf("oversized response frame")
	}
	payload := make([]byte, int(size))
	_, err = io.ReadFull(s, payload)
	return typ, payload, err
}
func value(fields []field, name string) string {
	for _, h := range fields {
		if h.Name == name {
			return h.Value
		}
	}
	return ""
}
func receive(s quic.Stream) reply {
	var r reply
	dec := qpack.NewDecoder(nil)
	for {
		typ, payload, err := readBlock(s)
		if err != nil {
			if err != io.EOF {
				r.err = err
			}
			return r
		}
		if typ == 0 {
			r.body = append(r.body, payload...)
		}
		if typ == 1 {
			h, err := dec.DecodeFull(payload)
			if err != nil {
				r.err = err
				return r
			}
			status := value(h, ":status")
			if len(status) == 3 && status[0] == '1' {
				r.hints++
				r.info = append(r.info, h)
				continue
			}
			if status != "" {
				fmt.Sscanf(status, "%d", &r.status)
				r.fields = h
			} else {
				r.tail = h
			}
		}
	}
}
func request(cn quic.Connection, host, path string) reply {
	return receive(open(cn, base(host, path, "GET"), nil, nil, true))
}
func healthy(cn quic.Connection, label string) {
	r := request(cn, "h1.on.test", "/ok")
	if r.err != nil || r.status != 200 || string(r.body) != "ok" {
		panic(fmt.Sprintf("%s: response=%+v, connection=%v", label, r, cn.Context().Err()))
	}
	check(true, label)
}
func rejected(addr string, values []field, body []byte, tail []field, label string) {
	cn := dial(addr)
	defer cn.CloseWithError(0, "")
	r := receive(open(cn, values, body, tail, true))
	check(r.err != nil || r.status >= 400, label)
	healthy(cn, label+": other stream survives")
}
func main() {
	addr := os.Args[1]
	cn := dial(addr)
	check(cn.ConnectionState().TLS.Version == tls.VersionTLS13 && cn.ConnectionState().TLS.NegotiatedProtocol == "h3", "TLS 1.3 / h3 negotiated")
	healthy(cn, "ordinary GET")
	r := receive(open(cn, base("h1.on.test", "/ok", "HEAD"), nil, nil, true))
	check(r.err == nil && r.status == 200 && len(r.body) == 0 && value(r.fields, "content-length") == "2", "HEAD keeps length without sending body")
	for _, form := range []struct {
		body, tail, want string
	}{
		{"log=&pwd=&testcookie=1", "", "22||log=&pwd=&testcookie=1|"},
		{"", "", "0|||"},
		{"abc", "form-tail", "|chunked|abc|form-tail"},
		{strings.Repeat("x", 65536), "", "65536||" + strings.Repeat("x", 65536) + "|"},
		{strings.Repeat("x", 65537), "", "65537||" + strings.Repeat("x", 65537) + "|"},
		{strings.Repeat("x", 1048576), "", "1048576||" + strings.Repeat("x", 1048576) + "|"},
		{strings.Repeat("x", 65537), "form-tail", "|chunked|" + strings.Repeat("x", 65537) + "|form-tail"},
	} {
		h := append(base("h1.on.test", "/form-framing", "POST"),
			field{Name: "content-type", Value: "application/x-www-form-urlencoded"},
			field{Name: "content-length", Value: fmt.Sprint(len(form.body))})
		var tail []field
		if form.tail != "" {
			h = append(h, field{Name: "trailer", Value: "x-upload"})
			tail = []field{{Name: "x-upload", Value: form.tail}}
		}
		r := receive(open(cn, h, []byte(form.body), tail, true))
		check(r.err == nil && r.status == 200 && string(r.body) == form.want,
			fmt.Sprintf("H3 form framing preserves length or declared trailers: size=%d trailer=%v", len(form.body), tail != nil))
	}
	for _, upload := range []struct {
		size     int
		typeName string
	}{
		{22, "application/x-www-form-urlencoded"},
		{1048576, "application/x-www-form-urlencoded"},
		{1048576, "application/json"},
		{1048576, "application/octet-stream"},
	} {
		before := request(cn, "h1.on.test", "/form-stream-prefix")
		var started int
		fmt.Sscanf(string(before.body), "%d", &started)
		h := append(base("h1.on.test", "/form-streaming", "POST"),
			field{Name: "content-type", Value: upload.typeName},
			field{Name: "content-length", Value: fmt.Sprint(upload.size)})
		s := open(cn, h, []byte("x"), nil, false)
		want := fmt.Sprintf("%d|%d|", started+1, upload.size)
		var prefix reply
		for i := 0; i < 100; i++ {
			prefix = request(cn, "h1.on.test", "/form-stream-prefix")
			if prefix.err == nil && string(prefix.body) == want {
				break
			}
			time.Sleep(20 * time.Millisecond)
		}
		check(prefix.err == nil && string(prefix.body) == want,
			fmt.Sprintf("H3 Content-Length first byte reaches origin before upload completes: size=%d type=%s", upload.size, upload.typeName))
		frame(s, 0, []byte(strings.Repeat("x", upload.size-1)))
		s.Close()
		r := receive(s)
		check(r.err == nil && r.status == 200, "H3 streamed upload completes")
	}
	hUndeclared := append(base("h1.on.test", "/form-framing", "POST"), field{Name: "content-length", Value: "3"})
	r = receive(open(cn, hUndeclared, []byte("abc"), []field{{Name: "x-upload", Value: "undeclared"}}, true))
	check(r.err == nil && r.status == 400, "H3 undeclared trailer on Content-Length HTTP/1 upstream gets clear 400")
	healthy(cn, "other H3 stream survives undeclared trailer rejection")
	for _, host := range []string{"h1.on.test", "h2.on.test"} {
		for _, path := range []string{"/trailers", "/empty"} {
			r := request(cn, host, path)
			key, want := "x-meta", "ok"
			if host == "h2.on.test" {
				key, want = "x-final", "yes"
			}
			check(r.err == nil && r.status == 200 && value(r.tail, key) == want, host+path+": response trailer")
		}
		for _, fixed := range []bool{false, true} {
			h := base(host, "/upload", "POST")
			if fixed {
				h = append(h, field{Name: "content-length", Value: "3"})
				if host == "h1.on.test" {
					h = append(h, field{Name: "trailer", Value: "x-upload"})
				}
			}
			r := receive(open(cn, h, []byte("abc"), []field{{Name: "x-upload", Value: "client-tail"}}, true))
			key := "x-meta"
			if host == "h2.on.test" {
				key = "x-final"
			}
			check(r.err == nil && r.status == 200 && string(r.body) == "abc" && value(r.tail, key) == "client-tail", fmt.Sprintf("%s: request/response trailers, fixed=%v", host, fixed))
		}
		h := append(base(host, "/upload", "POST"), field{Name: "content-length", Value: "0"})
		if host == "h1.on.test" {
			h = append(h, field{Name: "trailer", Value: "x-upload"})
		}
		r := receive(open(cn, h, nil, []field{{Name: "x-upload", Value: "zero-tail"}}, true))
		key := "x-meta"
		if host == "h2.on.test" {
			key = "x-final"
		}
		check(r.err == nil && value(r.tail, key) == "zero-tail", host+": zero length upload retains trailer")
	}
	for _, origin := range []string{"hint1", "hint2"} {
		for _, mode := range []string{"on", "off", "h2"} {
			r := request(cn, origin+"."+mode+".test", "/trailers")
			expected := 0
			if mode == "on" {
				expected = 2
			}
			check(r.err == nil && r.status == 200 && string(r.body) == "ok" && r.hints == expected && value(r.tail, "x-final") == "done", origin+" "+mode+": 103 then final response and trailer")
		}
	}
	// Read 100 before sending the upload, so a final-response overwrite fails.
	h := append(base("h1.on.test", "/upload", "POST"), field{Name: "content-length", Value: "3"}, field{Name: "expect", Value: "100-continue"})
	s := open(cn, h, nil, nil, false)
	typ, payload, err := readBlock(s)
	info, _ := qpack.NewDecoder(nil).DecodeFull(payload)
	check(err == nil && typ == 1 && value(info, ":status") == "100", "100 Continue arrives before upload")
	frame(s, 0, []byte("abc"))
	s.Close()
	r = receive(s)
	check(r.err == nil && r.status == 200 && string(r.body) == "abc", "100 does not consume final response state")
	s = open(cn, append(base("hint1.on.test", "/expect", "POST"), field{Name: "content-length", Value: "3"}, field{Name: "expect", Value: "100-continue"}), nil, nil, false)
	for _, status := range []string{"103", "103", "100"} {
		typ, payload, err = readBlock(s)
		info, _ = qpack.NewDecoder(nil).DecodeFull(payload)
		check(err == nil && typ == 1 && value(info, ":status") == status, "103 / 100 precede the request upload: "+status)
		if status == "103" && value(info, "x-early") == "one" {
			links := 0
			for _, f := range info {
				if f.Name == "link" {
					links++
				}
			}
			check(links == 2, "103 retains duplicate Link fields")
		}
	}
	frame(s, 0, []byte("abc"))
	s.Close()
	r = receive(s)
	check(r.err == nil && r.status == 200 && string(r.body) == "abc", "103 and 100 preserve final response and body")
	// Many requests on one connection exercise per-stream isolation.
	var wg sync.WaitGroup
	failures := make(chan string, 24)
	for i := 0; i < 24; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			r := request(cn, "hint2.on.test", "/trailers")
			if r.err != nil || r.hints != 2 || value(r.tail, "x-final") != "done" {
				failures <- fmt.Sprint(r)
			}
		}()
	}
	wg.Wait()
	close(failures)
	for f := range failures {
		panic(f)
	}
	check(true, "24 concurrent streams keep 103 / trailers isolated")
	// Cancellation must reach the HTTP/1 upstream, not just close QUIC.
	before := request(cn, "h1.on.test", "/cancel-count")
	held := request(cn, "h1.on.test", "/hold-count")
	s = open(cn, base("h1.on.test", "/hold", "GET"), nil, nil, true)
	var after reply
	for i := 0; i < 40; i++ {
		time.Sleep(50 * time.Millisecond)
		after = request(cn, "h1.on.test", "/hold-count")
		if !bytes.Equal(held.body, after.body) {
			break
		}
	}
	check(!bytes.Equal(held.body, after.body), "cancel test request reaches upstream")
	s.CancelRead(0x10c)
	for i := 0; i < 40; i++ {
		time.Sleep(50 * time.Millisecond)
		after = request(cn, "h1.on.test", "/cancel-count")
		if !bytes.Equal(before.body, after.body) {
			break
		}
	}
	check(!bytes.Equal(before.body, after.body), "STOP_SENDING cancels HTTP/1 upstream")
	healthy(cn, "unrelated stream after cancellation")
	before = request(cn, "h1.on.test", "/cancel-count")
	held = request(cn, "h1.on.test", "/hold-count")
	s = open(cn, append(base("h1.on.test", "/hold", "POST"), field{Name: "content-length", Value: "3"}), []byte("a"), nil, false)
	for i := 0; i < 40; i++ {
		time.Sleep(50 * time.Millisecond)
		after = request(cn, "h1.on.test", "/hold-count")
		if !bytes.Equal(held.body, after.body) {
			break
		}
	}
	check(!bytes.Equal(held.body, after.body), "reset upload reaches upstream")
	s.CancelWrite(0x10c)
	for i := 0; i < 40; i++ {
		time.Sleep(50 * time.Millisecond)
		after = request(cn, "h1.on.test", "/cancel-count")
		if !bytes.Equal(before.body, after.body) {
			break
		}
	}
	check(!bytes.Equal(before.body, after.body), "RESET_STREAM cancels stalled upload and upstream")
	healthy(cn, "other stream survives RESET_STREAM")
	before = request(cn, "h1.on.test", "/h2-reset-count")
	s = open(cn, append(base("h2.on.test", "/duplex", "POST"), field{Name: "content-type", Value: "application/grpc"}, field{Name: "te", Value: "trailers"}), []byte("first"), nil, false)
	typ, payload, err = readBlock(s)
	info, _ = qpack.NewDecoder(nil).DecodeFull(payload)
	check(err == nil && typ == 1 && value(info, ":status") == "200", "duplex response headers arrive before upload ends")
	typ, payload, err = readBlock(s)
	check(err == nil && typ == 0 && string(payload) == "first", "duplex first message arrives before upload ends")
	frame(s, 0, []byte("second"))
	s.Close()
	r = receive(s)
	check(r.err == nil && string(r.body) == "second" && value(r.tail, "grpc-status") == "0", "duplex continues and delivers gRPC trailers")
	s = open(cn, base("h2.on.test", "/duplex", "POST"), []byte("cancel"), nil, false)
	readBlock(s)
	readBlock(s)
	s.CancelRead(0x10c)
	s.CancelWrite(0x10c)
	for i := 0; i < 40; i++ {
		time.Sleep(50 * time.Millisecond)
		after = request(cn, "h1.on.test", "/h2-reset-count")
		if !bytes.Equal(before.body, after.body) {
			break
		}
	}
	check(!bytes.Equal(before.body, after.body), "cancel duplex stream resets HTTP/2 upstream")
	healthy(cn, "unrelated stream after duplex cancellation")
	// A stalled upload must time out while another stream keeps QUIC alive.
	s = open(cn, append(base("h1.on.test", "/upload", "POST"), field{Name: "content-length", Value: "3"}), []byte("a"), nil, false)
	go func() {
		for i := 0; i < 8; i++ {
			time.Sleep(400 * time.Millisecond)
			request(cn, "h1.on.test", "/ok")
		}
	}()
	started := time.Now()
	r = receive(s)
	var streamError *quic.StreamError
	check(errors.As(r.err, &streamError) && streamError.ErrorCode == 0x10c && time.Since(started) < 9*time.Second, "stalled upload has a server per-stream timeout")
	healthy(cn, "other stream survives upload timeout")
	s = open(cn, append(base("h2.on.test", "/early-response", "POST"), field{Name: "content-length", Value: "999"}, field{Name: "content-type", Value: "application/grpc"}), nil, nil, false)
	r = receive(s)
	check(r.err == nil && r.status == 413 && string(r.body) == "no", "early final response survives stopping request upload")
	old := sha256.Sum256(cn.ConnectionState().TLS.PeerCertificates[0].Raw)
	request(cn, "h1.on.test", "/reload")
	changed := false
	for i := 0; i < 30; i++ {
		time.Sleep(100 * time.Millisecond)
		fresh := dial(addr)
		now := sha256.Sum256(fresh.ConnectionState().TLS.PeerCertificates[0].Raw)
		fresh.CloseWithError(0, "")
		if old != now {
			changed = true
			break
		}
	}
	check(changed, "certificate reload changes new H3 handshakes")
	healthy(cn, "existing H3 connection survives certificate reload")
	fresh := dial(addr)
	old = sha256.Sum256(fresh.ConnectionState().TLS.PeerCertificates[0].Raw)
	fresh.CloseWithError(0, "")
	request(cn, "h1.on.test", "/reload-in-place")
	changed = false
	for i := 0; i < 30; i++ {
		time.Sleep(100 * time.Millisecond)
		fresh := dial(addr)
		now := sha256.Sum256(fresh.ConnectionState().TLS.PeerCertificates[0].Raw)
		fresh.CloseWithError(0, "")
		if old != now {
			changed = true
			break
		}
	}
	check(changed, "replacing certificate files in place updates new H3 handshakes")
	healthy(cn, "existing connection survives replacing certificate files")
	sni := dialName(addr, "h2.on.test")
	old = sha256.Sum256(sni.ConnectionState().TLS.PeerCertificates[0].Raw)
	request(cn, "h1.on.test", "/reload-sni")
	changed = false
	for i := 0; i < 30; i++ {
		time.Sleep(100 * time.Millisecond)
		fresh := dialName(addr, "h2.on.test")
		now := sha256.Sum256(fresh.ConnectionState().TLS.PeerCertificates[0].Raw)
		fresh.CloseWithError(0, "")
		if old != now {
			changed = true
			break
		}
	}
	check(changed, "virtual host SNI certificate updates new H3 handshakes")
	r = request(sni, "h2.on.test", "/empty")
	check(r.err == nil && r.status == 200 && value(r.tail, "grpc-status") == "0", "existing SNI connection survives certificate reload")
	sni.CloseWithError(0, "")
	cn.CloseWithError(0, "")
	good := base("h1.on.test", "/upload", "POST")
	cn = dial(addr)
	r = receive(open(cn, append(append([]field{}, good...), field{Name: "content-length", Value: "3"}, field{Name: "content-length", Value: "3"}), []byte("abc"), nil, true))
	check(r.err == nil && r.status == 200 && string(r.body) == "abc", "equal duplicate Content-Length remains compatible")
	cn.CloseWithError(0, "")
	cases := []struct {
		label string
		h     []field
	}{
		{"missing method", good[1:]},
		{"duplicate pseudo header", append(append([]field{}, good...), field{Name: ":method", Value: "POST"})},
		{"pseudo after ordinary field", append([]field{{Name: "x-a", Value: "b"}}, good...)},
		{"uppercase field", append(append([]field{}, good...), field{Name: "X-Test", Value: "bad"})},
		{"connection header", append(append([]field{}, good...), field{Name: "connection", Value: "close"})},
		{"transfer-encoding", append(append([]field{}, good...), field{Name: "transfer-encoding", Value: "chunked"})},
		{"invalid TE", append(append([]field{}, good...), field{Name: "te", Value: "gzip"})},
		{"authority/host mismatch", append(append([]field{}, good...), field{Name: "host", Value: "other.test"})},
		{"invalid length", append(append([]field{}, good...), field{Name: "content-length", Value: "3junk"})},
		{"ambiguous length", append(append([]field{}, good...), field{Name: "content-length", Value: "3"}, field{Name: "content-length", Value: "4"})},
		{"invalid value controls", append(append([]field{}, good...), field{Name: "x-bad", Value: "a\x00b"})},
		{"unknown pseudo header", append(append([]field{}, good...), field{Name: ":unknown", Value: "bad"})},
		{"unadvertised extended CONNECT", []field{{Name: ":method", Value: "CONNECT"}, {Name: ":scheme", Value: "https"}, {Name: ":authority", Value: "h1.on.test"}, {Name: ":path", Value: "/"}, {Name: ":protocol", Value: "websocket"}}},
	}
	many := append([]field{}, good...)
	for i := 0; i < 1024; i++ {
		many = append(many, field{Name: "x-field", Value: "v"})
	}
	cases = append(cases, struct {
		label string
		h     []field
	}{"too many fields", many})
	for _, tc := range cases {
		rejected(addr, tc.h, nil, nil, tc.label)
	}
	for _, length := range []string{"0", "2", "4"} {
		rejected(addr, append(append([]field{}, good...), field{Name: "content-length", Value: length}), []byte("abc"), nil, "body length mismatch "+length)
	}
	for _, name := range []string{":path", "content-length", "connection", "host", "X-Tail"} {
		rejected(addr, good, []byte("abc"), []field{{Name: name, Value: "bad"}}, "invalid trailer "+name)
	}
	// Ensure malformed traffic has not damaged the worker or UDP listener.
	cn = dial(addr)
	healthy(cn, "fresh request after malformed traffic")
	cn.CloseWithError(0, "")
	fmt.Printf("HTTP/3 checks passed: %d\n", total)
}
