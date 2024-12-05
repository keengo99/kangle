package framework

import (
	"flag"
	"fmt"
	"os/signal"
	"strings"
	"test_framework/common"
	"test_framework/config"
	"test_framework/kangle"

	"os"

	"path/filepath"
	"runtime"
	"test_framework/server"
	"test_framework/suite"
	"time"
)

var (
	kangle_base    *string = flag.String("o", "", "kangle test base path")
	kangle_exe     *string = flag.String("e", "", "kangle build output path")
	test_case      *string = flag.String("t", "", "test suite[.case][,suite[.case]])")
	list_case      *bool   = flag.Bool("l", false, "list all test case")
	server_model   *bool   = flag.Bool("s", false, "only run server")
	malloc_debug   *bool   = flag.Bool("m", true, "malloc debug")
	upstream       *string = flag.String("u", "", "upstream config(s,h2)")
	clean_cache    *bool   = flag.Bool("cc", true, "clean cache")
	force          *bool   = flag.Bool("f", false, "continue when error happened")
	test_count     *int    = flag.Int("c", 1, "test count")
	alpn           *int    = flag.Int("a", -1, "set alpn")
	prepare_config *bool   = flag.Bool("p", false, "only prepare config")
)

func ProcessSuites(suites []string) (success_count int, failed_count int) {
	defer func() {
		if err := recover(); err != nil {
			fmt.Println(err) //这里的err其实就是panic传入的内容，55
			buf := make([]byte, 2048)
			runtime.Stack(buf, false)
			fmt.Printf("--%s\n", buf)
		}
		if len(*kangle_exe) > 0 && !*prepare_config {
			kangle.Close()
		}
	}()
	kangle.Mkdir()
	suite.Init(suites)
	if *malloc_debug {
		kangle.CreateMainConfig(1)
	} else {
		kangle.CreateMainConfig(0)
	}
	config.Cfg.Force = *force
	if len(*kangle_exe) > 0 {
		kangle.Prepare(*kangle_exe, *prepare_config)
		err := kangle.GetCompileOptions(&config.Kangle)
		if err != nil {
			fmt.Printf("get compile options error=[%v]\n", err)
			panic("")
		}
	}
	if *prepare_config {
		return 0, 0
	}
	if *server_model {
		server.Start()
	} else {
		go server.Start()
		time.Sleep(time.Second)
	}
	for i := 0; i < *test_count; i++ {
		if *alpn >= 0 {
			if *clean_cache {
				kangle.CleanAllCache()
			}
			config.UseHttpClient(*alpn)
			fmt.Printf("test use url prefix=[%s] alpn=[%v]\n", config.Cfg.UrlPrefix, config.Cfg.Alpn)
			suite.Process(suites)
		} else {
			for alpn := config.HTTP1; alpn <= config.H2C; alpn++ {
				if alpn == config.HTTP3 && config.Kangle.DisableHttp3 {
					//skip http3 test
					continue
				}
				if *clean_cache {
					kangle.CleanAllCache()
				}
				config.UseHttpClient(alpn)
				fmt.Printf("test use url prefix=[%s] alpn=[%v]\n", config.Cfg.UrlPrefix, config.Cfg.Alpn)
				suite.Process(suites)
			}
		}
	}
	success_count, fc := common.Report()
	if len(*kangle_exe) > 0 {
		suite.Clean(suites)
		kangle.Close()
	}
	return success_count, fc
}
func handle_signal() {
	c := make(chan os.Signal, 1)
	signal.Notify(c, os.Interrupt, os.Kill)
	// Block until a signal is received.
	<-c
	buf := make([]byte, 1<<16)
	runtime.Stack(buf, true)
	fmt.Printf("%s", buf)
	os.Exit(1)
}
func Main() {
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
	common.InitClient(true)
	if len(*kangle_base) > 0 {
		config.Cfg.BasePath = *kangle_base
	} else {
		path, _ := os.Executable()
		config.Cfg.BasePath = filepath.Dir(filepath.Dir(path))
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
	//server.StartFcgiServer()
	var suites []string
	if len(*test_case) == 0 {
		suites = suite.GetSuies()
	} else {
		suites = strings.Split(*test_case, ",")
	}
	go handle_signal()
	_, fc := ProcessSuites(suites)
	if fc > 0 {
		os.Exit(1)
	}
	os.Exit(0)
}
