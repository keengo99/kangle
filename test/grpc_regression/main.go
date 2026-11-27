// Real grpc-go client/server regression, with isolated kangle configuration.
// Run: go run . /absolute/path/to/build/kangle
package main

import (
	"bytes"
	"context"
	"crypto/rand"
	"crypto/rsa"
	"crypto/tls"
	"crypto/x509"
	"crypto/x509/pkix"
	"encoding/pem"
	"fmt"
	"io"
	"math/big"
	"net"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"sync"
	"time"

	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/credentials"
	"google.golang.org/grpc/credentials/insecure"
	_ "google.golang.org/grpc/encoding/gzip"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/types/known/wrapperspb"
)

type service interface {
	Unary(context.Context, *wrapperspb.BytesValue) (*wrapperspb.BytesValue, error)
}
type echo struct{}

func (echo) Unary(ctx context.Context, msg *wrapperspb.BytesValue) (*wrapperspb.BytesValue, error) {
	if string(msg.Value) == "denied" {
		grpc.SetTrailer(ctx, metadata.Pairs("binary-bin", string([]byte{0, 1, 255})))
		return nil, status.Error(codes.PermissionDenied, "权限 denied %")
	}
	md, _ := metadata.FromIncomingContext(ctx)
	if len(md.Get("x-test")) != 1 || md.Get("x-test")[0] != "metadata-ok" {
		return nil, status.Error(codes.InvalidArgument, "metadata lost")
	}
	grpc.SetTrailer(ctx, metadata.Pairs("x-final", "ok", "binary-bin", string([]byte{0, 1, 255})))
	return msg, nil
}
func unary(srv interface{}, ctx context.Context, dec func(interface{}) error, interceptor grpc.UnaryServerInterceptor) (interface{}, error) {
	msg := new(wrapperspb.BytesValue)
	if err := dec(msg); err != nil {
		return nil, err
	}
	if interceptor == nil {
		return srv.(service).Unary(ctx, msg)
	}
	return interceptor(ctx, msg, &grpc.UnaryServerInfo{Server: srv, FullMethod: "/test.Echo/Unary"}, func(ctx context.Context, msg interface{}) (interface{}, error) {
		return srv.(service).Unary(ctx, msg.(*wrapperspb.BytesValue))
	})
}
func stream(_ interface{}, st grpc.ServerStream) error {
	st.SetTrailer(metadata.Pairs("x-final", "stream-ok"))
	method, _ := grpc.MethodFromServerStream(st)
	if method == "/test.Echo/Early" {
		return status.Error(codes.PermissionDenied, "early rejection")
	}
	if method == "/test.Echo/Server" {
		msg := new(wrapperspb.BytesValue)
		if err := st.RecvMsg(msg); err != nil {
			return err
		}
		for i := 0; i < 20; i++ {
			if err := st.SendMsg(msg); err != nil {
				return err
			}
		}
		return nil
	}
	total := 0
	for {
		msg := new(wrapperspb.BytesValue)
		err := st.RecvMsg(msg)
		if err == io.EOF {
			if method == "/test.Echo/Client" {
				return st.SendMsg(&wrapperspb.BytesValue{Value: []byte(strconv.Itoa(total))})
			}
			return nil
		}
		if err != nil {
			return err
		}
		total += len(msg.Value)
		if method == "/test.Echo/Bidi" {
			if err := st.SendMsg(msg); err != nil {
				return err
			}
		}
	}
}

var description = grpc.ServiceDesc{ServiceName: "test.Echo", HandlerType: (*service)(nil), Methods: []grpc.MethodDesc{{MethodName: "Unary", Handler: unary}}, Streams: []grpc.StreamDesc{
	{StreamName: "Bidi", Handler: stream, ClientStreams: true, ServerStreams: true},
	{StreamName: "Client", Handler: stream, ClientStreams: true},
	{StreamName: "Server", Handler: stream, ServerStreams: true},
	{StreamName: "Early", Handler: stream, ClientStreams: true, ServerStreams: true},
}}

