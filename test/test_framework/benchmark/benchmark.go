package main

import (
	"bytes"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"net/http"
	"os"
	"sort"
	"sync"
	"test_framework/common"
	"test_framework/config"
	"time"
)

type benchmarkResult struct {
	Success int      `json:"success"`
	Failed  int      `json:"failed"`
	Bytes   int64    `json:"bytes"`
	Seconds float64  `json:"seconds"`
	RPS     float64  `json:"rps"`
	P50     float64  `json:"p50_ms"`
	P95     float64  `json:"p95_ms"`
	P99     float64  `json:"p99_ms"`
	Errors  []string `json:"errors,omitempty"`
}

func main() {
	concurrency := flag.Int("c", 1, "concurrent workers")
	count := flag.Int("n", 1, "requests per worker")
	keepAlive := flag.Bool("k", false, "HTTP/1 keep-alive (HTTP/2 multiplexes by default)")
	alpn := flag.Int("a", 0, "protocol: 0 HTTP/1, 1 HTTPS HTTP/2, 2 HTTP/3, 3 h2c")
	postFile := flag.String("post-file", "", "POST this file instead of GET")
	expectHash := flag.String("sha256", "", "expected response SHA-256 (optional)")
	flag.Parse()
	if *concurrency <= 0 || *count <= 0 || flag.NArg() != 1 {
		fmt.Fprintln(os.Stderr, "positive -c/-n and one URL are required")
		os.Exit(2)
	}
	var payload []byte
	method := "GET"
	if *postFile != "" {
		var err error
		payload, err = os.ReadFile(*postFile)
		if err != nil {
			panic(err)
		}
		method = "POST"
	}
	config.Cfg.Alpn = *alpn
	common.InitClient(*keepAlive)
	client := common.GetClient(flag.Arg(0))
	client.Timeout = time.Duration(common.HTTP_TIME_OUT) * time.Second
	if transport, ok := client.Transport.(*http.Transport); ok {
		transport.MaxIdleConns = *concurrency
		transport.MaxIdleConnsPerHost = *concurrency
	}
	var lock sync.Mutex
	var workers sync.WaitGroup
	result := benchmarkResult{}
	latencies := make([]float64, 0)
	started := time.Now()
	for worker := 0; worker < *concurrency; worker++ {
		workers.Add(1)
		go func() {
			defer workers.Done()
			local := benchmarkResult{}
			latency := make([]float64, 0, *count)
			for n := 0; n < *count; n++ {
				start := time.Now()
				req, err := http.NewRequest(method, flag.Arg(0), bytes.NewReader(payload))
				var resp *http.Response
				if err == nil {
					resp, err = client.Do(req)
				}
				if err == nil {
					var size int64
					if *expectHash != "" {
						hash := sha256.New()
						size, err = io.Copy(hash, resp.Body)
						if err == nil && hex.EncodeToString(hash.Sum(nil)) != *expectHash {
							err = fmt.Errorf("response hash mismatch")
						}
					} else {
						size, err = io.Copy(io.Discard, resp.Body)
					}
					resp.Body.Close()
					local.Bytes += size
					if err == nil && (resp.StatusCode < 200 || resp.StatusCode >= 300) {
						err = fmt.Errorf("HTTP status %d", resp.StatusCode)
					}
					if err == nil && resp.ContentLength >= 0 && size != resp.ContentLength {
						err = fmt.Errorf("truncated body")
					}
					if err == nil && (*alpn == 1 || *alpn == 3) && resp.ProtoMajor != 2 {
						err = fmt.Errorf("expected HTTP/2, got %s", resp.Proto)
					}
				}
				latency = append(latency, float64(time.Since(start).Microseconds())/1000)
				if err == nil {
					local.Success++
				} else {
					local.Failed++
					if len(local.Errors) < 3 {
						local.Errors = append(local.Errors, err.Error())
					}
				}
			}
			lock.Lock()
			result.Success += local.Success
			result.Failed += local.Failed
			result.Bytes += local.Bytes
			latencies = append(latencies, latency...)
			for _, err := range local.Errors {
				if len(result.Errors) < 3 {
					result.Errors = append(result.Errors, err)
				}
			}
			lock.Unlock()
		}()
	}
	workers.Wait()
	result.Seconds = time.Since(started).Seconds()
	result.RPS = float64(result.Success) / result.Seconds
	sort.Float64s(latencies)
	percentile := func(p int) float64 { return latencies[(len(latencies)-1)*p/100] }
	result.P50, result.P95, result.P99 = percentile(50), percentile(95), percentile(99)
	json.NewEncoder(os.Stdout).Encode(result)
	if result.Failed != 0 {
		os.Exit(1)
	}
}
