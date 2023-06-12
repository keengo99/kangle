package kangle

import (
	"bufio"
	"fmt"
	"io"
	"io/ioutil"
	"net/http"
	"os"
	"os/exec"
	"strings"
	"test_framework/common"
	"test_framework/config"
	"time"
)

var kangleCommand string
var kangle_cmd *exec.Cmd

func Reload(filename string) *http.Response {
	resp, _ := common.Http1Client.Get("http://127.0.0.1:9911/reload?file=" + filename)
	return resp
}
func CleanAllCache() {
	resp, err := common.Http1Client.Get("http://127.0.0.1:9911/clean_all_cache")
	if err != nil {
		panic(err)
	}
	common.Assert("clean_all_cache", resp.StatusCode == 200)
}
func ReloadConfig() {
	resp, _ := common.Http1Client.Get("http://127.0.0.1:9911/reload_config")
	common.Assert("reload_config", resp.StatusCode == 200)
}
func FlushDiskCache() {
	resp, _ := common.Http1Client.Get("http://127.0.0.1:9911/flush_disk_cache.km")
	common.Assert("reload_config", resp.StatusCode == 200)
}
func CreateExtConfig(name string, content string) error {
	file := config.Cfg.BasePath + "/ext/" + name + ".xml"
	fmt.Printf("create ext file [%s]\n", file)
	config_file, err := os.Create(file)
	if err != nil {
		return err
	}
	defer config_file.Close()
	_, err = config_file.WriteString(content)
	return err
}
func CleanExtConfig(name string) error {
	file := config.Cfg.BasePath + "/ext/" + name + ".xml"
	fmt.Printf("remove ext file [%s]\n", file)
	return os.Remove(file)
}

func CreateMainConfig(malloc_debug int) (err error) {
	str := `
<config>
	<worker_thread>4</worker_thread>
	<mallocdebug>%d</mallocdebug>
	<listen ip='0.0.0.0' port='9999h' type='http' />	
	<listen ip='127.0.0.1' port='9943' type='https' certificate='etc/server.crt' certificate_key='etc/server.key' alpn='3' />
	<listen ip='127.0.0.1' port='9911' type='manage' />

	<!-- 9998开启proxy protocol端口 -->
	<listen ip='127.0.0.1' port='9998P' type='http' />
	<!-- 9800中转到kangle的9998端口 -->
	<listen ip='127.0.0.1' port='9800' type='tcp' />
	<!-- 9801中转连接上游h2(test_server的4412端口) -->
	<listen ip='127.0.0.1' port='9801' type='http' />
	<!-- 9900中转连接kangle的h2(kangle的9443端口) -->
	<listen ip='127.0.0.1' port='9900h' type='http' />
	<!-- 9902 https中转上上游http(kangle的9999) -->
	<listen ip='127.0.0.1' port='9902' type='https' certificate='etc/server.crt' certificate_key='etc/server.key' http2='1' />
	<compress only_cache='0' min_length='1' gzip_level='5' br_level='5'/>
	<cache default='1' max_cache_size='256k' max_bigobj_size='1g' memory='1G' disk='1g' cache_part='1' refresh_time='30'/>
	`
	str += "<cmd name='fastcgi' proto='fastcgi' file='bin/test_child" + common.ExeExtendFile() + " f 9005' port='9005' type='sp' param='' life_time='280' idle_time='280'>"
	str += `
	</cmd>
	<server name='localhost_proxy' proto='proxy' host='127.0.0.1' port='9998' life_time='5' />
	<server name='localhost' proto='http' host='127.0.0.1' port='9999' life_time='5' />
	<server name='localhost_https' proto='http' host='127.0.0.1' port='9943sp' life_time='2' />
	<server name='upstream' host='127.0.0.1' port='4411' proto='http' life_time='10'/>
	<server name='upstream_h2c' host='127.0.0.1' port='4411h' proto='http' life_time='10'/>
	<server name='upstream_ssl' host='127.0.0.1' port='4412s' proto='http' life_time='10'/>	
	<server name='upstream_h2' host='127.0.0.1' port='4412sp' proto='http' life_time='10'/>	
	<auth_delay>0</auth_delay>
	<timeout rw='60' connect='20'/>
	<admin user='admin' password='kangle' crypt='plain' auth_type='Basic' admin_ips='~127.0.0.1'/>
	<request action='vhs'></request>
	<vhs >
		<index id='100' file='index.html'/>
		<mime_type ext='*' type='text/plain'/>
		<mime_type ext='html' type='text/html' compress='1'/>
		<mime_type ext='id' type='text/html' compress='2'/>
	</vhs>
</config>`
	config_file, err := os.Create(config.Cfg.BasePath + "/etc/config.xml")
	if err != nil {
		panic(err)
	}
	defer config_file.Close()
	_, err = config_file.WriteString(fmt.Sprintf(str, malloc_debug))
	return
}