func must(err error) {
	if err != nil {
		panic(err)
	}
}
func freePort() int {
	l, err := net.Listen("tcp", "127.0.0.1:0")
	must(err)
	p := l.Addr().(*net.TCPAddr).Port
	l.Close()
	return p
}
func copyFile(src, dst string) {
	in, err := os.Open(src)
	must(err)
	defer in.Close()
	out, err := os.OpenFile(dst, os.O_CREATE|os.O_WRONLY|os.O_TRUNC, 0755)
	must(err)
	defer out.Close()
	_, err = io.Copy(out, in)
	must(err)
}
func main() {
	if len(os.Args) != 2 {
		panic("usage: grpc_regression /path/to/build/kangle")
	}
	root, err := os.MkdirTemp("", "kangle-grpc-")
	must(err)
	fmt.Println("artifacts:", root)
	for _, d := range []string{"bin", "etc", "ext", "var", "tmp", "log", "www"} {
		must(os.Mkdir(filepath.Join(root, d), 0755))
	}
	copyFile(os.Args[1], root+"/bin/kangle")
	for _, name := range []string{"libstdc++.so.6", "libgcc_s.so.1"} {
		source := filepath.Join(filepath.Dir(os.Args[1]), name)
		if _, err := os.Stat(source); err == nil {
			copyFile(source, filepath.Join(root, "bin", name))
		}
	}
	key, err := rsa.GenerateKey(rand.Reader, 2048)
	must(err)
	cert := &x509.Certificate{SerialNumber: big.NewInt(1), Subject: pkix.Name{CommonName: "localhost"}, DNSNames: []string{"localhost", "tls.test"}, NotBefore: time.Now().Add(-time.Hour), NotAfter: time.Now().Add(time.Hour), KeyUsage: x509.KeyUsageDigitalSignature | x509.KeyUsageKeyEncipherment, ExtKeyUsage: []x509.ExtKeyUsage{x509.ExtKeyUsageServerAuth}}
	der, err := x509.CreateCertificate(rand.Reader, cert, cert, &key.PublicKey, key)
	must(err)
	certPEM := pem.EncodeToMemory(&pem.Block{Type: "CERTIFICATE", Bytes: der})
	keyPEM := pem.EncodeToMemory(&pem.Block{Type: "RSA PRIVATE KEY", Bytes: x509.MarshalPKCS1PrivateKey(key)})
	must(os.WriteFile(root+"/etc/cert.pem", certPEM, 0600))
	must(os.WriteFile(root+"/etc/key.pem", keyPEM, 0600))
	pair, err := tls.X509KeyPair(certPEM, keyPEM)
	must(err)
	plain, err := net.Listen("tcp", "127.0.0.1:0")
	must(err)
	secure, err := net.Listen("tcp", "127.0.0.1:0")
	must(err)
	servers := []*grpc.Server{grpc.NewServer(), grpc.NewServer(grpc.Creds(credentials.NewTLS(&tls.Config{Certificates: []tls.Certificate{pair}, MinVersion: tls.VersionTLS12})))}
	for i, s := range servers {
		s.RegisterService(&description, echo{})
		l := plain
		if i == 1 {
			l = secure
		}
		go s.Serve(l)
		defer s.Stop()
	}
	front, tlsfront := freePort(), freePort()
	cfg := fmt.Sprintf(`<config><worker_thread>2</worker_thread><timeout rw='10' connect='2'/>
<listen ip='127.0.0.1' port='%dh' type='http'/>
<listen ip='127.0.0.1' port='%d' type='https' certificate='etc/cert.pem' certificate_key='etc/key.pem' http2='1'/>
<cache default='0' memory='16M' disk='0'/><request action='allow'/><response action='allow'/>
<server name='plain' host='127.0.0.1' port='%dh' proto='http' life_time='30'/>
<server name='tls' host='127.0.0.1' port='%dsp' proto='http' life_time='30'/>
<vh name='plain' doc_root='www' inherit='on'><host>localhost</host><map path='/' extend='server:plain' confirm_file='0' allow_method='*'/></vh>
<vh name='tls' doc_root='www' inherit='on'><host>tls.test</host><map path='/' extend='server:tls' confirm_file='0' allow_method='*'/></vh></config>`, front, tlsfront, plain.Addr().(*net.TCPAddr).Port, secure.Addr().(*net.TCPAddr).Port)
	must(os.WriteFile(root+"/etc/config.xml", []byte(cfg), 0600))
	log, err := os.Create(root + "/console.log")
	must(err)
	defer log.Close()
	cmd := exec.Command(root+"/bin/kangle", "-n", "-g")
	cmd.Stdout = log
	cmd.Stderr = log
	must(cmd.Start())
	defer func() {
		cmd.Process.Signal(os.Interrupt)
		done := make(chan struct{})
		go func() { cmd.Wait(); close(done) }()
		select {
		case <-done:
		case <-time.After(5 * time.Second):
			cmd.Process.Kill()
			<-done
		}
	}()
	deadline := time.Now().Add(5 * time.Second)
	for {
		c, err := net.DialTimeout("tcp", fmt.Sprintf("127.0.0.1:%d", front), time.Millisecond*50)
		if err == nil {
			c.Close()
			break
		}
		if time.Now().After(deadline) {
			panic("kangle startup failed")
		}
		time.Sleep(time.Millisecond * 30)
	}
	count := 0
	check := func(ok bool, label string) {
		if !ok {
			panic(label)
		}
		count++
		fmt.Println("PASS:", label)
	}
	dial := func(p int, authority string, secure bool) *grpc.ClientConn {
		options := []grpc.DialOption{grpc.WithAuthority(authority)}
		if secure {
			options = append(options, grpc.WithTransportCredentials(credentials.NewTLS(&tls.Config{InsecureSkipVerify: true, MinVersion: tls.VersionTLS12})))
		} else {
			options = append(options, grpc.WithTransportCredentials(insecure.NewCredentials()))
		}
		cn, err := grpc.NewClient(fmt.Sprintf("127.0.0.1:%d", p), options...)
		must(err)
		return cn
	}
	cn := dial(front, "localhost", false)
	defer cn.Close()
	contextFor := func() (context.Context, context.CancelFunc) {
		ctx, cancel := context.WithTimeout(context.Background(), 8*time.Second)
		return metadata.NewOutgoingContext(ctx, metadata.Pairs("x-test", "metadata-ok")), cancel
	}
	call := func(cn *grpc.ClientConn, value []byte) ([]byte, metadata.MD, error) {
		ctx, cancel := contextFor()
		defer cancel()
		msg := new(wrapperspb.BytesValue)
		var trailer metadata.MD
		err := cn.Invoke(ctx, "/test.Echo/Unary", &wrapperspb.BytesValue{Value: value}, msg, grpc.Trailer(&trailer))
		return msg.Value, trailer, err
	}
	for _, test := range []struct {
		port      int
		authority string
		secure    bool
		label     string
	}{{front, "localhost", false, "h2c frontend/upstream"}, {tlsfront, "localhost", true, "TLS frontend"}, {front, "tls.test", false, "TLS upstream ALPN"}} {
		c := dial(test.port, test.authority, test.secure)
		value, trailer, err := call(c, []byte("echo"))
		must(err)
		check(string(value) == "echo" && trailer.Get("x-final")[0] == "ok" && bytes.Equal([]byte(trailer.Get("binary-bin")[0]), []byte{0, 1, 255}), "unary metadata and binary trailers: "+test.label)
		c.Close()
	}
	compressedCtx, compressedCancel := contextFor()
	compressedPayload := bytes.Repeat([]byte("compressed-message"), 8192)
	compressedReply := new(wrapperspb.BytesValue)
	must(cn.Invoke(compressedCtx, "/test.Echo/Unary", &wrapperspb.BytesValue{Value: compressedPayload}, compressedReply, grpc.UseCompressor("gzip")))
	check(bytes.Equal(compressedReply.Value, compressedPayload), "gRPC gzip message compression")
	compressedCancel()
	_, deniedTrailer, err := call(cn, []byte("denied"))
	check(status.Code(err) == codes.PermissionDenied && status.Convert(err).Message() == "权限 denied %" && len(deniedTrailer.Get("binary-bin")) == 1 && bytes.Equal([]byte(deniedTrailer.Get("binary-bin")[0]), []byte{0, 1, 255}), fmt.Sprintf("trailers-only gRPC error, escaped message and binary metadata: %v", err))
	earlyCtx, earlyCancel := contextFor()
	early, err := cn.NewStream(earlyCtx, &description.Streams[3], "/test.Echo/Early")
	must(err)
	err = early.RecvMsg(new(wrapperspb.BytesValue))
	check(status.Code(err) == codes.PermissionDenied && status.Convert(err).Message() == "early rejection", "server rejection before request half-close")
	earlyCancel()
	payload := bytes.Repeat([]byte("x"), 32768)
	ctx, cancel := contextFor()
	st, err := cn.NewStream(ctx, &description.Streams[0], "/test.Echo/Bidi")
	must(err)
	for i := 0; i < 20; i++ {
		must(st.SendMsg(&wrapperspb.BytesValue{Value: payload}))
		reply := new(wrapperspb.BytesValue)
		must(st.RecvMsg(reply))
		check(bytes.Equal(reply.Value, payload), fmt.Sprintf("bidirectional message %d received before CloseSend", i))
	}
	must(st.CloseSend())
	err = st.RecvMsg(new(wrapperspb.BytesValue))
	check(err == io.EOF && st.Trailer().Get("x-final")[0] == "stream-ok", "bidirectional EOF and trailers")
	cancel()
	ctx, cancel = contextFor()
	st, err = cn.NewStream(ctx, &description.Streams[1], "/test.Echo/Client")
	must(err)
	for i := 0; i < 20; i++ {
		must(st.SendMsg(&wrapperspb.BytesValue{Value: payload}))
	}
	must(st.CloseSend())
	reply := new(wrapperspb.BytesValue)
	must(st.RecvMsg(reply))
	err = st.RecvMsg(new(wrapperspb.BytesValue))
	check(string(reply.Value) == strconv.Itoa(20*len(payload)) && err == io.EOF, "client streaming and half close")
	cancel()
	ctx, cancel = contextFor()
	st, err = cn.NewStream(ctx, &description.Streams[2], "/test.Echo/Server")
	must(err)
	must(st.SendMsg(&wrapperspb.BytesValue{Value: payload}))
	must(st.CloseSend())
	n := 0
	for {
		reply := new(wrapperspb.BytesValue)
		err := st.RecvMsg(reply)
		if err == io.EOF {
			break
		}
		must(err)
		if !bytes.Equal(reply.Value, payload) {
			panic("server stream payload mismatch")
		}
		n++
	}
	check(n == 20 && st.Trailer().Get("x-final")[0] == "stream-ok", "server streaming across flow-control windows")
	cancel()
	ctx, cancel = contextFor()
	st, err = cn.NewStream(ctx, &description.Streams[0], "/test.Echo/Bidi")
	must(err)
	must(st.SendMsg(&wrapperspb.BytesValue{Value: []byte("cancel")}))
	must(st.RecvMsg(new(wrapperspb.BytesValue)))
	cancel()
	err = st.RecvMsg(new(wrapperspb.BytesValue))
	check(status.Code(err) == codes.Canceled, "cancellation")
	ctx, cancel = context.WithTimeout(context.Background(), 100*time.Millisecond)
	st, err = cn.NewStream(ctx, &description.Streams[0], "/test.Echo/Bidi")
	must(err)
	err = st.RecvMsg(new(wrapperspb.BytesValue))
	check(status.Code(err) == codes.DeadlineExceeded, "deadline")
	cancel()
	var wg sync.WaitGroup
	errors := make(chan error, 24)
	for i := 0; i < 24; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			value, _, err := call(cn, []byte("parallel"))
			if err != nil {
				errors <- err
			} else if string(value) != "parallel" {
				errors <- fmt.Errorf("payload mismatch")
			}
		}()
	}
	wg.Wait()
	close(errors)
	for err := range errors {
		must(err)
	}
	check(true, "24 concurrent unary RPCs after cancel/deadline reuse connection")
	fmt.Printf("%d grpc-go integration checks passed; artifacts: %s\n", count, root)
}
