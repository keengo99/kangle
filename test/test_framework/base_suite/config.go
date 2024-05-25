package base_suite

import (
	"fmt"
	"net/http"
	"test_framework/common"
	"test_framework/config"
	"test_framework/kangle"
)

func check_listen_config() {
	kangle.CreateExtConfig("10", `<!--#start 10 -->
	<config>
		<listen ip='127.0.0.1' port='7777' type='http'/>
	</config>	
	`)
	resp := kangle.Reload("ext|10.xml_")
	common.AssertSame(resp.StatusCode, 204)
	common.AssertPort(7777, true)
	kangle.CreateExtConfig("10", `<!--#start 10 -->
	<config>		
	</config>	
	`)
	resp = kangle.Reload("ext|10.xml_")
	common.AssertSame(resp.StatusCode, 204)
	common.AssertPort(7777, false)
}
func check_vh_config() {
	access_file := config.Cfg.BasePath + "/www/access.xml"
	kangle.CreateFile(access_file, `<config><request><table name='aaa'></table></request></config>`)
	kangle.CreateExtConfig("20", `<!--#start 20 -->
<config>
	<vh doc_root='www'  access='access.xml'>
		<host>vh_config</host>
		<request/>
	</vh>
</config>
<!--configfileisok-->
`)
	kangle.Reload("ext|20.xml_")

	kangle.CreateExtConfig("20", `<!--#start 20 -->
	<config>
		<vh doc_root='www'>
			<host>vh_config</host>
			<request/>
		</vh>
	</config>
	`)
	kangle.Reload("ext|20.xml_")

	kangle.CreateExtConfig("20", `<!--#start 20 -->
		<config>
			<vh doc_root='www'>
				<host>vh_config</host>
			</vh>
		</config>
		`)
	kangle.Reload("ext|20.xml_")

	kangle.ReloadConfig()
}
func create_wrong_ssl_cert() {
	/*create wrong crt file */
	err := kangle.CreateFile(config.Cfg.BasePath+"/etc/server2.crt", "wrong format crt")
	if err != nil {
		panic(err.Error())
	}
}
func create_right_ssl_cert() {
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
	err := kangle.CreateFile(config.Cfg.BasePath+"/etc/server2.crt", str)
	if err != nil {
		panic(err.Error())
	}
}
func check_listen_ssl_config_change() {
	create_wrong_ssl_cert()
	defer create_wrong_ssl_cert()
	kangle.CreateExtConfig("20", `<!--#start 20 -->
<config>
	<listen ip='127.0.0.1' port='7766' type='https' certificate='etc/server2.crt' certificate_key='etc/server.key' http2='1' />
</config>`)
	kangle.Reload("ext|20.xml_")
	create_right_ssl_cert()
	kangle.Reload("ext|20.xml_")
	check_ssl_port_status("", 7766)
}
func check_ssl_port_status(host string, port int) {
	config.Push()
	defer config.Pop()
	config.Cfg.UrlPrefix = config.HttpsUrlPrefix
	config.Cfg.Alpn = 0
	common.Getx(fmt.Sprintf("https://127.0.0.1:%d/kangle.status", port), host, nil, func(resp *http.Response, err error) {
		common.Assert("port", err == nil)
	})
}
func check_vh_ssl_config_change() {
	/* test vh ssl */
	create_wrong_ssl_cert()
	defer create_wrong_ssl_cert()
	kangle.CreateExtConfig("20", `<!--#start 20 -->
<config>
	<vh doc_root='www'  name='test_ssl' certificate='../etc/server2.crt' certificate_key='../etc/server.key'>
		<host>`+config.GetLocalhost("testssl")+`</host>
		<bind>!*:7767s</bind>
	</vh>
</config>
<!--configfileisok-->
`)
	kangle.Reload("ext|20.xml_")
	create_right_ssl_cert()
	kangle.Reload("ext|20.xml_")
	check_ssl_port_status("", 7767)
	kangle.CreateExtConfig("20", `<!--#start 20 --><!--configfileisok-->`)
	kangle.Reload("ext|20.xml_")

}
func check_vh_host_ssl_change() {
	create_wrong_ssl_cert()
	defer create_wrong_ssl_cert()
	kangle.CreateExtConfig("20", `<!--#start 20 -->
<config>
	<vh doc_root='www'  name='test_ssl'>
		<host dir='sub;../etc/server2.crt|../etc/server.key'>`+config.GetLocalhost("testssl")+`</host>
		<bind>!*:7768s</bind>
	</vh>
</config>
<!--configfileisok-->
`)
	kangle.Reload("ext|20.xml_")
	create_right_ssl_cert()
	kangle.Reload("ext|20.xml_")
	check_ssl_port_status(config.GetLocalhost("testssl"), 7768)
	kangle.CreateExtConfig("20", `<!--#start 20 --><!--configfileisok-->`)
	kangle.Reload("ext|20.xml_")
}
func check_config() {

	check_listen_config()
	check_vh_config()
	check_listen_ssl_config_change()
	check_vh_ssl_config_change()
	check_vh_host_ssl_change()
}