func Prepare(kangle_path string, only_prepare bool) {
	os.Mkdir(config.Cfg.BasePath+"/etc", 0755)
	os.Mkdir(config.Cfg.BasePath+"/var", 0755)
	os.Mkdir(config.Cfg.BasePath+"/ext", 0755)
	os.Mkdir(config.Cfg.BasePath+"/bin", 0755)
	os.Mkdir(config.Cfg.BasePath+"/www/dav", 0755)
	kangleCommand = config.Cfg.BasePath + "/bin/kangle" + common.ExeExtendFile()
	common.CopyFile(kangle_path+"/kangle"+common.ExeExtendFile(), kangleCommand)
	common.CopyFile(kangle_path+"/extworker"+common.ExeExtendFile(), config.Cfg.BasePath+"/bin/extworker"+common.ExeExtendFile())
	common.CopyFile(kangle_path+"/test_child"+common.ExeExtendFile(), config.Cfg.BasePath+"/bin/test_child"+common.ExeExtendFile())
	common.CopyFile(kangle_path+"/testdso"+common.DllExtendFile(), config.Cfg.BasePath+"/bin/testdso"+common.DllExtendFile())
	common.CopyFile(kangle_path+"/vhs_sqlite"+common.DllExtendFile(), config.Cfg.BasePath+"/bin/vhs_sqlite"+common.DllExtendFile())
	common.CopyFile(kangle_path+"/filter"+common.DllExtendFile(), config.Cfg.BasePath+"/bin/filter"+common.DllExtendFile())
	common.CopyFile(kangle_path+"/webdav"+common.DllExtendFile(), config.Cfg.BasePath+"/bin/webdav"+common.DllExtendFile())
	common.CopyFile(kangle_path+"/webdav_client_example"+common.ExeExtendFile(), config.Cfg.BasePath+"/bin/webdav_client_example"+common.ExeExtendFile())
	Start(only_prepare)
	time.Sleep(time.Second)
}
func Close() {
	go Stop()
	kangle_cmd.Wait()
}
func Stop() {
	exec.Command(kangleCommand, "-q").Run()
}
func GetCompileOptions(cfg *config.KangleCompileOptions) error {
	//fmt.Printf("kangle command=[%v]\n", kangleCommand)
	cmd := exec.Command(kangleCommand, "-v")
	stdout, err := cmd.StdoutPipe()
	if err != nil {
		return err
	}
	if err := cmd.Start(); err != nil {
		return err
	}
	defer cmd.Wait()

	buf := bufio.NewReader(stdout)
	feature, err := buf.ReadString('\n')
	if err != nil && err != io.EOF {
		return err
	}
	fmt.Printf("kangle feature is [%s]\n", feature)
	if !strings.Contains(feature, " brotli") {
		cfg.DisableBrotli = true
		fmt.Printf("not found brotli support. now disable brotli test.\n")
	}
	if !strings.Contains(feature, " h3") {
		cfg.DisableHttp3 = true
		fmt.Printf("not found http3 support. now disable http3 test.\n")
	}
	return nil
}
func Start(only_prepare bool) {
	_, err := os.Stat(config.Cfg.BasePath + "/cache/f/d")
	if err != nil {
		fmt.Printf("create cache dir\n")
		exec.Command(kangleCommand, "-z", config.Cfg.BasePath+"/cache").Run()
	}
	if only_prepare {
		return
	}
	kangle_cmd = exec.Command(kangleCommand, "-n", "-g")
	kangle_cmd.Stdout = os.Stdout
	kangle_cmd.Stderr = os.Stderr
	err = kangle_cmd.Start()
	if err != nil {
		panic(err)
	}
}
func CheckExtDir() {
	ext_dir := config.Cfg.BasePath + "/ext/"
	files, err := ioutil.ReadDir(ext_dir)
	if err != nil {
		fmt.Printf("%v\n", err)
	}
	for _, f := range files {
		if f.IsDir() {
			continue
		}
		fmt.Printf("warning find ext config file [%s]\n", ext_dir+f.Name())
	}
}
