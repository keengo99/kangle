package kangle

import (
	"fmt"
	"io/ioutil"
	"os"
	"os/exec"
	"runtime"
	"test_server/common"
	"test_server/config"
	"time"
)

var KangleCommand string
var kangle_cmd *exec.Cmd

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
	<listen ip='127.0.0.1' port='9999' type='http' />	
	<listen ip='127.0.0.1' port='9943' type='https' certificate='etc/server.crt' certificate_key='etc/server.key' http2='1' />
	<listen ip='127.0.0.1' port='9911' type='manage' />
	<admin user='admin' password='kangle' crypt='plain' auth_type='Basic' admin_ips='~127.0.0.1'/>
	<request action='vhs' >
	</request>
</config>`

	config_file, err := os.Create(config.Cfg.BasePath + "/etc/config.xml")
	if err != nil {
		panic(err)
	}
	defer config_file.Close()
	_, err = config_file.WriteString(fmt.Sprintf(str, malloc_debug))
	return
}
func Prepare(kangle string, kangle_bin string) {
	os.Mkdir(config.Cfg.BasePath+"/etc", 0755)
	os.Mkdir(config.Cfg.BasePath+"/var", 0755)
	os.Mkdir(config.Cfg.BasePath+"/ext", 0755)
	os.Mkdir(config.Cfg.BasePath+"/"+kangle_bin, 0755)
	KangleCommand = config.Cfg.BasePath + "/" + kangle_bin + "/kangle.exe"

	err := common.CopyFile(kangle, KangleCommand)
	if err != nil {
		panic(err)
	}
	test_src_dso := ""
	test_dst_dso := config.Cfg.BasePath + "/bin/testdso."
	if runtime.GOOS == "windows" {
		test_src_dso = config.Cfg.BasePath + "/../Debug/testdso."
		test_src_dso += "dll"
		test_dst_dso += "dll"
	} else {
		test_src_dso = config.Cfg.BasePath + "/../build/testdso."
		test_src_dso += "so"
		test_dst_dso += "so"
	}
	err = common.CopyFile(test_src_dso, test_dst_dso)
	if err != nil {
		panic(err)
	}
	Start()
	time.Sleep(time.Second)
}
func Close() {
	go Stop()
	kangle_cmd.Wait()
}
func Stop() {
	exec.Command(KangleCommand, "-q").Run()
}
func Start() {
	_, err := os.Stat(config.Cfg.BasePath + "/cache/f/d")
	if err != nil {
		fmt.Printf("create cache dir\n")
		exec.Command(KangleCommand, "-z", config.Cfg.BasePath+"/cache").Run()
	}
	kangle_cmd = exec.Command(KangleCommand, "-n", "-g")
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
