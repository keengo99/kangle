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
}

func benchmark(count int, url string, report chan int, ch chan *benchmarkResult) {
	result := &benchmarkResult{}
	StartTime := time.Now().UnixMicro()
	defer func() {
		result.UseTime = time.Now().UnixMicro() - StartTime
		ch <- result
		close(ch)
	}()
	successed := common.Bench("GET", url, nil, count, report)
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
	report := make(chan int)
	go func() {
		last_report_time := time.Now().UnixMilli()
		var last_report_size int64
		for {
			n := <-report
			last_report_size += int64(n)
			now_time := time.Now().UnixMilli()
			past_time := now_time - last_report_time
			if past_time > 1000 {
				fmt.Printf("speed %.2f(K/s)\n", float64(last_report_size)/(float64(past_time)/1000.00)/1024)
				last_report_size = 0
				last_report_time = now_time
			}
		}
	}()
	for n := 0; n < *concurrenct; n++ {
		chs[n] = make(chan *benchmarkResult)
		go benchmark(*numbers, url, report, chs[n])
	}
	result := &benchmarkResult{}

	for n := 0; n < *concurrenct; n++ {
		br := <-chs[n]
		result.Failed += br.Failed
		result.Successed += br.Successed
		result.UseTime += br.UseTime
	}
	totalUseTime := time.Now().UnixMicro() - StartTime
	fmt.Printf("benchmark end.\n")
	fmt.Printf("success: %v\n", result.Successed)
	fmt.Printf("failed:  %v\n", result.Failed)
	fmt.Printf("time:    %v\n", totalUseTime)
}
