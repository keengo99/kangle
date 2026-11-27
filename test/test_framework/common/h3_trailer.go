package common

import (
	"bytes"
	"context"
	"crypto/tls"
	"io"
	"net/http"
	"strings"
	"time"

	"github.com/quic-go/qpack"
	"github.com/quic-go/quic-go"
	"github.com/quic-go/quic-go/quicvarint"
)

// H3TrailerRequest exercises request trailers with wire frames because the
// pinned quic-go HTTP transport does not implement sending them.
func H3TrailerRequest(req *http.Request) string {
	ctx, cancel := context.WithTimeout(req.Context(), 10*time.Second)
	defer cancel()
	cn, err := quic.DialAddr(ctx, req.URL.Host, &tls.Config{
		InsecureSkipVerify: true, ServerName: req.URL.Hostname(), NextProtos: []string{"h3"},
	}, nil)
	if err != nil {
		panic(err)
	}
	defer cn.CloseWithError(0, "")
	go func() {
		for {
			s, err := cn.AcceptUniStream(ctx)
			if err != nil {
				return
			}
			go io.Copy(io.Discard, s)
		}
	}()
	writeFrame := func(s quic.SendStream, typ uint64, payload []byte) {
		b := quicvarint.Append(nil, typ)
		b = quicvarint.Append(b, uint64(len(payload)))
		if _, err := s.Write(append(b, payload...)); err != nil {
			panic(err)
		}
	}
	control, err := cn.OpenUniStreamSync(ctx)
	if err != nil {
		panic(err)
	}
	if _, err := control.Write([]byte{0}); err != nil {
		panic(err)
	}
	writeFrame(control, 4, nil)
	s, err := cn.OpenStreamSync(ctx)
	if err != nil {
		panic(err)
	}
	s.SetDeadline(time.Now().Add(10 * time.Second))
	writeHeaders := func(fields []qpack.HeaderField) {
		var b bytes.Buffer
		enc := qpack.NewEncoder(&b)
		for _, f := range fields {
			if err := enc.WriteField(f); err != nil {
				panic(err)
			}
		}
		writeFrame(s, 1, b.Bytes())
	}
	var trailerNames []string
	for name := range req.Trailer {
		trailerNames = append(trailerNames, strings.ToLower(name))
	}
	writeHeaders([]qpack.HeaderField{{Name: ":method", Value: req.Method},
		{Name: ":scheme", Value: "https"}, {Name: ":authority", Value: req.URL.Host},
		{Name: ":path", Value: req.URL.RequestURI()}, {Name: "trailer", Value: strings.Join(trailerNames, ", ")}})
	body, err := io.ReadAll(req.Body)
	if err != nil {
		panic(err)
	}
	writeFrame(s, 0, body)
	var tail []qpack.HeaderField
	for name, values := range req.Trailer {
		for _, value := range values {
			tail = append(tail, qpack.HeaderField{Name: strings.ToLower(name), Value: value})
		}
	}
	writeHeaders(tail)
	if err := s.Close(); err != nil {
		panic(err)
	}
	var response bytes.Buffer
	reader := quicvarint.NewReader(s)
	for {
		typ, err := quicvarint.Read(reader)
		if err == io.EOF {
			return response.String()
		}
		if err != nil {
			panic(err)
		}
		size, err := quicvarint.Read(reader)
		if err != nil {
			panic(err)
		}
		if size > 1024*1024 {
			panic("oversized HTTP/3 frame")
		}
		b := make([]byte, int(size))
		if _, err := io.ReadFull(s, b); err != nil {
			panic(err)
		}
		if typ == 0 {
			response.Write(b)
		}
	}
}
