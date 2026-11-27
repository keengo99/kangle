// HTTP/2 and HTTP/3 load probe. Run through test/http3_stability.py.
package main

import (
	"context"
	"crypto/sha256"
	"crypto/tls"
	"encoding/hex"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io"
	"net"
	"net/http"
	"net/url"
	"os"
	"sort"
	"sync"
	"sync/atomic"
	"time"

	"github.com/quic-go/quic-go"
	"github.com/quic-go/quic-go/http3"
	"golang.org/x/net/http2"
)

type result struct {
	Protocol     string   `json:"protocol"`
	Mode         string   `json:"mode"`
	Concurrency  int      `json:"concurrency"`
	Connections  int      `json:"connections"`
	Seconds      float64  `json:"seconds"`
	Success      int64    `json:"success"`
	Errors       int64    `json:"errors"`
	Canceled     int64    `json:"canceled"`
	TimedOut     int64    `json:"timed_out"`
	Bytes        int64    `json:"bytes"`
	RPS          float64  `json:"rps"`
	MiBPS        float64  `json:"mibps"`
	P50MS        float64  `json:"p50_ms"`
	P95MS        float64  `json:"p95_ms"`
	P99MS        float64  `json:"p99_ms"`
	ErrorSamples []string `json:"error_samples,omitempty"`
}

