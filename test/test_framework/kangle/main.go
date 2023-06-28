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
func CreateFile(filename string, content string) error {
	config_file, err := os.Create(filename)
	if err != nil {
		return err
	}
	defer config_file.Close()
	_, err = config_file.WriteString(content)
	return err
}
func CreateExtConfig(name string, content string) error {
	file := config.Cfg.BasePath + "/ext/" + name + ".xml"
	fmt.Printf("create ext file [%s]\n", file)
	return CreateFile(file, content)
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
	<listen ip='*' port='9999h' type='http' />
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
func createSslFile() {
	str := `-----BEGIN CERTIFICATE-----
MIIDETCCAfkCFGqw5HR92Ds5slo2Pfq8z3Bxdo1iMA0GCSqGSIb3DQEBCwUAMEUx
CzAJBgNVBAYTAkNOMRMwEQYDVQQIDApTb21lLVN0YXRlMSEwHwYDVQQKDBhJbnRl
cm5ldCBXaWRnaXRzIFB0eSBMdGQwHhcNMjIwMjIyMDMyMDM4WhcNMjMwMjIyMDMy
MDM4WjBFMQswCQYDVQQGEwJDTjETMBEGA1UECAwKU29tZS1TdGF0ZTEhMB8GA1UE
CgwYSW50ZXJuZXQgV2lkZ2l0cyBQdHkgTHRkMIIBIjANBgkqhkiG9w0BAQEFAAOC
AQ8AMIIBCgKCAQEA4fPjieRPLf83Hgevm8x39KmfuJ5eVH/r5lPw9uBBC8VOdeqI
Qdq96Cmi9VHdKvDxpEYoeQ5fkLqQzX/DKOWOPHpj+Wgp5C7c81fEHlSvMhKlQUbE
7XkonEsBkdtJ6FrLufTsagfJ6BTTGgp8vRn6JpGEPecC7OLL/DTQsJxwdgmVdpXr
wjGHC6LgoVUOfrASCTg4rZJDHKiKjfZtRa4N5iGUjFR3hEnTftpnubZAQi0tzaCE
g6M0sga80tAAegdRhs+j4WKrm1lCipjNIi+klwbTlLeMENjrI3nvrwsfafzLP44v
UYYgnwW7xlyG8AyveTYueoAvItVDyFpKMt1JlQIDAQABMA0GCSqGSIb3DQEBCwUA
A4IBAQApjHj3b8SzeOOLdaJX0i6T/A3MN74HH7x3fMZMCrpmwWjTLq8KoKimyZgh
XzPiaBVra2SVZE63RgWE3wkLOp84K0WF/cKVS5+ebaZ52aqy6ODVAsx14rjhlIRQ
BgX7FBXWVJN/5DHru5sgsAA4UQLKF9/zh6IlWgtH5qJ3K6jx8JY90R9QMX1gJnJX
ppspxmzNu2IPbg7wui6LAg0ETslp4ablsbeP8ew9lKQCR3ZbCunpiyn8FbGl8pAY
lf+S1EJI0uY6kqmt3zmRTpv5ysIm7NLdcPdUYIquK0ceOr69LGni80g5r6vsGAWC
ze3XUTzbzWM1/+07065mw7bYgl/U
-----END CERTIFICATE-----`
	err := CreateFile(config.Cfg.BasePath+"/etc/server.crt", str)
	if err != nil {
		panic(err.Error())
	}
	str = `-----BEGIN RSA PRIVATE KEY-----
MIIEpgIBAAKCAQEA4fPjieRPLf83Hgevm8x39KmfuJ5eVH/r5lPw9uBBC8VOdeqI
Qdq96Cmi9VHdKvDxpEYoeQ5fkLqQzX/DKOWOPHpj+Wgp5C7c81fEHlSvMhKlQUbE
7XkonEsBkdtJ6FrLufTsagfJ6BTTGgp8vRn6JpGEPecC7OLL/DTQsJxwdgmVdpXr
wjGHC6LgoVUOfrASCTg4rZJDHKiKjfZtRa4N5iGUjFR3hEnTftpnubZAQi0tzaCE
g6M0sga80tAAegdRhs+j4WKrm1lCipjNIi+klwbTlLeMENjrI3nvrwsfafzLP44v
UYYgnwW7xlyG8AyveTYueoAvItVDyFpKMt1JlQIDAQABAoIBAQCtqGVruFYGkw0I
fn3AL0DOgIOqP8VeCkcC6ebbxvUXF9i6lbuNaZHlWgLNqtJhy3bce7Nlft+B+3GJ
DzWuO+e6oZIuwJjZsA7O09h+OzW/NUdfSQXXQfQtUxRsxm4iL44+aHg+8aeDQGYS
sJa4O7vfYp2Reffsmk6OkwUFh+aDQF6bWo0Dop6Ov1F3I/Q4/4BSMwGULf4TlvRC
CebwlS9icjbM9sdZ+/+iywMupatbpjQ2iKohJvBYrapK0Kg1t+HyNBZo7rmbCku1
+8lbfpkaNWAsdpHL0LoYcgIIPar2/BbPjED0BYsA70wj7LbT3JRqvWLJO1rMad5r
gvXeclCtAoGBAPZWU/2RkDI3pTtbsr9DNhJq1ZuzvgWr/MHCyKv8eUn4Bq8vwjZr
GzyLfijUnO887at/UM7m2rkG89FS4cMxT2RdXqriQfauKR1FRDeOrSLrYtx/260y
H4Ry1sDNJ7DrQb118ooC3ppi4mg2tT+McUj/+KKvTRM2CGd51On0w+ufAoGBAOrQ
3PcB09CUbgdPmUF4HLFIpaHNpi5gs6Hq8DtImxIr/dPytdHo7+yprmYxyMYzZ4d8
fdMXgS7wmOUAeiztO6/GbdqdBQ4lUIRp5C2cx82iZek80uTCOA1b97pa1oVjasbm
FwaW/bANHP5FTre8ENMiuOAIbDC9LdHyWRxQIX5LAoGBAIyS0RVXtvjhRlpsRsHc
wgOakdFrrhmgfvm3hTqYNkLe1jmswGC7mGxhkhoM0o23sE14tw2LMe/6prKiYJE6
F3tHyRktSsVRt8arW3V05xqRRvZbxGm+u7uiqSiXKnpMllRe9YyKfKuPmHIuHhpo
s9EbubBk50/6OquKG9Vyx0czAoGBAIU0EHUKk1a6LKR3EhAii9xBwrvDxiZ+8sfC
V565tEYdsHLgNyYphpjxNJ6CVUuh83PXOiVaKw0urP0TRTthJD+1R7IA6tI4drF2
xFrfmjRbkIY728KrLlLdvez4BMNMP1EvSxaQ5r5M4gqX1GzEAaNUCh4EiSMo3epA
GS7Hggh7AoGBALFfnYEL0BtqGL9Sm7pLk1smtQ5F1T4mR3VsPnBZED1+513b/54V
mBktbUKiCi0n5NrLYQFnSLMAmGtn8stH+rSmB9pvk/YlSXbtRhmoeltO+WOx8i+4
7raWgb152Nm97jiGKEb/AmEPj13m7Inu0XLfWdQI4ScpRNVeNqclzVhT
-----END RSA PRIVATE KEY-----`
	err = CreateFile(config.Cfg.BasePath+"/etc/server.key", str)
	if err != nil {
		panic(err.Error())
	}
}
func Prepare(kangle_path string, only_prepare bool) {
	fmt.Printf("prepare kangle_path [%v] test_base_path=[%v] for test.\n", kangle_path, config.Cfg.BasePath)
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
	createSslFile()
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
