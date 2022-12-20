package main

import (
	"flag"
	"fmt"
	"test_server/common"
	"test_server/config"
	"time"
)

type benchmarkResult struct {
	Successed int
	Failed    int
	UseTime   int64
	Body      int64
}

func benchmark(count int, url string, ch chan *benchmarkResult) {
	result := &benchmarkResult{}
	StartTime := time.Now().UnixMicro()
	defer func() {
		result.UseTime = time.Now().UnixMicro() - StartTime
		ch <- result
		close(ch)
	}()
	total_read, successed := common.Bench("GET", url, nil, count)
	result.Body = total_read
	result.Successed = successed
	result.Failed = count - successed
}
func main() {
	concurrenct := flag.Int("c", 1, "concurrenct")
	numbers := flag.Int("n", 1, "test count per concurrenct")
	keepAlive := flag.Bool("k", false, "keep-alive")
	alpn := flag.Int("a", 0, "https alpn protocol value 0,1,2 mean use http1 http2 http3")
	flag.Parse()
	url := flag.Arg(0)
	config.Cfg.Alpn = *alpn
	common.InitClient(*keepAlive)
	chs := make([]chan *benchmarkResult, *concurrenct)
	fmt.Printf("benchmark url=[%v] concurrent=[%v] numbers=[%v].\n", url, *concurrenct, *numbers)
	StartTime := time.Now().UnixMicro()
	for n := 0; n < *concurrenct; n++ {
		chs[n] = make(chan *benchmarkResult)
		go benchmark(*numbers, url, chs[n])
	}
	result := &benchmarkResult{}

	for n := 0; n < *concurrenct; n++ {
		br := <-chs[n]
		result.Body += br.Body
		result.Failed += br.Failed
		result.Successed += br.Successed
		result.UseTime += br.UseTime
	}
	totalUseTime := time.Now().UnixMicro() - StartTime
	fmt.Printf("benchmark end.\n")
	fmt.Printf("success: %v\n", result.Successed)
	fmt.Printf("failed:  %v\n", result.Failed)
	fmt.Printf("body:    %v\n", result.Body)
	fmt.Printf("time:    %v\n", totalUseTime)
}