func main() {
	target := flag.String("url", "", "target URL")
	protocol := flag.String("protocol", "h3", "h3 or h2")
	mode := flag.String("mode", "bench", "bench, mixed, churn, blocked, or timeout")
	seconds := flag.Int("seconds", 10, "duration")
	concurrency := flag.Int("concurrency", 16, "workers")
	connections := flag.Int("connections", 1, "independent transports")
	size := flag.Int64("size", 1024, "expected response size")
	wantHash := flag.String("sha256", "", "expected body digest")
	flag.Parse()
	if *target == "" || *concurrency < 1 || *connections < 1 || *seconds < 1 {
		panic("invalid load parameters")
	}
	address, err := url.Parse(*target)
	if err != nil || address.Scheme != "https" || (*protocol != "h3" && *protocol != "h2") {
		panic("invalid target or protocol")
	}
	switch *mode {
	case "bench", "mixed", "churn", "blocked", "timeout":
	default:
		panic("invalid load mode")
	}
	hostPort := address.Host
	if address.Port() == "" {
		hostPort = net.JoinHostPort(address.Hostname(), "443")
	}
	newClient := func() (*http.Client, func()) {
		tlsConfig := &tls.Config{InsecureSkipVerify: true, ServerName: "localhost"}
		if *protocol == "h3" {
			config := &quic.Config{MaxIdleTimeout: 20 * time.Second, InitialStreamReceiveWindow: 1 << 20,
				MaxStreamReceiveWindow: 16 << 20, InitialConnectionReceiveWindow: 16 << 20, MaxConnectionReceiveWindow: 64 << 20}
			if *mode == "blocked" || *mode == "timeout" {
				config.InitialStreamReceiveWindow, config.MaxStreamReceiveWindow = 64<<10, 64<<10
				config.InitialConnectionReceiveWindow, config.MaxConnectionReceiveWindow = 128<<10, 128<<10
			}
			tr := &http3.Transport{TLSClientConfig: tlsConfig, QUICConfig: config}
			return &http.Client{Transport: tr, Timeout: 15 * time.Second}, func() { tr.Close() }
		}
		// Use one explicit H2 connection per client. The pinned x/net Transport's
		// strict mode can deadlock on stream reservations above the peer limit;
		// ClientConn.RoundTrip waits on actual active streams without reservations.
		tlsConfig.NextProtos = []string{"h2"}
		conn, err := tls.DialWithDialer(&net.Dialer{Timeout: 5 * time.Second}, "tcp", hostPort, tlsConfig)
		if err != nil {
			panic(err)
		}
		tr := &http2.Transport{StrictMaxConcurrentStreams: true}
		cc, err := tr.NewClientConn(conn)
		if err != nil {
			conn.Close()
			panic(err)
		}
		return &http.Client{Transport: cc, Timeout: 15 * time.Second}, func() { cc.Close() }
	}
	clients := make([]*http.Client, *connections)
	closes := make([]func(), *connections)
	for i := range clients {
		clients[i], closes[i] = newClient()
		defer closes[i]()
	}
	// Establish TLS before the measured phase; churn intentionally includes it.
	if *mode != "churn" {
		for _, c := range clients {
			r, err := c.Get(*target)
			if err != nil {
				panic(err)
			}
			io.Copy(io.Discard, r.Body)
			r.Body.Close()
		}
	}
	start := time.Now()
	stop := start.Add(time.Duration(*seconds) * time.Second)
	var successes, failures, canceled, timedOut, downloaded int64
	var mu sync.Mutex
	var errorSamples []string
	latencies := make([][]float64, *concurrency)
	var wg sync.WaitGroup
	for worker := 0; worker < *concurrency; worker++ {
		wg.Add(1)
		go func(worker int) {
			defer wg.Done()
			c := clients[worker%len(clients)]
			buffer := make([]byte, 64*1024)
			n := 0
			for time.Now().Before(stop) {
				var closeOne func()
				if *mode == "churn" {
					c, closeOne = newClient()
				}
				began := time.Now()
				deadline := 12 * time.Second
				if *mode == "timeout" {
					deadline = 25 * time.Second
				}
				ctx, cancel := context.WithTimeout(context.Background(), deadline)
				isCancel := (*mode == "mixed" || *mode == "blocked") && n%4 == 0
				wasTimeout := false
				if isCancel && n%8 == 0 {
					cancel()
				}
				req, _ := http.NewRequestWithContext(ctx, "GET", *target, nil)
				r, err := c.Do(req)
				wasCanceled := isCancel && errors.Is(err, context.Canceled)
				var got int64
				if err == nil {
					if (*protocol == "h3" && r.ProtoMajor != 3) || (*protocol == "h2" && r.ProtoMajor != 2) {
						err = fmt.Errorf("unexpected protocol %s", r.Proto)
					}
					if r.StatusCode != 200 {
						err = fmt.Errorf("status %d", r.StatusCode)
					}
					validationErr := err
					if isCancel {
						_, readErr := r.Body.Read(buffer[:1])
						wasCanceled = validationErr == nil && readErr == nil
						err = readErr
						if validationErr != nil {
							err = validationErr
						}
						cancel()
						r.Body.Close()
					} else {
						if *mode == "blocked" {
							time.Sleep(time.Second)
						}
						if *mode == "timeout" {
							time.Sleep(10 * time.Second)
						}
						hash := sha256.New()
						got, err = io.CopyBuffer(hash, r.Body, buffer)
						r.Body.Close()
						if *mode == "timeout" {
							var reset *http3.Error
							wasTimeout = errors.As(err, &reset) && reset.Remote && reset.ErrorCode == http3.ErrCodeRequestCanceled && got < *size && validationErr == nil
							if wasTimeout {
								err = nil
							} else {
								err = fmt.Errorf("expected server timeout reset, got %d bytes: %v", got, err)
							}
						}
						if err == nil {
							err = validationErr
						}
						if err == nil && !wasTimeout && got != *size {
							err = fmt.Errorf("body size %d, expected %d", got, *size)
						}
						if err == nil && !wasTimeout && *wantHash != "" && hex.EncodeToString(hash.Sum(nil)) != *wantHash {
							err = fmt.Errorf("body digest mismatch")
						}
					}
				}
				cancel()
				if closeOne != nil {
					closeOne()
				}
				if wasCanceled {
					atomic.AddInt64(&canceled, 1)
				} else if wasTimeout {
					atomic.AddInt64(&timedOut, 1)
				} else if err != nil {
					atomic.AddInt64(&failures, 1)
					mu.Lock()
					if len(errorSamples) < 12 {
						errorSamples = append(errorSamples, err.Error())
					}
					mu.Unlock()
				} else {
					atomic.AddInt64(&successes, 1)
					atomic.AddInt64(&downloaded, got)
					ms := float64(time.Since(began)) / float64(time.Millisecond)
					if len(latencies[worker]) < 50000 {
						latencies[worker] = append(latencies[worker], ms)
					} else {
						latencies[worker][n%50000] = ms
					}
				}
				n++
			}
		}(worker)
	}
	done := make(chan struct{})
	go func() { wg.Wait(); close(done) }()
	ticker := time.NewTicker(10 * time.Second)
	defer ticker.Stop()
	for running := true; running; {
		select {
		case <-ticker.C:
			fmt.Fprintf(os.Stderr, "progress seconds=%.0f success=%d errors=%d cancel=%d\n", time.Since(start).Seconds(), atomic.LoadInt64(&successes), atomic.LoadInt64(&failures), atomic.LoadInt64(&canceled))
		case <-done:
			running = false
		}
	}
	elapsed := time.Since(start).Seconds()
	var all []float64
	for _, values := range latencies {
		all = append(all, values...)
	}
	sort.Float64s(all)
	percentile := func(p float64) float64 {
		if len(all) == 0 {
			return 0
		}
		return all[int(float64(len(all)-1)*p)]
	}
	r := result{Protocol: *protocol, Mode: *mode, Concurrency: *concurrency, Connections: *connections, Seconds: elapsed,
		Success: successes, Errors: failures, Canceled: canceled, TimedOut: timedOut, Bytes: downloaded, RPS: float64(successes) / elapsed,
		MiBPS: float64(downloaded) / elapsed / (1 << 20), P50MS: percentile(.5), P95MS: percentile(.95), P99MS: percentile(.99), ErrorSamples: errorSamples}
	json.NewEncoder(os.Stdout).Encode(r)
	if failures > 0 {
		os.Exit(1)
	}
}
