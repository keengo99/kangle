package main

import (
	"flag"
	"fmt"
	"strings"
	"test_server/common"
	"test_server/config"
	"time"

	"os"
	"test_server/kangle"

	"path/filepath"
	"runtime"
	"test_server/server"
	"test_server/suite"
)

var (
	kangle_base  *string = flag.String("o", "", "kangle test base path")
	kangle_exe   *string = flag.String("e", "", "kangle build output path")
	test_case    *string = flag.String("t", "", "test suite[.case][,suite[.case]])")
	list_case    *bool   = flag.Bool("l", false, "list all test case")
	server_model *bool   = flag.Bool("s", false, "only run server")
	malloc_debug *bool   = flag.Bool("m", false, "malloc debug")
	upstream     *string = flag.String("u", "", "upstream config(s,h2)")
	force        *bool   = flag.Bool("f", false, "continue when error happened")
)

func ProcessSuites(suites []string) {
	defer func() {
		if err := recover(); err != nil {
			fmt.Println(err) //这里的err其实就是panic传入的内容，55
			buf := make([]byte, 2048)
			runtime.Stack(buf, false)
			fmt.Printf("--%s\n", buf)
		}
		if len(*kangle_exe) > 0 {
			kangle.Close()
		}
	}()
	suite.Init(suites)
	if *server_model {
		for {
			time.Sleep(time.Second)
		}
	}

	if *malloc_debug {
		kangle.CreateMainConfig(1)
	} else {
		kangle.CreateMainConfig(0)
	}
	config.Cfg.Force = *force
	if len(*kangle_exe) > 0 {
		kangle.Prepare(*kangle_exe)
	} else {
		kangle.ReloadConfig()
	}

	//url_prefixs := []string{"https://127.0.0.1:9943", "http://127.0.0.1:9999"}
	url_prefixs := []string{"http://127.0.0.1:9999", "https://127.0.0.1:9943"}
	//url_prefixs := []string{"http://127.0.0.1:9999"}
	//url_prefixs := []string{"https://127.0.0.1:9943"}
	for _, url_prefix := range url_prefixs {
		config.Cfg.UrlPrefix = url_prefix
		config.Cfg.Http2 = true
		fmt.Printf("test use url prefix=[%s]\n", config.Cfg.UrlPrefix)
		kangle.CleanAllCache()
		suite.Process(suites)
	}
	common.Report()
	suite.Clean(suites)
	if len(*kangle_exe) > 0 {
		kangle.Close()
	} else {
		kangle.ReloadConfig()
	}
}
func main() {
	fmt.Printf("kangle test...\n")
	flag.Usage = func() {
		fmt.Fprintf(os.Stderr, "Usage: %s [OPTIONS]\r\n", os.Args[0])
		flag.PrintDefaults()
	}
	flag.Parse()
	if *list_case {
		suite.List()
		return
	}
	common.InitClient()

	if len(*kangle_base) > 0 {
		config.Cfg.BasePath = *kangle_base
	} else {
		config.Cfg.BasePath = filepath.Dir(filepath.Dir(os.Args[0]) + "/../")
	}
	if *upstream == "s" {
		config.Cfg.UpstreamSsl = true
	} else if *upstream == "h2" {
		config.Cfg.UpstreamHttp2 = true
		config.Cfg.UpstreamSsl = true
	}
	fmt.Printf("os=[%v] base dir=[%s] upstream ssl=[%v] h2=[%v]\n",
		runtime.GOOS,
		config.Cfg.BasePath,
		config.Cfg.UpstreamSsl,
		config.Cfg.UpstreamHttp2)
	kangle.CheckExtDir()
	server.Start()
	//server.StartFcgiServer()
	var suites []string
	if len(*test_case) == 0 {
		suites = suite.GetSuies()
	} else {
		suites = strings.Split(*test_case, ",")
	}
	ProcessSuites(suites)
	b := make([]byte, 1)
	os.Stdin.Read(b)

}
