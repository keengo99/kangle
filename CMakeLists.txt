cmake_minimum_required(VERSION 2.8.12) 
project(kangle)
set(KANGLE_VERSION "3.6.0")

set(CPACK_PACKAGE_VERSION ${KANGLE_VERSION})
set(KANGLE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_CXX_STANDARD 14)
file(GLOB KANGLE_SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h  
    ${CMAKE_CURRENT_SOURCE_DIR}/module/access/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/module/whm/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/module/whm/whmdso/core/*.cpp    
    )
list(APPEND KANGLE_SOURCE_FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/khttpd/davlib/KXml.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/khttpd/davlib/KXmlContext.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/khttpd/davlib/KXmlException.cpp
    )
file(GLOB EXTWORKER_SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/extworker/*.cpp
    )
file(GLOB VHS_SQLITE_SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/module/vhs_sqlite/*.cpp
)
file(GLOB TESTDSO_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/module/testdso/*.cpp)

list(APPEND EXTWORKER_SOURCE_FILES 
    src/EdcodeUtils.cpp 
    src/KHttpExt.cpp 
    src/KApiDso.cpp 
    src/KApiEnv.cpp 
    src/KBuffer.cpp 
    src/KEnvInterface.cpp 
    src/KFastcgiUtils.cpp 
    src/KPipeStream.cpp
    src/KProcess.cpp 
    khttpd/src/KStream.cpp 
    khttpd/src/KStringBuf.cpp 
    src/KCgiEnv.cpp 
    src/process.cpp 
    src/KDynamicString.cpp
    khttpd/src/KAutoBuffer.cpp
    src/win_iconv.cpp
)
list(APPEND KANGLE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
list(APPEND KANGLE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/module/whm)
list(APPEND KANGLE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/module/access)
list(APPEND KANGLE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/khttpd/davlib)

include(CheckSymbolExists)
include(CheckIncludeFile)
include(./cmake/FindPCRE.cmake)
list(APPEND KANGLE_INCLUDE_DIR ${PCRE_INCLUDE_DIRS}) 
check_symbol_exists(timegm time.h HAVE_TIMEGM)
if (HAVE_TIMEGM)
    add_definitions(-DHAVE_TIMEGM)
endif()
if (LSQUIC_DIR)
    add_definitions(-DENABLE_HTTP3)
endif()
#message(STATUS ${PCRE_INCLUDE_DIRS})
check_symbol_exists(KSOCKET_SSL ${KASYNC_INCLUDE_DIR}/kasync_config.h HAVE_KSOCKET_SSL)
set(KANGLE_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/kasync/build")
option(ENABLE_BROTLI "enable brotli compress support" OFF)
option(HTTP_PROXY "build kangle as proxy server" OFF)
option(ENABLE_DISK_CACHE "enable disk cache" ON)

if (ENABLE_BROTLI)
    check_include_file("brotli/encode.h" ENABLE_BROTLI)
    if (NOT ENABLE_BROTLI)
        message(FATAL_ERROR "cann't found brotli libary")
    else()
	    list(APPEND KANGLE_LIBS "brotlienc-static" "brotlicommon-static")
	    #      list(APPEND KANGLE_LIBS "brotlicommon" "brotlienc")
    endif()
endif()
set(KASYNC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/kasync)
add_subdirectory(khttpd)


#list(APPEND KANGLE_SOURCE_FILES ${KHTTPD_SOURCE_FILES})
list(APPEND KANGLE_INCLUDE_DIR ${KHTTPD_INCLUDE_DIR})
list(APPEND KANGLE_LIBS ${KHTTPD_LIBS})
#windows
if (CMAKE_SYSTEM_NAME MATCHES "Windows")
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/build)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/build)
    list(APPEND KANGLE_LIBS ws2_32)
    list(APPEND KANGLE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdinclude")
    list(APPEND KANGLE_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/win32lib/${CMAKE_VS_PLATFORM_NAME}")
    list(APPEND KANGLE_LIBS zdll pcre)
    list(APPEND EXTWORKER_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/KWinCgiEnv.cpp")
    list(APPEND VHS_SQLITE_SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/module/vhs_sqlite/vhs_sqlite.def")
else()
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build)
	include(FindZLIB)
	add_compile_options(-Wno-invalid-offsetof)
	if (NOT ZLIB_FOUND) 
		message(FATAL_ERROR "cann't found zlib")
	endif()
	list(APPEND KANGLE_LIBS ZLIB::ZLIB)
	list(APPEND KANGLE_LIBS ${PCRE_LIBRARIES})
	if (CMAKE_SYSTEM_NAME MATCHES "Linux")
		list(APPEND KANGLE_LIBS dl)
    elseif (CMAKE_SYSTEM_NAME MATCHES "Darwin")
        list(APPEND KANGLE_LIBS iconv)
	endif()	
endif()

include(FindSQLite3)
if (SQLite3_FOUND)
    list(APPEND KANGLE_LIBS SQLite::SQLite3)
    set(SQLITE3_LIB SQLite::SQLite3)
else()
    add_library(sqlite3 ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite/sqlite3.c)
    if (NOT(CMAKE_SYSTEM_NAME MATCHES "Windows"))
        target_compile_options(sqlite3 PUBLIC -fPIC)
    endif()
    target_include_directories(sqlite3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite)
    list(APPEND KANGLE_LIBS sqlite3)
    set(SQLITE3_LIB sqlite3)
endif()

if (ENABLE_JEMALLOC)
	list(APPEND KANGLE_LIBS jemalloc)
endif()
message(STATUS "kangle version ${KANGLE_VERSION}")
configure_file(${KANGLE_DIR}/include/global.h.in ${KANGLE_DIR}/include/global.h @ONLY)
add_executable(kangle ${KANGLE_SOURCE_FILES} ${KHTTPD_INCLUDE_FILES})
list(APPEND KANGLE_LIBS khttpd)
target_include_directories(kangle PUBLIC ${KANGLE_INCLUDE_DIR})
target_link_directories(kangle PUBLIC ${KANGLE_LIBS_DIR})
#message(STATUS "kangle link libs ${KANGLE_LIBS} http_libs ${KHTTPD_LIBS} ${KASYNC_LIBS}")
target_link_libraries(kangle ${KANGLE_LIBS})
if (NOT HTTP_PROXY) 
    add_executable(extworker ${EXTWORKER_SOURCE_FILES})
    add_library(vhs_sqlite MODULE  ${VHS_SQLITE_SOURCE_FILES})
    add_library(testdso MODULE ${TESTDSO_SOURCE_FILES})

    target_include_directories(extworker PUBLIC ${KANGLE_INCLUDE_DIR})
    target_link_directories(extworker PUBLIC ${KANGLE_LIBS_DIR})
    target_link_libraries(extworker ${KANGLE_LIBS})

    set_property(TARGET vhs_sqlite PROPERTY  PREFIX "")
    target_include_directories(vhs_sqlite PUBLIC ${KANGLE_INCLUDE_DIR})
    target_link_directories(vhs_sqlite PUBLIC ${KANGLE_LIBS_DIR})
    target_link_libraries(vhs_sqlite ${SQLITE3_LIB})

    set_property(TARGET testdso PROPERTY  PREFIX "")
    target_include_directories(testdso PUBLIC ${KANGLE_INCLUDE_DIR})
    target_link_directories(testdso PUBLIC ${KANGLE_LIBS_DIR})

    install(TARGETS extworker vhs_sqlite
     RUNTIME DESTINATION bin 
     LIBRARY DESTINATION bin
     )
endif()
add_subdirectory(module/webdav)
add_subdirectory(test/test_child)
add_dependencies(kangle khttpd)

install(TARGETS kangle
     RUNTIME DESTINATION bin 
     LIBRARY DESTINATION bin
     )

install(FILES 
     ${KANGLE_DIR}/include/ksapi.h 
     ${KANGLE_DIR}/kasync/include/kfeature.h    
     ${KANGLE_DIR}/kasync/include/kforwin32.h     
     ${KANGLE_DIR}/kasync/include/kasync_config.h
     ${KANGLE_DIR}/khttpd/include/khttp.h   
     TYPE INCLUDE)
install(DIRECTORY
    ${KANGLE_DIR}/webadmin/
    DESTINATION "webadmin"
    )
install(FILES
    ${KANGLE_DIR}/etc/config-default.xml
    DESTINATION etc)

include(CPack)
set(CPACK_PACKAGE_NAME "kangle")
nagios-plugins-ifstatus-2.4.2-r0
perl-exporter-doc-5.74-r1
etckeeper-bash-completion-1.18.18-r0
rsyslog-openrc-8.2212.0-r0
nagios-plugins-overcr-2.4.2-r0
mbedtls-static-2.28.2-r0
perl-data-uuid-1.226-r3
ipset-openrc-7.15-r1
device-mapper-libs-2.03.17-r1
perl-fcgi-0.82-r1
vulkan-loader-dbg-1.3.231.2-r0
perl-http-message-doc-6.44-r0
nginx-mod-http-perl-1.22.1-r0
sshguard-doc-2.4.2-r1
lua5.1-unit-3.4-r0
swig-doc-4.0.2-r7
perl-sub-identify-doc-0.14-r7
uwsgi-carbon-2.0.21-r0
linux-firmware-ti-connectivity-20221214-r0
perl-filesys-notify-simple-0.14-r1
charybdis-openrc-4.1.2-r5
lsyncd-doc-2.3.1-r0
nodejs-doc-18.12.1-r0
kamailio-jansson-5.6.2-r1
drbd-utils-pacemaker-9.22.0-r1
vanessa_logger-doc-0.0.10-r2
static-routing-1.0-r2
gnokii-libs-0.6.31-r13
perl-net-dns-doc-1.35-r0
uwsgi-stats_pusher_socket-2.0.21-r0
gnokii-smsd-mysql-0.6.31-r13
cairo-dbg-1.17.6-r3
lua-soap-3.0-r1
kamailio-dbg-5.6.2-r1
musl-nscd-doc-1.1.1-r1
perl-devel-stacktrace-ashtml-0.15-r2
sharutils-doc-4.15.2-r2
perl-dist-checkconflicts-doc-0.11-r2
syslog-ng-sql-3.38.1-r0
squid-lang-fa-5.7-r0
ferm-openrc-2.7-r0
speexdsp-doc-1.2.1-r0
lcms2-doc-2.14-r0
alpine-baselayout-data-3.4.0-r0
libxpm-doc-3.5.14-r0
libx11-static-1.8.3-r0
gdb-doc-12.1-r2
lighttpd-dbg-1.4.67-r0
aconf-openrc-0.8.1-r2
openldap-lloadd-2.6.3-r6
xkbcomp-dev-1.4.5-r1
rsyslog-mmcount-8.2212.0-r0
apr-util-dbd_pgsql-1.6.1-r14
hylafax-lang-6.0.7-r4
perdition-openrc-2.2-r4
irssi-proxy-1.4.3-r0
readline-doc-8.2.0-r0
py3-libxml2-2.10.3-r1
command-not-found-0.3-r1
perl-php-serialization-doc-0.34-r3
py3-sphinxcontrib-websupport-1.2.4-r2
rsyslog-zmq-8.2212.0-r0
libxfixes-6.0.0-r0
perl-file-slurp-9999.32-r1
czmq-4.2.1-r2
nagios-plugins-nagios-2.4.2-r0
knot-mod-dnstap-3.2.4-r0
lua5.1-sql-odbc-2.6.0-r2
asterisk-speex-18.15.0-r0
postgresql15-doc-15.1-r0
busybox-openrc-1.35.0-r29
perl-convert-asn1-0.33-r0
udev-init-scripts-openrc-35-r1
curl-doc-7.87.0-r0
lua5.4-unit-3.4-r0
asterisk-sounds-en-18.15.0-r0
lua5.4-maxminddb-0.1-r3
lua5.2-stringy-0.5.1-r2
libwebsockets-doc-4.3.2-r1
darkhttpd-openrc-1.14-r0
perl-devel-globaldestruction-doc-0.14-r2
perl-extutils-installpaths-doc-0.012-r2
lua-mosquitto-0.4.1-r1
lua-file-magic-0.2-r3
postfix-redis-3.7.3-r1
perl-gd-doc-2.76-r0
gettext-tiny-0.3.2-r3
sshguard-openrc-2.4.2-r1
perl-file-which-1.27-r0
bzip2-static-1.0.8-r4
mailcap-doc-2.1.53-r0
perl-json-doc-4.10-r0
lua5.3-md5-1.3-r1
nasm-doc-2.15.05-r1
libelf-static-0.187-r2
openldap-overlay-ppolicy-2.6.3-r6
rsyslog-mmanon-8.2212.0-r0
uwsgi-emperor_pg-2.0.21-r0
apache-mod-auth-ntlm-winbind-0.1-r7
ulogd-json-2.0.8-r0
python3-doc-3.10.9-r1
ngircd-doc-26.1-r3
open-isns-lib-0.102-r0
ruby-full-3.1.3-r0
avahi-ui-dev-0.8-r3
perl-class-accessor-0.51-r2
uwsgi-forkptyrouter-2.0.21-r0
clang14-static-14.0.6-r1
py3-six-1.16.0-r3
multipath-tools-openrc-0.9.3-r1
libsodium-static-1.0.18-r2
perl-text-wikiformat-doc-0.81-r2
krb5-pkinit-1.20.1-r0
ruby-test-unit-doc-3.5.5-r0
perl-text-password-pronounceable-doc-0.30-r3
nginx-mod-http-brotli-1.22.1-r0
lua5.2-evdev-2.2.1-r2
attr-static-2.5.1-r2
rhash-doc-1.4.3-r1
font-bh-75dpi-1.0.3-r2
linux-firmware-ti-keystone-20221214-r0
freeswitch-sounds-music-32000-1.0.8-r2
gpm-openrc-1.20.7-r2
lm-sensors-fancontrol-openrc-3.6.0-r4
perl-super-1.20190531-r1
brotli-static-1.0.9-r9
asterisk-cdr-mysql-18.15.0-r0
python3-tests-3.10.9-r1
alpine-ipxe-ipxe_efi-1.20.1-r1
lua5.1-sec-1.2.0-r1
gnokii-doc-0.6.31-r13
nagios-plugins-all-2.4.2-r0
links-doc-2.28-r0
privoxy-doc-3.0.33-r0
nginx-mod-http-js-1.22.1-r0
font-sony-misc-1.0.3-r3
perl-encode-3.19-r0
wireless-tools-doc-30_pre9-r2
mariadb-plugin-rocksdb-10.6.11-r0
kamailio-ldap-5.6.2-r1
dnsmasq-openrc-2.87-r0
cyrus-sasl-doc-2.1.28-r3
syslog-ng-amqp-3.38.1-r0
autoconf-doc-2.71-r1
lua5.3-5.3.6-r4
perl-x10-doc-0.04-r5
libxcb-static-1.15-r0
bluez-plugins-5.66-r1
uwsgi-rrdtool-2.0.21-r0
varnish-doc-7.2.1-r0
libuv-static-1.44.2-r0
ruby-debug-doc-1.6.3-r0
uwsgi-stats_pusher_file-2.0.21-r0
perl-http-daemon-6.14-r0
liblockfile-doc-1.17-r1
freeswitch-perlesl-1.10.8-r0
rng-tools-extra-openrc-6.15-r1
dhcrelay-openrc-4.4.3_p1-r1
dev86-doc-0.16.21-r2
flite-dev-2.2-r1
perl-proc-wait3-0.05-r7
f2fs-tools-static-1.15.0-r0
expect-doc-5.45.4-r3
perl-future-0.49-r0
perl-path-tiny-0.142-r0
lua5.2-yaml-1.1.2-r5
iftop-0.17-r9
openldap-back-ldap-2.6.3-r6
skalibs-static-2.12.0.1-r0
libpcrecpp-8.45-r2
font-liberation-2.1.5-r2
openldap-overlay-dds-2.6.3-r6
lua5.1-sql-postgres-2.6.0-r2
perl-metrics-any-0.08-r0
esh-0.3.2-r0
libgccjit-dev-12.2.1_git20220924-r4
unbound-dbg-1.17.0-r0
lxc-templates-oci-5.0.1-r1
iniparser-dev-4.1-r1
rtpproxy-openrc-2.1.1-r1
lua5.4-filesystem-1.8.0-r2
linux-firmware-cpia2-20221214-r0
nmap-doc-7.93-r0
bzip2-doc-1.0.8-r4
doas-sudo-shim-0.1.1-r0
perl-heap-doc-0.80-r4
squid-lang-sv-5.7-r0
dovecot-pop3d-2.3.20-r6
perl-mail-imapclient-3.43-r0
syslog-ng-graphite-3.38.1-r0
btrfs-progs-bash-completion-6.0.2-r0
perl-ipc-system-simple-doc-1.30-r2
iperf3-openrc-3.12-r0
fcgiwrap-doc-1.1.0-r6
alpine-make-rootfs-0.6.1-r0
lua5.1-posix-35.1-r0
rsyslog-rabbitmq-8.2212.0-r0
gawk-doc-5.1.1-r1
isl24-0.24-r1
perl-apache-session-1.94-r0
perl-file-copy-recursive-0.45-r2
perl-gd-2.76-r0
ncftp-dev-3.2.6-r5
xl2tpd-doc-1.3.18-r0
font-ibm-type1-1.0.3-r2
libsndfile-doc-1.1.0-r2
perl-dbd-odbc-doc-1.61-r3
lua-subprocess-0.0.20141229-r4
aom-dev-3.5.0-r0
perl-data-hexdump-doc-0.04-r0
dnstop-doc-20140915-r5
libev-dev-4.33-r0
docbook2x-doc-0.8.8-r8
strace-doc-6.0-r0
perl-html-parser-3.80-r0
perl-io-socket-inet6-2.73-r0
procps-doc-3.3.17-r2
lua5.1-evdev-2.2.1-r2
logrotate-doc-3.20.1-r3
liburing-2.3-r0
libconfig-doc-1.7.3-r0
acf-lib-lua5.1-0.11.0-r2
spawn-fcgi-doc-1.6.4-r5
libtool-doc-2.4.7-r0
font-bitstream-speedo-1.0.2-r2
lld-dbg-15.0.6-r0
libedit-doc-20221030.3.1-r0
kamailio-presence-5.6.2-r1
perl-term-table-doc-0.016-r0
parted-doc-3.5-r0
tmux-zsh-completion-5.9-r0
oidentd-doc-2.5.0-r1
nagios-plugins-ntp-2.4.2-r0
linux-lts-doc-5.15.85-r0
xz-static-5.2.9-r0
openldap-back-sql-2.6.3-r6
perl-class-returnvalue-0.55-r3
pingu-doc-1.5-r3
lua5.3-filesystem-1.8.0-r2
cups-doc-2.4.2-r1
libdbi-0.9.0-r2
perl-data-optlist-0.112-r0
acf-alpine-baselayout-0.13.3-r2
dmraid-doc-1.0.0_rc16-r1
m4-doc-1.4.19-r1
kamailio-sqlite-5.6.2-r1
perl-apache-logformat-compiler-doc-0.36-r1
nginx-mod-http-array-var-1.22.1-r0
perl-html-scrubber-0.19-r1
uwsgi-geoip-2.0.21-r0
nginx-mod-http-encrypted-session-1.22.1-r0
linux-firmware-brcm-20221214-r0
perl-xml-sax-base-1.09-r3
lua-alt-getopt-0.8.0-r0
jbig2dec-doc-0.19-r1
perl-sub-install-0.928-r2
lua-sql-mysql-2.6.0-r2
py3-sphinxcontrib-serializinghtml-1.1.5-r2
samurai-doc-1.2-r2
perl-posix-strftime-compiler-doc-0.44-r0
perl-namespace-autoclean-doc-0.29-r2
irssi-perl-1.4.3-r0
rdnssd-openrc-1.0.6-r0
at-spi2-core-lang-2.46.0-r0
tcl-doc-8.6.12-r1
gnu-efi-3.0.15-r0
uwsgi-logpipe-2.0.21-r0
cairo-static-1.17.6-r3
linux-firmware-rtl_nic-20221214-r0
gpm-doc-1.20.7-r2
iucode-tool-doc-2.3.1-r1
perl-cgi-fast-doc-2.16-r0
linux-firmware-other-20221214-r0
openldap-overlay-dynlist-2.6.3-r6
lua5.4-pty-1.2.2-r1
nagios-plugins-2.4.2-r0
libasr-dbg-1.0.4-r3
gnupg-scdaemon-2.2.40-r0
ccache-doc-4.7.4-r0
uwsgi-logzmq-2.0.21-r0
lua5.3-ldap-1.2.6-r2
lua5.3-filesize-0.1.1-r2
perl-file-listing-doc-6.15-r0
perl-scope-guard-0.21-r2
perl-io-captureoutput-1.1105-r1
perl-test-mockrandom-1.01-r3
libxshmfence-dev-1.3.1-r0
font-adobe-utopia-100dpi-1.0.4-r2
perl-sub-info-doc-0.002-r2
font-adobe-75dpi-1.0.3-r2
perl-unix-syslog-1.1-r17
openssh-server-9.1_p1-r1
linux-firmware-cypress-20221214-r0
perl-stream-buffered-doc-0.03-r2
perl-moo-2.005004-r1
perl-convert-asn1-doc-0.33-r0
perl-io-html-doc-1.004-r0
linux-pam-dev-1.5.2-r1
nagios-plugins-dbi-2.4.2-r0
hostapd-doc-2.10-r5
perl-class-load-doc-0.25-r2
confuse-doc-3.3-r1
uwsgi-router_redirect-2.0.21-r0
uwsgi-zabbix-2.0.21-r0
syslog-ng-stomp-3.38.1-r0
nmap-nselibs-7.93-r0
file-doc-5.43-r0
ldapvi-doc-1.7-r13
kbd-doc-2.5.1-r3
ack-doc-3.6.0-r0
lua5.2-pty-1.2.2-r1
perl-dbix-searchbuilder-doc-1.71-r0
acl-static-2.3.1-r1
db-doc-5.3.28-r2
dialog-static-1.3.20220728-r0
perl-gdgraph-1.54-r3
perl-javascript-minifier-doc-1.16-r0
s6-rc-dev-0.5.3.2-r0
uwsgi-tuntap-2.0.21-r0
nagios-plugins-pgsql-2.4.2-r0
checkbashisms-bash-completion-2.22.2-r0
iftop-doc-0.17-r9
uwsgi-syslog-2.0.21-r0
perl-file-temp-doc-0.2311-r0
libmaxminddb-1.7.1-r0
openssl-3.0.7-r0
linux-firmware-tehuti-20221214-r0
openssh-9.1_p1-r1
ruby-net-imap-doc-0.2.3-r0
perl-unix-syslog-doc-1.1-r17
acf-dnscache-0.6.0-r4
hylafaxplus-dbg-7.0.6-r1
libgnat-static-12.2.1_git20220924-r4
sqlite-tcl-doc-3.40.0-r0
u-boot-2022.10-r0
fcgiwrap-openrc-1.1.0-r6
lua5.4-md5-1.3-r1
freeswitch-static-1.10.8-r0
net-snmp-dbg-5.9.3-r1
perl-path-class-0.37-r2
lxc-doc-5.0.1-r1
spirv-tools-dbg-1.3.231.1-r0
ivykis-static-0.42.4-r2
perl-file-next-doc-1.18-r2
perl-net-smtp-tls-butmaintained-0.24-r2
aconf-doc-0.8.1-r2
fish-doc-3.5.1-r1
speex-dev-1.2.1-r0
perl-sys-mmap-0.20-r3
perl-carp-doc-1.50-r1
perl-yaml-syck-1.34-r2
perl-test-nowarnings-doc-1.06-r0
perl-test-script-1.29-r0
libsamplerate-static-0.2.2-r0
gc-doc-8.2.2-r0
perl-sub-exporter-progressive-doc-0.001013-r2
zsh-doc-5.9-r0
perl-control-x10-2.09-r4
lua5.2-sql-sqlite3-2.6.0-r2
perl-io-html-1.004-r0
iperf3-dev-3.12-r0
libnfsidmap-ldap-2.6.2-r0
libxxf86vm-1.1.5-r0
hicolor-icon-theme-0.17-r2
mesa-22.2.5-r0
libcap-static-2.66-r0
perl-locale-maketext-fuzzy-doc-0.11-r3
nginx-mod-http-upstream-fair-1.22.1-r0
busybox-mdev-openrc-1.35.0-r29
mariadb-common-10.6.11-r0
linux-firmware-qlogic-20221214-r0
shorewall6-5.2.8-r0
perl-tree-dag_node-doc-1.32-r0
lua5.3-luaxml-2.0.0-r0
s6-linux-init-dev-1.0.8.0-r0
pstree-doc-2.40-r0
linux-firmware-tigon-20221214-r0
eudev-openrc-3.2.11-r4
ppp-radius-2.4.9-r5
nginx-mod-http-naxsi-1.22.1-r0
yash-doc-2.53-r2
asterisk-pgsql-18.15.0-r0
atf-doc-0.21-r4
acf-nsd-0.0.1-r2
lua5.4-lxc-3.0.2-r1
c-ares-doc-1.18.1-r1
perl-role-basic-doc-0.13-r2
nagios-plugins-ssh-2.4.2-r0
lua5.2-dbg-5.2.4-r10
rsyslog-mmdblookup-8.2212.0-r0
ruby-rexml-doc-3.2.5-r2
nginx-mod-http-vod-1.22.1-r0
musl-libintl-1.2.3-r4
libdbi-doc-0.9.0-r2
fakeroot-doc-1.29-r0
kamailio-perl-5.6.2-r1
xtables-addons-doc-3.22-r0
mqtt-exec-openrc-0.5-r0
perl-extutils-pkgconfig-doc-1.16-r3
rtapd-dbg-1.7-r7
perl-xml-sax-doc-1.02-r1
lynx-zsh-completion-5.9-r0
kamailio-tls-5.6.2-r1
perl-cache-simple-timedexpiry-doc-0.27-r3
dejagnu-dev-1.6.2-r1
lua-sql-2.6.0-r2
fish-tools-3.5.1-r1
kyua-dbg-0.13-r6
parallel-zsh-completion-20221022-r0
daq-static-2.0.7-r1
perl-params-util-doc-1.102-r2
gsm-doc-1.0.22-r1
rsyslog-pmaixforwardedfrom-8.2212.0-r0
kamailio-mqtt-5.6.2-r1
apache2-ldap-2.4.54-r1
conntrack-tools-openrc-1.4.7-r0
ssmtp-doc-2.64-r18
nagios-plugins-load-2.4.2-r0
lua5.2-cjson-2.1.0-r10
libxi-doc-1.8-r0
kamailio-carrierroute-5.6.2-r1
openssl-libs-static-3.0.7-r0
at-spi2-core-doc-2.46.0-r0
fontconfig-static-2.14.1-r0
jwhois-4.0-r6
py3-pretend-1.0.9-r3
perl-mime-lite-3.033-r0
libunistring-static-1.1-r0
perl-mail-spamassassin-3.4.6-r4
lua-resty-lrucache-0.13-r0
bsd-compat-headers-0.7.2-r3
lua5.3-dbi-sqlite3-0.7.2-r1
postgresql14-contrib-jit-14.6-r1
lua5.2-pc-1.0.0-r11
tdb-doc-1.4.7-r0
libunistring-doc-1.1-r0
perl-net-server-doc-2.010-r1
kamailio-extras-5.6.2-r1
s6-networking-doc-2.5.1.1-r0
lua5.4-struct-0.2-r3
libcap-2.66-r0
perl-list-moreutils-xs-0.430-r2
libfido2-doc-1.12.0-r0
freeswitch-dbg-1.10.8-r0
hunspell-pt-br-20131017-r1
uwsgi-cgi-2.0.21-r0
dtc-dev-1.6.1-r2
radvd-openrc-2.19-r2
git-subtree-doc-2.38.2-r0
awall-masquerade-1.12.1-r0
bash-doc-5.2.15-r0
libutempter-doc-1.2.1-r5
dropbear-ssh-2022.83-r0
mg-doc-20221112-r0
nagios-plugins-dummy-2.4.2-r0
pgpool-openrc-4.3.3-r0
font-misc-meltho-1.0.3-r2
mqtt-exec-dbg-0.5-r0
gsm-tools-1.0.22-r1
arpon-doc-3.0-r6
libsecret-lang-0.20.5-r0
gettext-dev-0.21.1-r1
unbound-openrc-1.17.0-r0
perl-data-guid-0.050-r0
lua5.3-optarg-0.3-r0
perl-class-mix-doc-0.006-r2
mesa-dbg-22.2.5-r0
xen-hypervisor-4.16.3-r0
gperf-doc-3.1-r3
openldap-overlay-sssvlv-2.6.3-r6
iproute2-bash-completion-6.0.0-r1
perl-scope-upper-doc-0.33-r1
strongswan-openrc-5.9.8-r1
nano-syntax-7.0-r0
perl-xml-parser-doc-2.46-r3
perl-datetime-timezone-2.56-r0
font-cursor-misc-1.0.3-r3
supervisor-openrc-4.2.4-r0
perl-module-implementation-0.09-r3
nginx-doc-1.22.1-r0
bubblewrap-0.7.0-r0
knot-mod-geoip-3.2.4-r0
dovecot-dev-2.3.20-r6
perl-crypt-x509-0.54-r0
perl-fcgi-doc-0.82-r1
perl-file-slurp-tiny-doc-0.004-r2
libpaper-doc-2.0.4-r0
openrc-bash-completion-0.45.2-r7
perl-inc-latest-0.500-r1
nss-pam-ldapd-openrc-0.9.11-r2
apr-util-dbd_sqlite3-1.6.1-r14
curl-zsh-completion-7.87.0-r0
alsa-lib-dbg-1.2.8-r0
perl-cache-cache-doc-1.08-r3
nginx-mod-stream-geoip-1.22.1-r0
nsd-openrc-4.6.1-r0
linux-firmware-advansys-20221214-r0
bats-doc-1.8.2-r0
squid-lang-id-5.7-r0
nagios-plugins-file_age-2.4.2-r0
uwsgi-logfile-2.0.21-r0
font-vollkorn-4.105-r1
dropbear-doc-2022.83-r0
lua5.4-stdlib-41.2.2-r1
libidn-doc-1.41-r0
openldap-overlay-syncprov-2.6.3-r6
secureboot-hook-0.1-r4
perl-text-reform-doc-1.20-r2
perl-test-needs-doc-0.002009-r0
libxinerama-doc-1.1.5-r0
lua-ldbus-20150430-r3
perl-gdtextutil-doc-0.86-r2
perl-parallel-forkmanager-2.02-r3
aconf-mod-dnsmasq-0.8.1-r2
json-c-doc-0.16-r0
tree-doc-2.0.4-r0
lua5.1-pc-1.0.0-r11
perl-namespace-clean-doc-0.27-r2
kbd-legacy-2.5.1-r3
aspell-uk-1.4.0-r2
docbook-xsl-1.79.2-r8
openssh-dbg-9.1_p1-r1
graphviz-graphs-7.0.5-r0
uwsgi-signal-2.0.21-r0
softhsm-doc-2.6.1-r3
perl-mime-types-2.22-r0
perl-net-http-doc-6.22-r0
shared-mime-info-lang-2.2-r2
localhost:~# apk search go
argon2-20190702-r2
perl-http-negotiate-doc-6.01-r3
py3-gobject3-3.42.2-r0
gcc-go-12.2.1_git20220924-r4
goaccess-lang-1.6.5-r0
pango-dev-1.50.12-r0
pango-1.50.12-r0
cairo-gobject-1.17.6-r3
goaccess-1.6.5-r0
goaccess-doc-1.6.5-r0
binutils-gold-2.39-r2
argon2-libs-20190702-r2
pango-doc-1.50.12-r0
harfbuzz-5.3.1-r1
libgo-12.2.1_git20220924-r4
graphviz-7.0.5-r0
freeswitch-sangoma-1.10.8-r0
s6-linux-init-1.0.8.0-r0
avahi-glib-0.8-r6
pango-tools-1.50.12-r0
libgomp-12.2.1_git20220924-r4
argon2-dev-20190702-r2
gst-plugins-base-1.20.5-r0
graphene-dev-1.10.8-r1
perl-http-negotiate-6.01-r3
gobject-introspection-dev-1.74.0-r1
linux-firmware-go7007-20221214-r0
rhash-1.4.3-r1
ruby-google-protobuf-3.21.9-r0
gobject-introspection-doc-1.74.0-r1
py3-gobject3-dev-3.42.2-r0
gobject-introspection-1.74.0-r1
argon2-static-20190702-r2
avahi-dev-0.8-r6
openldap-passwd-argon2-2.6.3-r6
gtk-doc-1.33.2-r3
cairo-dev-1.17.6-r3
glib-dev-2.74.4-r0
pango-dbg-1.50.12-r0
u-boot-tools-2022.10-r0
samba-libs-4.16.8-r0
re2c-3.0-r0
linux-firmware-tigon-20221214-r0
harfbuzz-dev-5.3.1-r1
glib-2.74.4-r0
localhost:~# ls
localhost:~# pwd
/root
localhost:~# ls
localhost:~# git clone https://gitee.com/keengo/kangle
Cloning into 'kangle'...
remote: Enumerating objects: 2556, done.
remote: Counting objects: 100% (2556/2556), done.
remote: Compressing objects: 100% (1880/1880), done.
remote: Total 2556 (delta 1562), reused 1270 (delta 618), pack-reused 0
Receiving objects: 100% (2556/2556), 3.62 MiB | 737.00 KiB/s, done.
Resolving deltas: 100% (1562/1562), done.
localhost:~# cd kangle/
localhost:~/kangle# ls
CMakeLists.txt  LICENSE         README.md       cmake           docs            etc             extworker       include         kasync          khttpd          module          src             test            third_party     webadmin
localhost:~/kangle# git submodule init
Submodule 'kasync' (https://gitee.com/keengo/kasync) registered for path 'kasync'
Submodule 'khttpd' (https://gitee.com/keengo/khttpd) registered for path 'khttpd'
localhost:~/kangle# git submodule update
Cloning into '/root/kangle/kasync'...
Cloning into '/root/kangle/khttpd'...
Submodule path 'kasync': checked out '0f4f7bb2514e01d29c99800fdc260f85144a1dcf'
Submodule path 'khttpd': checked out 'aa137175acbda345759ae41813c6df005d829bde'
localhost:~/kangle# cd bu
-ash: cd: can't cd to bu: No such file or directory
localhost:~/kangle# ls
CMakeLists.txt  LICENSE         README.md       cmake           docs            etc             extworker       include         kasync          khttpd          module          src             test            third_party     webadmin
localhost:~/kangle# mkdir build
localhost:~/kangle# cd build
localhost:~/kangle/build# cmake ..
-ash: cmake: not found
localhost:~/kangle/build# apk install cmake
ERROR: 'install' is not an apk command. See 'apk --help'.
localhost:~/kangle/build# apk search cmake
cmake-bash-completion-3.24.3-r0
cmake-doc-3.24.3-r0
imake-1.0.9-r0
cmake-3.24.3-r0
localhost:~/kangle/build# apk add cmake
(1/7) Installing libacl (2.3.1-r1)
(2/7) Installing libbz2 (1.0.8-r4)
(3/7) Installing lz4-libs (1.9.4-r1)
(4/7) Installing libarchive (3.6.1-r2)
(5/7) Installing rhash-libs (1.4.3-r1)
(6/7) Installing libuv (1.44.2-r0)
(7/7) Installing cmake (3.24.3-r0)
Executing busybox-1.35.0-r29.trigger
OK: 468 MiB in 89 packages
localhost:~/kangle/build# cmake ..
CMake Error: CMake was unable to find a build program corresponding to "Unix Makefiles".  CMAKE_MAKE_PROGRAM is not set.  You probably need to select a different build tool.
CMake Error: CMAKE_C_COMPILER not set, after EnableLanguage
CMake Error: CMAKE_CXX_COMPILER not set, after EnableLanguage
-- Configuring incomplete, errors occurred!
See also "/root/kangle/build/CMakeFiles/CMakeOutput.log".
localhost:~/kangle/build# make
-ash: make: not found
localhost:~/kangle/build# apk add make
(1/1) Installing make (4.3-r1)
Executing busybox-1.35.0-r29.trigger
OK: 468 MiB in 90 packages
localhost:~/kangle/build# make
make: *** No targets specified and no makefile found.  Stop.
localhost:~/kangle/build# cmake ..
-- The C compiler identification is GNU 12.2.1
-- The CXX compiler identification is GNU 12.2.1
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/bin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Check for working CXX compiler: /usr/bin/c++ - skipped
-- Detecting CXX compile features
-- Detecting CXX compile features - done
-- Could NOT find PCRE (missing: PCRE_LIBRARY PCRE_INCLUDE_DIR)
-- Looking for timegm
-- Looking for timegm - found
-- Looking for KSOCKET_SSL
-- Looking for KSOCKET_SSL - not found
-- system is Linux
-- OPENSSL_DIR is
-- Could NOT find OpenSSL, try to set the path to OpenSSL root folder in the system variable OPENSSL_ROOT_DIR (missing: OPENSSL_CRYPTO_LIBRARY OPENSSL_INCLUDE_DIR)
-- cann't find openssl
-- Looking for accept4
-- Looking for accept4 - found
-- Looking for memalign
-- Looking for memalign - found
-- Looking for setmntent
-- Looking for setmntent - found
-- /root/kangle/kasync/include
-- build as static libary
-- KHTTP_LIBS pthread
-- Could NOT find ZLIB (missing: ZLIB_LIBRARY ZLIB_INCLUDE_DIR)
CMake Error at CMakeLists.txt:101 (message):
  cann't found zlib


-- Configuring incomplete, errors occurred!
See also "/root/kangle/build/CMakeFiles/CMakeOutput.log".
See also "/root/kangle/build/CMakeFiles/CMakeError.log".
localhost:~/kangle/build# apk search zlib
perl-compress-raw-zlib-2.202-r0
dovecot-2.3.20-r6
lua5.1-lzlib-0.4.3-r1
lua5.3-lzlib-0.4.3-r1
zlib-dev-1.2.13-r0
lua5.4-lzlib-0.4.3-r1
zlib-1.2.13-r0
lua-lzlib-0.4.3-r1
zlib-static-1.2.13-r0
lua5.2-lzlib-0.4.3-r1
zlib-doc-1.2.13-r0
localhost:~/kangle/build# apk add zlib-dev
(1/2) Installing pkgconf (1.9.3-r0)
(2/2) Installing zlib-dev (1.2.13-r0)
Executing busybox-1.35.0-r29.trigger
OK: 469 MiB in 92 packages
localhost:~/kangle/build# cmake ..
-- Could NOT find PCRE (missing: PCRE_LIBRARY PCRE_INCLUDE_DIR)
-- system is Linux
-- OPENSSL_DIR is
-- Could NOT find OpenSSL, try to set the path to OpenSSL root folder in the system variable OPENSSL_ROOT_DIR (missing: OPENSSL_CRYPTO_LIBRARY OPENSSL_INCLUDE_DIR)
-- cann't find openssl
-- /root/kangle/kasync/include
-- build as static libary
-- KHTTP_LIBS pthread
-- Found ZLIB: /lib/libz.so (found version "1.2.13")
-- Could NOT find SQLite3 (missing: SQLite3_INCLUDE_DIR SQLite3_LIBRARY)
-- kangle version 3.6.0
-- Looking for 3 include files ksapi.h, ..., kforwin32.h
-- Looking for 3 include files ksapi.h, ..., kforwin32.h - found
-- Configuring done
-- Generating done
-- Build files have been written to: /root/kangle/build
localhost:~/kangle/build# make
[  0%] Building C object CMakeFiles/sqlite3.dir/third_party/sqlite/sqlite3.c.o
cc1: warning: command-line option '-Wno-invalid-offsetof' is valid for C++/ObjC++ but not for C
[  0%] Linking C static library libsqlite3.a
[  0%] Built target sqlite3
[  0%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KAutoBuffer.cpp.o
[  0%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KDechunkContext.cpp.o
In file included from /root/kangle/kasync/include/kfiber.h:7,
                 from /root/kangle/khttpd/src/KDechunkContext.cpp:3:
/root/kangle/kasync/include/kasync_file.h:7:10: fatal error: linux/aio_abi.h: No such file or directory
    7 | #include <linux/aio_abi.h>
      |          ^~~~~~~~~~~~~~~~~
compilation terminated.
make[2]: *** [khttpd/CMakeFiles/khttpd.dir/build.make:90: khttpd/CMakeFiles/khttpd.dir/src/KDechunkContext.cpp.o] Error 1
make[1]: *** [CMakeFiles/Makefile2:300: khttpd/CMakeFiles/khttpd.dir/all] Error 2
make: *** [Makefile:156: all] Error 2
localhost:~/kangle/build# apk search linux
syslinux-6.04_pre1-r11
linux-firmware-av7110-20221214-r0
gcc-12.2.1_git20220924-r4
linux-firmware-20221214-r0
linux-firmware-rtl8192e-20221214-r0
linux-firmware-ene-ub6250-20221214-r0
linux-firmware-edgeport-20221214-r0
linux-firmware-bnx2-20221214-r0
linux-firmware-mwlwifi-20221214-r0
linux-firmware-korg-20221214-r0
linux-firmware-amdgpu-20221214-r0
dahdi-linux-lts-dev-5.15.85-r1
linux-firmware-cadence-20221214-r0
font-linux-libertine-doc-5.3.0-r2
g++-12.2.1_git20220924-r4
linux-firmware-keyspan_pda-20221214-r0
linux-firmware-yamaha-20221214-r0
linux-firmware-qcom-20221214-r0
util-linux-misc-2.38.1-r0
gcc-gdc-12.2.1_git20220924-r4
gfortran-12.2.1_git20220924-r4
linux-lts-dev-5.15.85-r0
font-linux-libertine-5.3.0-r2
linux-lts-5.15.85-r0
linux-firmware-vxge-20221214-r0
linux-firmware-i915-20221214-r0
syslinux-doc-6.04_pre1-r11
linux-firmware-vicam-20221214-r0
devicemaster-linux-7.34-r2
s6-linux-init-static-1.0.8.0-r0
linux-firmware-cxgb3-20221214-r0
linux-firmware-ti-20221214-r0
linux-firmware-amd-ucode-20221214-r0
linux-firmware-matrox-20221214-r0
linux-firmware-adaptec-20221214-r0
linux-firmware-ath6k-20221214-r0
dahdi-linux-3.2.0-r0
samba-libs-py3-4.16.8-r0
linux-firmware-none-20221214-r0
s6-linux-init-1.0.8.0-r0
dahdi-linux-lts-5.15.85-r1
linux-firmware-nvidia-20221214-r0
linux-firmware-rtw89-20221214-r0
linux-firmware-mediatek-20221214-r0
linux-firmware-sun-20221214-r0
linux-firmware-emi62-20221214-r0
linux-headers-5.19.5-r0
linux-firmware-ess-20221214-r0
py3-ldb-2.5.2-r0
linux-firmware-rtlwifi-20221214-r0
linux-firmware-rtl_bt-20221214-r0
linux-firmware-isci-20221214-r0
util-linux-login-doc-2.38.1-r0
linux-firmware-cxgb4-20221214-r0
linux-firmware-r128-20221214-r0
linux-firmware-bnx2x-20221214-r0
linux-firmware-cirrus-20221214-r0
linux-firmware-slicoss-20221214-r0
linux-firmware-rtw88-20221214-r0
linux-firmware-go7007-20221214-r0
linux-firmware-mwl8k-20221214-r0
linux-firmware-cis-20221214-r0
linux-firmware-nxp-20221214-r0
linux-firmware-amd-20221214-r0
util-linux-2.38.1-r0
linux-virt-5.15.85-r0
linux-firmware-atmel-20221214-r0
linux-firmware-amphion-20221214-r0
linux-firmware-moxa-20221214-r0
gcc-go-12.2.1_git20220924-r4
linux-virt-dev-5.15.85-r0
util-linux-doc-2.38.1-r0
linux-firmware-cavium-20221214-r0
linux-firmware-rockchip-20221214-r0
linux-firmware-e100-20221214-r0
util-linux-openrc-2.38.1-r0
linux-firmware-sxg-20221214-r0
linux-firmware-meson-20221214-r0
linux-firmware-microchip-20221214-r0
linux-firmware-keyspan-20221214-r0
linux-firmware-liquidio-20221214-r0
linux-firmware-dsp56k-20221214-r0
s6-linux-init-doc-1.0.8.0-r0
devicemaster-linux-openrc-7.34-r2
linux-pam-doc-1.5.2-r1
linux-firmware-3com-20221214-r0
linux-firmware-wfx-20221214-r0
linux-firmware-netronome-20221214-r0
linux-firmware-atusb-20221214-r0
linux-firmware-sb16-20221214-r0
syslinux-dev-6.04_pre1-r11
devicemaster-linux-doc-7.34-r2
linux-firmware-kaweth-20221214-r0
linux-firmware-inside-secure-20221214-r0
linux-firmware-ueagle-atm-20221214-r0
linux-firmware-ositech-20221214-r0
linux-firmware-ath10k-20221214-r0
linux-firmware-ath11k-20221214-r0
linux-firmware-cnm-20221214-r0
s6-linux-utils-doc-2.6.0.0-r0
py3-talloc-2.3.4-r0
linux-firmware-radeon-20221214-r0
linux-firmware-intel-20221214-r0
linux-firmware-mrvl-20221214-r0
linux-firmware-libertas-20221214-r0
linux-firmware-qca-20221214-r0
linux-firmware-ttusb-budget-20221214-r0
hylafaxplus-7.0.6-r1
linux-firmware-dpaa2-20221214-r0
ldb-dev-2.5.2-r0
util-linux-bash-completion-2.38.1-r0
linux-firmware-yam-20221214-r0
linux-firmware-qed-20221214-r0
dahdi-linux-dev-3.2.0-r0
linux-firmware-ath9k_htc-20221214-r0
linux-firmware-rsi-20221214-r0
linux-firmware-ar3k-20221214-r0
linux-firmware-mellanox-20221214-r0
linux-firmware-emi26-20221214-r0
talloc-dev-2.3.4-r0
linux-firmware-myricom-20221214-r0
linux-firmware-acenic-20221214-r0
linux-firmware-imx-20221214-r0
linux-firmware-dabusb-20221214-r0
grub-2.06-r6
linux-firmware-ti-connectivity-20221214-r0
s6-linux-utils-2.6.0.0-r0
linux-firmware-ti-keystone-20221214-r0
samba-dc-libs-4.16.8-r0
samba-dev-4.16.8-r0
util-linux-dev-2.38.1-r0
linux-firmware-cpia2-20221214-r0
util-linux-login-2.38.1-r0
linux-lts-doc-5.15.85-r0
linux-firmware-brcm-20221214-r0
linux-firmware-rtl_nic-20221214-r0
linux-firmware-other-20221214-r0
linux-pam-1.5.2-r1
linux-firmware-cypress-20221214-r0
linux-pam-dev-1.5.2-r1
linux-firmware-tehuti-20221214-r0
linux-firmware-qlogic-20221214-r0
s6-linux-init-dev-1.0.8.0-r0
linux-firmware-tigon-20221214-r0
linux-firmware-advansys-20221214-r0
localhost:~/kangle/build# apk search kernel|grep include
localhost:~/kangle/build# apk search kernel
kernel-hooks-0.2-r0
installkernel-3.5-r2
alpine-conf-3.15.0-r0
strongswan-5.9.8-r1
byobu-5.133-r2
localhost:~/kangle/build# apk help
ERROR: 'help' is not an apk command. See 'apk --help'.
localhost:~/kangle/build# apk --help
apk-tools 2.12.10, compiled for x86_64.

usage: apk [<OPTIONS>...] COMMAND [<ARGUMENTS>...]

Package installation and removal:
  add        Add packages to WORLD and commit changes
  del        Remove packages from WORLD and commit changes

System maintenance:
  fix        Fix, reinstall or upgrade packages without modifying WORLD
  update     Update repository indexes
  upgrade    Install upgrades available from repositories
  cache      Manage the local package cache

Querying package information:
  info       Give detailed information about packages or repositories
  list       List packages matching a pattern or other criteria
  dot        Render dependencies as graphviz graphs
  policy     Show repository policy for packages
  search     Search for packages by name or description

Repository maintenance:
  index      Create repository index file from packages
  fetch      Download packages from global repositories to a local directory
  manifest   Show checksums of package contents
  verify     Verify package integrity and signature

Miscellaneous:
  audit      Audit system for changes
  stats      Show statistics about repositories and installations
  version    Compare package versions or perform tests on version strings

This apk has coffee making abilities.
For more information: man 8 apk
localhost:~/kangle/build# apk search linux/aio_abi.h
localhost:~/kangle/build# apk search aio_abi.h
localhost:~/kangle/build# ifconfig
eth0      Link encap:Ethernet  HWaddr 00:0C:29:A2:CB:41
          inet addr:192.168.0.102  Bcast:0.0.0.0  Mask:255.255.255.0
          inet6 addr: fe80::20c:29ff:fea2:cb41/64 Scope:Link
          UP BROADCAST RUNNING MULTICAST  MTU:1500  Metric:1
          RX packets:300475 errors:0 dropped:0 overruns:0 frame:0
          TX packets:121908 errors:0 dropped:0 overruns:0 carrier:0
          collisions:0 txqueuelen:1000
          RX bytes:126186554 (120.3 MiB)  TX bytes:7875358 (7.5 MiB)

lo        Link encap:Local Loopback
          inet addr:127.0.0.1  Mask:255.0.0.0
          inet6 addr: ::1/128 Scope:Host
          UP LOOPBACK RUNNING  MTU:65536  Metric:1
          RX packets:0 errors:0 dropped:0 overruns:0 frame:0
          TX packets:0 errors:0 dropped:0 overruns:0 carrier:0
          collisions:0 txqueuelen:1000
          RX bytes:0 (0.0 B)  TX bytes:0 (0.0 B)

localhost:~/kangle/build# uname -a
Linux localhost 5.15.85-0-lts #1-Alpine SMP Thu, 22 Dec 2022 08:57:12 +0000 x86_64 Linux
localhost:~/kangle/build# cd /usr/include/
localhost:/usr/include# ls
aio.h          byteswap.h     dirent.h       fcntl.h        ftw.h          ifaddrs.h      limits.h       mntent.h       netpacket      pwd.h          semaphore.h    stdarg.h       stdio_ext.h    sys            tgmath.h       unistd.h       wchar.h
alloca.h       c++            dlfcn.h        features.h     getopt.h       inttypes.h     link.h         monetary.h     nl_types.h     regex.h        setjmp.h       stdbool.h      stdlib.h       syscall.h      threads.h      utime.h        wctype.h
ar.h           complex.h      elf.h          fenv.h         glob.h         iso646.h       locale.h       mqueue.h       paths.h        resolv.h       shadow.h       stdc-predef.h  stdnoreturn.h  sysexits.h     time.h         utmp.h         wordexp.h
arpa           cpio.h         endian.h       float.h        gnumake.h      langinfo.h     malloc.h       net            poll.h         sched.h        signal.h       stddef.h       string.h       syslog.h       uchar.h        utmpx.h        zconf.h
assert.h       crypt.h        err.h          fmtmsg.h       grp.h          lastlog.h      math.h         netdb.h        pthread.h      scsi           spawn.h        stdint.h       strings.h      tar.h          ucontext.h     values.h       zlib.h
bits           ctype.h        errno.h        fnmatch.h      iconv.h        libgen.h       memory.h       netinet        pty.h          search.h       stdalign.h     stdio.h        stropts.h      termios.h      ulimit.h       wait.h
localhost:/usr/include# apk search headers
spirv-headers-1.3.231.1-r0
vulkan-headers-1.3.231.1-r0
linux-headers-5.19.5-r0
fortify-headers-1.1-r1
nginx-mod-http-headers-more-1.22.1-r0
bsd-compat-headers-0.7.2-r3
localhost:/usr/include# apk add linux-headers
(1/1) Installing linux-headers (5.19.5-r0)
OK: 476 MiB in 93 packages
localhost:/usr/include# cd /root/kangle/build/
localhost:~/kangle/build# make
Consolidate compiler generated dependencies of target sqlite3
[  0%] Built target sqlite3
Consolidate compiler generated dependencies of target khttpd
[  0%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KDechunkContext.cpp.o
[  0%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KDechunkEngine.cpp.o
[  1%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KHttp2.cpp.o
[  1%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KHttp2Table.cpp.o
[  1%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KHttp2Upstream.cpp.o
[  1%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KHttp2WriteBuffer.cpp.o
[  2%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KHttp3.cpp.o
[  2%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KHttp3Connection.cpp.o
[  2%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KHttp3Sink.cpp.o
[  2%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KHttpKeyValue.cpp.o
[  3%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KHttpLib.cpp.o
[  3%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KHttpParser.cpp.o
[  3%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KHttpServer.cpp.o
[  3%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KHttpSink.cpp.o
[  4%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KHttpUpstream.cpp.o
[  4%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KHuffDecode.cpp.o
[  4%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KHuffEncode.cpp.o
[  5%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KPoolableSocketContainer.cpp.o
[  5%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KPreRequest.cpp.o
[  5%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KProxy.cpp.o
[  5%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KRequest.cpp.o
[  6%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KSink.cpp.o
In file included from /root/kangle/kasync/include/klist.h:4,
                 from /root/kangle/kasync/include/kselector.h:6,
                 from /root/kangle/khttpd/include/KSink.h:3,
                 from /root/kangle/khttpd/src/KSink.cpp:1:
/root/kangle/khttpd/src/KSink.cpp: In function 'kev_result ksink_iterator(KOPAQUE, void*, int)':
/root/kangle/khttpd/src/KSink.cpp:32:58: warning: 'offsetof' within non-standard-layout type 'KSink' is conditionally-supported [-Winvalid-offsetof]
   32 |                 KSink* sink = (KSink*)kgl_list_data(pos, KSink, queue);
/root/kangle/khttpd/src/KSink.cpp:32:39: note: in expansion of macro 'kgl_list_data'
   32 |                 KSink* sink = (KSink*)kgl_list_data(pos, KSink, queue);
      |                                       ^~~~~~~~~~~~~
[  6%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KSockPoolHelper.cpp.o
[  6%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KStream.cpp.o
[  6%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KStringBuf.cpp.o
[  7%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KTcpSink.cpp.o
[  7%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KTcpUpstream.cpp.o
[  7%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KTsUpstream.cpp.o
[  7%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KUpstream.cpp.o
[  8%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kaddr.c.o
[  8%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kasync_file.c.o
[  8%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kasync_worker.c.o
[  9%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kbuf.c.o
[  9%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kconnection.c.o
[  9%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kepoll_selector.c.o
[  9%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kfiber.c.o
[ 10%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kfiber_sync.c.o
[ 10%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kfile.c.o
[ 10%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kgl_ssl.c.o
[ 10%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kiocp_selector.c.o
[ 11%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kiouring_selector.c.o
[ 11%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kkqueue_selector.c.o
[ 11%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/klib.c.o
[ 11%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/klog.c.o
[ 13%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kmalloc.c.o
[ 13%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kmalloc_debug.c.o
[ 13%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kmalloc_debug_win32.c.o
[ 14%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/krbtree.c.o
[ 14%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kselectable.c.o
[ 14%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kselector.c.o
[ 14%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kselector_manager.c.o
[ 15%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kserver.c.o
[ 15%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/ksocket.c.o
[ 15%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kssl_bio.c.o
[ 15%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kthread.c.o
[ 16%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kudp.c.o
[ 16%] Linking CXX static library libkhttpd.a
[ 16%] Built target khttpd
[ 16%] Building CXX object CMakeFiles/kangle.dir/module/whm/KWhmService.cpp.o
In file included from /root/kangle/include/KPathRedirect.h:17,
                 from /root/kangle/include/KFetchObject.h:27,
                 from /root/kangle/include/KAcserver.h:25,
                 from /root/kangle/include/KHttpRequest.h:27,
                 from /root/kangle/include/KSubVirtualHost.h:12,
                 from /root/kangle/include/KVirtualHost.h:38,
                 from /root/kangle/module/whm/WhmContext.h:15,
                 from /root/kangle/module/whm/KWhmService.cpp:9:
/root/kangle/include/KReg.h:25:10: fatal error: pcre.h: No such file or directory
   25 | #include <pcre.h>
      |          ^~~~~~~~
compilation terminated.
make[2]: *** [CMakeFiles/kangle.dir/build.make:76: CMakeFiles/kangle.dir/module/whm/KWhmService.cpp.o] Error 1
make[1]: *** [CMakeFiles/Makefile2:195: CMakeFiles/kangle.dir/all] Error 2
make: *** [Makefile:156: all] Error 2
localhost:~/kangle/build# apk search pcre
libpcre2-32-10.42-r0
pcre-dev-8.45-r2
libpcre2-16-10.42-r0
lua5.4-rex-pcre-2.9.1-r1
lua-rex-pcre-2.9.1-r1
pcre-tools-8.45-r2
pcre-8.45-r2
pcre2-dev-10.42-r0
postfix-pcre-3.7.3-r1
pcre2-doc-10.42-r0
lua5.1-rex-pcre-2.9.1-r1
libpcre16-8.45-r2
zsh-pcre-5.9-r0
libpcre32-8.45-r2
pcre2-10.42-r0
pcre2-tools-10.42-r0
lua5.3-rex-pcre-2.9.1-r1
lua5.2-rex-pcre-2.9.1-r1
pcre-doc-8.45-r2
libpcrecpp-8.45-r2
localhost:~/kangle/build# apk install pcre-dev
ERROR: 'install' is not an apk command. See 'apk --help'.
localhost:~/kangle/build# apk add pcre-dev
(1/5) Installing libpcre16 (8.45-r2)
(2/5) Installing libpcre32 (8.45-r2)
(3/5) Installing pcre (8.45-r2)
(4/5) Installing libpcrecpp (8.45-r2)
(5/5) Installing pcre-dev (8.45-r2)
Executing busybox-1.35.0-r29.trigger
OK: 479 MiB in 98 packages
localhost:~/kangle/build# make
[  0%] Built target sqlite3
Consolidate compiler generated dependencies of target khttpd
[ 16%] Built target khttpd
[ 16%] Building CXX object CMakeFiles/kangle.dir/module/whm/KWhmService.cpp.o
[ 17%] Building CXX object CMakeFiles/kangle.dir/module/whm/WhmCallMap.cpp.o
[ 17%] Building CXX object CMakeFiles/kangle.dir/module/whm/WhmCommand.cpp.o
[ 17%] Building CXX object CMakeFiles/kangle.dir/module/whm/WhmContext.cpp.o
[ 18%] Building CXX object CMakeFiles/kangle.dir/module/whm/WhmDso.cpp.o
[ 18%] Building CXX object CMakeFiles/kangle.dir/module/whm/WhmExtend.cpp.o
[ 18%] Building CXX object CMakeFiles/kangle.dir/module/whm/WhmLog.cpp.o
[ 18%] Building CXX object CMakeFiles/kangle.dir/module/whm/WhmPackage.cpp.o
[ 19%] Building CXX object CMakeFiles/kangle.dir/module/whm/WhmPackageManage.cpp.o
[ 19%] Building CXX object CMakeFiles/kangle.dir/module/whm/WhmShell.cpp.o
[ 19%] Building CXX object CMakeFiles/kangle.dir/module/whm/WhmShellProcess.cpp.o
[ 19%] Building CXX object CMakeFiles/kangle.dir/module/whm/WhmShellSession.cpp.o
[ 20%] Building CXX object CMakeFiles/kangle.dir/module/whm/WhmUrl.cpp.o
[ 20%] Building CXX object CMakeFiles/kangle.dir/module/whm/dllmain.cpp.o
[ 20%] Building CXX object CMakeFiles/kangle.dir/module/whm/stdafx.cpp.o
[ 20%] Building CXX object CMakeFiles/kangle.dir/module/whm/whm.cpp.o
[ 21%] Building CXX object CMakeFiles/kangle.dir/module/whm/whmdso/core/core.cpp.o
[ 21%] Building CXX object CMakeFiles/kangle.dir/src/EdcodeUtils.cpp.o
[ 21%] Building CXX object CMakeFiles/kangle.dir/src/HttpCore.cpp.o
[ 21%] Building CXX object CMakeFiles/kangle.dir/src/HttpFiber.cpp.o
[ 22%] Building CXX object CMakeFiles/kangle.dir/src/KAccess.cpp.o
[ 22%] Building CXX object CMakeFiles/kangle.dir/src/KAccessDso.cpp.o
[ 22%] Building CXX object CMakeFiles/kangle.dir/src/KAccessDsoSupport.cpp.o
[ 23%] Building CXX object CMakeFiles/kangle.dir/src/KAccessParser.cpp.o
[ 23%] Building CXX object CMakeFiles/kangle.dir/src/KAcserver.cpp.o
[ 23%] Building CXX object CMakeFiles/kangle.dir/src/KAcserverManager.cpp.o
[ 23%] Building CXX object CMakeFiles/kangle.dir/src/KAjpFetchObject.cpp.o
[ 25%] Building CXX object CMakeFiles/kangle.dir/src/KAjpMessage.cpp.o
[ 25%] Building CXX object CMakeFiles/kangle.dir/src/KApacheVirtualHost.cpp.o
[ 25%] Building CXX object CMakeFiles/kangle.dir/src/KApiDso.cpp.o
[ 25%] Building CXX object CMakeFiles/kangle.dir/src/KApiEnv.cpp.o
[ 26%] Building CXX object CMakeFiles/kangle.dir/src/KApiFastcgiFetchObject.cpp.o
[ 26%] Building CXX object CMakeFiles/kangle.dir/src/KApiFetchObject.cpp.o
[ 26%] Building CXX object CMakeFiles/kangle.dir/src/KApiPipeStream.cpp.o
[ 26%] Building CXX object CMakeFiles/kangle.dir/src/KApiProcess.cpp.o
[ 27%] Building CXX object CMakeFiles/kangle.dir/src/KApiRedirect.cpp.o
[ 27%] Building CXX object CMakeFiles/kangle.dir/src/KAsyncFetchObject.cpp.o
[ 27%] Building CXX object CMakeFiles/kangle.dir/src/KAsyncFileApi.cpp.o
[ 28%] Building CXX object CMakeFiles/kangle.dir/src/KAsyncWorker.cpp.o
[ 28%] Building CXX object CMakeFiles/kangle.dir/src/KAuthMark.cpp.o
[ 28%] Building CXX object CMakeFiles/kangle.dir/src/KBaseVirtualHost.cpp.o
[ 28%] Building CXX object CMakeFiles/kangle.dir/src/KBigObject.cpp.o
[ 29%] Building CXX object CMakeFiles/kangle.dir/src/KBigObjectContext.cpp.o
[ 29%] Building CXX object CMakeFiles/kangle.dir/src/KBigObjectManager.cpp.o
[ 29%] Building CXX object CMakeFiles/kangle.dir/src/KBigObjectStream.cpp.o
[ 29%] Building CXX object CMakeFiles/kangle.dir/src/KBrotli.cpp.o
[ 30%] Building CXX object CMakeFiles/kangle.dir/src/KBuffer.cpp.o
[ 30%] Building CXX object CMakeFiles/kangle.dir/src/KBufferFetchObject.cpp.o
[ 30%] Building CXX object CMakeFiles/kangle.dir/src/KBugReport.cpp.o
[ 30%] Building CXX object CMakeFiles/kangle.dir/src/KCache.cpp.o
[ 31%] Building CXX object CMakeFiles/kangle.dir/src/KCacheStream.cpp.o
[ 31%] Building CXX object CMakeFiles/kangle.dir/src/KCdnContainer.cpp.o
[ 31%] Building CXX object CMakeFiles/kangle.dir/src/KCdnRedirect.cpp.o
[ 32%] Building CXX object CMakeFiles/kangle.dir/src/KCdnRewriteMark.cpp.o
[ 32%] Building CXX object CMakeFiles/kangle.dir/src/KCgiEnv.cpp.o
[ 32%] Building CXX object CMakeFiles/kangle.dir/src/KChain.cpp.o
[ 32%] Building CXX object CMakeFiles/kangle.dir/src/KChunked.cpp.o
[ 33%] Building CXX object CMakeFiles/kangle.dir/src/KCloudIpAcl.cpp.o
[ 33%] Building CXX object CMakeFiles/kangle.dir/src/KCmdPoolableRedirect.cpp.o
[ 33%] Building CXX object CMakeFiles/kangle.dir/src/KCmdProcess.cpp.o
[ 33%] Building CXX object CMakeFiles/kangle.dir/src/KCompressStream.cpp.o
[ 34%] Building CXX object CMakeFiles/kangle.dir/src/KConfig.cpp.o
[ 34%] Building CXX object CMakeFiles/kangle.dir/src/KConfigBuilder.cpp.o
[ 34%] Building CXX object CMakeFiles/kangle.dir/src/KConfigParser.cpp.o
[ 34%] Building CXX object CMakeFiles/kangle.dir/src/KConnectionLimit.cpp.o
[ 35%] Building CXX object CMakeFiles/kangle.dir/src/KContentMark.cpp.o
[ 35%] Building CXX object CMakeFiles/kangle.dir/src/KContentTransfer.cpp.o
[ 35%] Building CXX object CMakeFiles/kangle.dir/src/KContentType.cpp.o
[ 35%] Building CXX object CMakeFiles/kangle.dir/src/KContext.cpp.o
[ 36%] Building CXX object CMakeFiles/kangle.dir/src/KDefaultFetchObject.cpp.o
[ 36%] Building CXX object CMakeFiles/kangle.dir/src/KDirectoryFetchObject.cpp.o
[ 36%] Building CXX object CMakeFiles/kangle.dir/src/KDiskCache.cpp.o
[ 38%] Building CXX object CMakeFiles/kangle.dir/src/KDiskCacheIndex.cpp.o
[ 38%] Building CXX object CMakeFiles/kangle.dir/src/KDiskCacheStream.cpp.o
[ 38%] Building CXX object CMakeFiles/kangle.dir/src/KDomainUser.cpp.o
[ 38%] Building CXX object CMakeFiles/kangle.dir/src/KDsoAsyncFetchObject.cpp.o
[ 39%] Building CXX object CMakeFiles/kangle.dir/src/KDsoConfigParser.cpp.o
[ 39%] Building CXX object CMakeFiles/kangle.dir/src/KDsoExtend.cpp.o
[ 39%] Building CXX object CMakeFiles/kangle.dir/src/KDsoExtendManage.cpp.o
[ 39%] Building CXX object CMakeFiles/kangle.dir/src/KDsoFilter.cpp.o
[ 40%] Building CXX object CMakeFiles/kangle.dir/src/KDsoModule.cpp.o
[ 40%] Building CXX object CMakeFiles/kangle.dir/src/KDsoNextUpstream.cpp.o
[ 40%] Building CXX object CMakeFiles/kangle.dir/src/KDsoRedirect.cpp.o
[ 40%] Building CXX object CMakeFiles/kangle.dir/src/KDynamicListen.cpp.o
[ 41%] Building CXX object CMakeFiles/kangle.dir/src/KDynamicString.cpp.o
[ 41%] Building CXX object CMakeFiles/kangle.dir/src/KEnvInterface.cpp.o
[ 41%] Building CXX object CMakeFiles/kangle.dir/src/KExtendProgram.cpp.o
[ 42%] Building CXX object CMakeFiles/kangle.dir/src/KFastcgiFetchObject.cpp.o
[ 42%] Building CXX object CMakeFiles/kangle.dir/src/KFastcgiUtils.cpp.o
[ 42%] Building CXX object CMakeFiles/kangle.dir/src/KFdStream.cpp.o
[ 42%] Building CXX object CMakeFiles/kangle.dir/src/KFetchBigObject.cpp.o
[ 43%] Building CXX object CMakeFiles/kangle.dir/src/KFetchObject.cpp.o
[ 43%] Building CXX object CMakeFiles/kangle.dir/src/KFileMsg.cpp.o
[ 43%] Building CXX object CMakeFiles/kangle.dir/src/KFileName.cpp.o
[ 43%] Building CXX object CMakeFiles/kangle.dir/src/KFixHeaderMark.cpp.o
[ 44%] Building CXX object CMakeFiles/kangle.dir/src/KFormInputFilter.cpp.o
[ 44%] Building CXX object CMakeFiles/kangle.dir/src/KGeoMark.cpp.o
[ 44%] Building CXX object CMakeFiles/kangle.dir/src/KGzip.cpp.o
[ 44%] Building CXX object CMakeFiles/kangle.dir/src/KHtAccess.cpp.o
[ 45%] Building CXX object CMakeFiles/kangle.dir/src/KHtModule.cpp.o
[ 45%] Building CXX object CMakeFiles/kangle.dir/src/KHtRewriteModule.cpp.o
[ 45%] Building CXX object CMakeFiles/kangle.dir/src/KHtmlSupport.cpp.o
[ 46%] Building CXX object CMakeFiles/kangle.dir/src/KHtmlSupportException.cpp.o
[ 46%] Building CXX object CMakeFiles/kangle.dir/src/KHtmlTagFilter.cpp.o
[ 46%] Building CXX object CMakeFiles/kangle.dir/src/KHttpAuth.cpp.o
[ 46%] Building CXX object CMakeFiles/kangle.dir/src/KHttpBasicAuth.cpp.o
[ 47%] Building CXX object CMakeFiles/kangle.dir/src/KHttpClient.cpp.o
[ 47%] Building CXX object CMakeFiles/kangle.dir/src/KHttpDigestAuth.cpp.o
[ 47%] Building CXX object CMakeFiles/kangle.dir/src/KHttpExt.cpp.o
[ 47%] Building CXX object CMakeFiles/kangle.dir/src/KHttpField.cpp.o
[ 48%] Building CXX object CMakeFiles/kangle.dir/src/KHttpFilterContext.cpp.o
[ 48%] Building CXX object CMakeFiles/kangle.dir/src/KHttpFilterDso.cpp.o
[ 48%] Building CXX object CMakeFiles/kangle.dir/src/KHttpFilterDsoManage.cpp.o
[ 48%] Building CXX object CMakeFiles/kangle.dir/src/KHttpFilterHook.cpp.o
[ 50%] Building CXX object CMakeFiles/kangle.dir/src/KHttpFilterHookCollectRequest.cpp.o
[ 50%] Building CXX object CMakeFiles/kangle.dir/src/KHttpFilterHookCollectResponse.cpp.o
[ 50%] Building CXX object CMakeFiles/kangle.dir/src/KHttpFilterManage.cpp.o
[ 50%] Building CXX object CMakeFiles/kangle.dir/src/KHttpFilterStream.cpp.o
[ 51%] Building CXX object CMakeFiles/kangle.dir/src/KHttpManage.cpp.o
[ 51%] Building CXX object CMakeFiles/kangle.dir/src/KHttpObject.cpp.o
[ 51%] Building CXX object CMakeFiles/kangle.dir/src/KHttpObjectApi.cpp.o
[ 52%] Building CXX object CMakeFiles/kangle.dir/src/KHttpObjectHash.cpp.o
[ 52%] Building CXX object CMakeFiles/kangle.dir/src/KHttpObjectSwaping.cpp.o
[ 52%] Building CXX object CMakeFiles/kangle.dir/src/KHttpPost.cpp.o
[ 52%] Building CXX object CMakeFiles/kangle.dir/src/KHttpProxyFetchObject.cpp.o
[ 53%] Building CXX object CMakeFiles/kangle.dir/src/KHttpRequest.cpp.o
[ 53%] Building CXX object CMakeFiles/kangle.dir/src/KHttpResponseParser.cpp.o
[ 53%] Building CXX object CMakeFiles/kangle.dir/src/KHttpServerParser.cpp.o
[ 53%] Building CXX object CMakeFiles/kangle.dir/src/KHttpTransfer.cpp.o
[ 54%] Building CXX object CMakeFiles/kangle.dir/src/KISAPIServiceProvider.cpp.o
[ 54%] Building CXX object CMakeFiles/kangle.dir/src/KInputFilter.cpp.o
[ 54%] Building CXX object CMakeFiles/kangle.dir/src/KIpList.cpp.o
[ 54%] Building CXX object CMakeFiles/kangle.dir/src/KIpMap.cpp.o
[ 55%] Building CXX object CMakeFiles/kangle.dir/src/KIpSpeedLimitMark.cpp.o
[ 55%] Building CXX object CMakeFiles/kangle.dir/src/KLang.cpp.o
[ 55%] Building CXX object CMakeFiles/kangle.dir/src/KLangParser.cpp.o
[ 56%] Building CXX object CMakeFiles/kangle.dir/src/KLineFile.cpp.o
[ 56%] Building CXX object CMakeFiles/kangle.dir/src/KListNode.cpp.o
[ 56%] Building CXX object CMakeFiles/kangle.dir/src/KListenConfigParser.cpp.o
[ 56%] Building CXX object CMakeFiles/kangle.dir/src/KLogDrill.cpp.o
[ 57%] Building CXX object CMakeFiles/kangle.dir/src/KLogElement.cpp.o
[ 57%] Building CXX object CMakeFiles/kangle.dir/src/KLogHandle.cpp.o
[ 57%] Building CXX object CMakeFiles/kangle.dir/src/KLogManage.cpp.o
[ 57%] Building CXX object CMakeFiles/kangle.dir/src/KMapRedirectMark.cpp.o
[ 58%] Building CXX object CMakeFiles/kangle.dir/src/KMultiAcserver.cpp.o
[ 58%] Building CXX object CMakeFiles/kangle.dir/src/KMultiHostAcl.cpp.o
[ 58%] Building CXX object CMakeFiles/kangle.dir/src/KMultiPartInputFilter.cpp.o
[ 58%] Building CXX object CMakeFiles/kangle.dir/src/KObjectList.cpp.o
[ 59%] Building CXX object CMakeFiles/kangle.dir/src/KPathRedirect.cpp.o
[ 59%] Building CXX object CMakeFiles/kangle.dir/src/KPerIpAcl.cpp.o
[ 59%] Building CXX object CMakeFiles/kangle.dir/src/KPipeStream.cpp.o
[ 60%] Building CXX object CMakeFiles/kangle.dir/src/KProcess.cpp.o
[ 60%] Building CXX object CMakeFiles/kangle.dir/src/KProcessManage.cpp.o
[ 60%] Building CXX object CMakeFiles/kangle.dir/src/KPushGate.cpp.o
[ 60%] Building CXX object CMakeFiles/kangle.dir/src/KQueueMark.cpp.o
[ 61%] Building CXX object CMakeFiles/kangle.dir/src/KReadWriteBuffer.cpp.o
[ 61%] Building CXX object CMakeFiles/kangle.dir/src/KRedirect.cpp.o
[ 61%] Building CXX object CMakeFiles/kangle.dir/src/KRedirectMark.cpp.o
[ 61%] Building CXX object CMakeFiles/kangle.dir/src/KReg.cpp.o
[ 63%] Building CXX object CMakeFiles/kangle.dir/src/KReplaceContentFilter.cpp.o
[ 63%] Building CXX object CMakeFiles/kangle.dir/src/KReplaceContentMark.cpp.o
[ 63%] Building CXX object CMakeFiles/kangle.dir/src/KReplaceUrlMark.cpp.o
[ 63%] Building CXX object CMakeFiles/kangle.dir/src/KReportIp.cpp.o
[ 64%] Building CXX object CMakeFiles/kangle.dir/src/KRequestQueue.cpp.o
[ 64%] Building CXX object CMakeFiles/kangle.dir/src/KResponseContext.cpp.o
[ 64%] Building CXX object CMakeFiles/kangle.dir/src/KRewriteMark.cpp.o
[ 64%] Building CXX object CMakeFiles/kangle.dir/src/KRewriteMarkEx.cpp.o
[ 65%] Building CXX object CMakeFiles/kangle.dir/src/KSSLSniContext.cpp.o
[ 65%] Building CXX object CMakeFiles/kangle.dir/src/KSboFile.cpp.o
[ 65%] Building CXX object CMakeFiles/kangle.dir/src/KSequence.cpp.o
[ 66%] Building CXX object CMakeFiles/kangle.dir/src/KServerListen.cpp.o
[ 66%] Building CXX object CMakeFiles/kangle.dir/src/KServiceProvider.cpp.o
[ 66%] Building CXX object CMakeFiles/kangle.dir/src/KSharedBigObject.cpp.o
[ 66%] Building CXX object CMakeFiles/kangle.dir/src/KSimulateRequest.cpp.o
[ 67%] Building CXX object CMakeFiles/kangle.dir/src/KSingleAcserver.cpp.o
[ 67%] Building CXX object CMakeFiles/kangle.dir/src/KSingleProgram.cpp.o
[ 67%] Building CXX object CMakeFiles/kangle.dir/src/KSocketApi.cpp.o
[ 67%] Building CXX object CMakeFiles/kangle.dir/src/KSocketBuffer.cpp.o
[ 68%] Building CXX object CMakeFiles/kangle.dir/src/KSqliteDiskCacheIndex.cpp.o
[ 68%] Building CXX object CMakeFiles/kangle.dir/src/KStaticFetchObject.cpp.o
[ 68%] Building CXX object CMakeFiles/kangle.dir/src/KSubVirtualHost.cpp.o
[ 68%] Building CXX object CMakeFiles/kangle.dir/src/KTable.cpp.o
[ 69%] Building CXX object CMakeFiles/kangle.dir/src/KTargetRate.cpp.o
[ 69%] Building CXX object CMakeFiles/kangle.dir/src/KTempFile.cpp.o
[ 69%] Building CXX object CMakeFiles/kangle.dir/src/KTempleteVirtualHost.cpp.o
[ 70%] Building CXX object CMakeFiles/kangle.dir/src/KTimeMatch.cpp.o
[ 70%] Building CXX object CMakeFiles/kangle.dir/src/KTimer.cpp.o
[ 70%] Building CXX object CMakeFiles/kangle.dir/src/KUdpSelector.cpp.o
[ 70%] Building CXX object CMakeFiles/kangle.dir/src/KUrlParser.cpp.o
[ 71%] Building CXX object CMakeFiles/kangle.dir/src/KUrlValue.cpp.o
[ 71%] Building CXX object CMakeFiles/kangle.dir/src/KUserManageInterface.cpp.o
[ 71%] Building CXX object CMakeFiles/kangle.dir/src/KVary.cpp.o
[ 71%] Building CXX object CMakeFiles/kangle.dir/src/KVectorBufferFetchObject.cpp.o
[ 72%] Building CXX object CMakeFiles/kangle.dir/src/KVirtualHost.cpp.o
[ 72%] Building CXX object CMakeFiles/kangle.dir/src/KVirtualHostContainer.cpp.o
[ 72%] Building CXX object CMakeFiles/kangle.dir/src/KVirtualHostDatabase.cpp.o
[ 72%] Building CXX object CMakeFiles/kangle.dir/src/KVirtualHostManage.cpp.o
[ 73%] Building CXX object CMakeFiles/kangle.dir/src/KVirtualHostProcess.cpp.o
[ 73%] Building CXX object CMakeFiles/kangle.dir/src/KWhiteList.cpp.o
[ 73%] Building CXX object CMakeFiles/kangle.dir/src/KWinCgiEnv.cpp.o
[ 75%] Building CXX object CMakeFiles/kangle.dir/src/KWriteBack.cpp.o
[ 75%] Building CXX object CMakeFiles/kangle.dir/src/KWriteBackManager.cpp.o
[ 75%] Building CXX object CMakeFiles/kangle.dir/src/KXmlSupport.cpp.o
[ 75%] Building CXX object CMakeFiles/kangle.dir/src/cache.cpp.o
[ 76%] Building CXX object CMakeFiles/kangle.dir/src/crash.cpp.o
[ 76%] Building CXX object CMakeFiles/kangle.dir/src/directory.cpp.o
[ 76%] Building CXX object CMakeFiles/kangle.dir/src/garbage_c.cpp.o
[ 76%] Building CXX object CMakeFiles/kangle.dir/src/kmd5.cpp.o
[ 77%] Building CXX object CMakeFiles/kangle.dir/src/log.cpp.o
[ 77%] Building CXX object CMakeFiles/kangle.dir/src/main.cpp.o
[ 77%] Building CXX object CMakeFiles/kangle.dir/src/process.cpp.o
[ 77%] Building CXX object CMakeFiles/kangle.dir/src/ssl_utils.cpp.o
[ 78%] Building CXX object CMakeFiles/kangle.dir/src/test.cpp.o
/root/kangle/src/test.cpp: In function 'void test_suffix_corrupt()':
/root/kangle/src/test.cpp:365:19: warning: 'void* __builtin_memcpy(void*, const void*, long unsigned int)' writing 5 bytes into a region of size 4 overflows the destination [-Wstringop-overflow=]
  365 |         kgl_memcpy(str,"test1",5);
/root/kangle/src/test.cpp:364:31: note: destination object of size 4 allocated by 'operator new []'
  364 |         char *str = new char[4];
      |                               ^
/root/kangle/src/test.cpp: In function 'void test_prefix_corrupt()':
/root/kangle/src/test.cpp:371:19: warning: 'void* __builtin_memcpy(void*, const void*, long unsigned int)' writing 4 bytes into a region of size 0 overflows the destination [-Wstringop-overflow=]
  371 |         kgl_memcpy(pp,"test",4);
/root/kangle/src/test.cpp:369:35: note: at offset -1 into destination object of size 4 allocated by 'malloc'
  369 |         char *str = (char *)malloc(4);
      |                             ~~~~~~^~~
[ 78%] Building CXX object CMakeFiles/kangle.dir/src/time_utils.cpp.o
[ 78%] Building CXX object CMakeFiles/kangle.dir/src/utils.cpp.o
[ 78%] Building CXX object CMakeFiles/kangle.dir/src/win_iconv.cpp.o
[ 79%] Building CXX object CMakeFiles/kangle.dir/src/windows_service.cpp.o
[ 79%] Building CXX object CMakeFiles/kangle.dir/src/work.cpp.o
[ 79%] Building CXX object CMakeFiles/kangle.dir/khttpd/davlib/KXml.cpp.o
[ 80%] Building CXX object CMakeFiles/kangle.dir/khttpd/davlib/KXmlContext.cpp.o
[ 80%] Building CXX object CMakeFiles/kangle.dir/khttpd/davlib/KXmlException.cpp.o
[ 80%] Linking CXX executable kangle
/usr/lib/gcc/x86_64-alpine-linux-musl/12.2.1/../../../../x86_64-alpine-linux-musl/bin/ld: CMakeFiles/kangle.dir/src/KReg.cpp.o: in function `KReg::~KReg()':
KReg.cpp:(.text+0xe3): undefined reference to `pcre_free'
/usr/lib/gcc/x86_64-alpine-linux-musl/12.2.1/../../../../x86_64-alpine-linux-musl/bin/ld: CMakeFiles/kangle.dir/src/KReg.cpp.o: in function `KReg::isPartialModel()':
KReg.cpp:(.text+0x276): undefined reference to `pcre_fullinfo'
/usr/lib/gcc/x86_64-alpine-linux-musl/12.2.1/../../../../x86_64-alpine-linux-musl/bin/ld: CMakeFiles/kangle.dir/src/KReg.cpp.o: in function `KReg::setModel(char const*, int, int)':
KReg.cpp:(.text+0x325): undefined reference to `pcre_free'
/usr/lib/gcc/x86_64-alpine-linux-musl/12.2.1/../../../../x86_64-alpine-linux-musl/bin/ld: KReg.cpp:(.text+0x37c): undefined reference to `pcre_compile'
/usr/lib/gcc/x86_64-alpine-linux-musl/12.2.1/../../../../x86_64-alpine-linux-musl/bin/ld: KReg.cpp:(.text+0x3cb): undefined reference to `pcre_study'
/usr/lib/gcc/x86_64-alpine-linux-musl/12.2.1/../../../../x86_64-alpine-linux-musl/bin/ld: CMakeFiles/kangle.dir/src/KReg.cpp.o: in function `KReg::matchPartial(char const*, int, int, int*, int, int*, int)':
KReg.cpp:(.text+0x61f): undefined reference to `pcre_dfa_exec'
/usr/lib/gcc/x86_64-alpine-linux-musl/12.2.1/../../../../x86_64-alpine-linux-musl/bin/ld: CMakeFiles/kangle.dir/src/KReg.cpp.o: in function `KReg::matchNext(char const*, int, int, int*, int, int*, int)':
KReg.cpp:(.text+0x685): undefined reference to `pcre_dfa_exec'
/usr/lib/gcc/x86_64-alpine-linux-musl/12.2.1/../../../../x86_64-alpine-linux-musl/bin/ld: CMakeFiles/kangle.dir/src/KReg.cpp.o: in function `KReg::match(char const*, int, int, int*, int)':
KReg.cpp:(.text+0x76c): undefined reference to `pcre_exec'
/usr/lib/gcc/x86_64-alpine-linux-musl/12.2.1/../../../../x86_64-alpine-linux-musl/bin/ld: CMakeFiles/kangle.dir/src/KReg.cpp.o: in function `KReg::freeStudy(pcre_extra*)':
KReg.cpp:(.text._ZN4KReg9freeStudyEP10pcre_extra[_ZN4KReg9freeStudyEP10pcre_extra]+0x14): undefined reference to `pcre_free_study'
/usr/lib/gcc/x86_64-alpine-linux-musl/12.2.1/../../../../x86_64-alpine-linux-musl/bin/ld: CMakeFiles/kangle.dir/src/main.cpp.o: in function `Usage(bool)':
main.cpp:(.text+0x1083): undefined reference to `pcre_version'
/usr/lib/gcc/x86_64-alpine-linux-musl/12.2.1/../../../../x86_64-alpine-linux-musl/bin/ld: khttpd/libkhttpd.a(kfiber.c.o): in function `__kfiber_switch(_kfiber*, _kfiber*)':
kfiber.c:(.text+0x371): undefined reference to `swapcontext'
/usr/lib/gcc/x86_64-alpine-linux-musl/12.2.1/../../../../x86_64-alpine-linux-musl/bin/ld: khttpd/libkhttpd.a(kfiber.c.o): in function `kfiber_new':
kfiber.c:(.text+0xab9): undefined reference to `getcontext'
/usr/lib/gcc/x86_64-alpine-linux-musl/12.2.1/../../../../x86_64-alpine-linux-musl/bin/ld: kfiber.c:(.text+0xb3e): undefined reference to `makecontext'
collect2: error: ld returned 1 exit status
make[2]: *** [CMakeFiles/kangle.dir/build.make:3620: kangle] Error 1
make[1]: *** [CMakeFiles/Makefile2:195: CMakeFiles/kangle.dir/all] Error 2
make: *** [Makefile:156: all] Error 2
localhost:~/kangle/build# make clean
localhost:~/kangle/build# rm -f CMake
CMakeCache.txt  CMakeFiles/
localhost:~/kangle/build# rm -f CMake
CMakeCache.txt  CMakeFiles/
localhost:~/kangle/build# rm -f CMake
CMakeCache.txt  CMakeFiles/
localhost:~/kangle/build# rm -f CMake
CMakeCache.txt  CMakeFiles/
localhost:~/kangle/build# rm -f CMakeCache.txt
localhost:~/kangle/build# cmake ..
-- The C compiler identification is GNU 12.2.1
-- The CXX compiler identification is GNU 12.2.1
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/bin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Check for working CXX compiler: /usr/bin/c++ - skipped
-- Detecting CXX compile features
-- Detecting CXX compile features - done
-- Found PCRE: /usr/lib/libpcre.so
-- Looking for timegm
-- Looking for timegm - found
-- Looking for KSOCKET_SSL
-- Looking for KSOCKET_SSL - not found
-- system is Linux
-- OPENSSL_DIR is
-- Could NOT find OpenSSL, try to set the path to OpenSSL root folder in the system variable OPENSSL_ROOT_DIR (missing: OPENSSL_CRYPTO_LIBRARY OPENSSL_INCLUDE_DIR)
-- cann't find openssl
-- Looking for accept4
-- Looking for accept4 - found
-- Looking for memalign
-- Looking for memalign - found
-- Looking for setmntent
-- Looking for setmntent - found
-- /root/kangle/kasync/include
-- build as static libary
-- KHTTP_LIBS pthread
-- Found ZLIB: /lib/libz.so (found version "1.2.13")
-- Could NOT find SQLite3 (missing: SQLite3_INCLUDE_DIR SQLite3_LIBRARY)
-- kangle version 3.6.0
-- Looking for 3 include files ksapi.h, ..., kforwin32.h
-- Looking for 3 include files ksapi.h, ..., kforwin32.h - found
-- Configuring done
-- Generating done
-- Build files have been written to: /root/kangle/build
localhost:~/kangle/build# make
[  0%] Building C object CMakeFiles/sqlite3.dir/third_party/sqlite/sqlite3.c.o
cc1: warning: command-line option '-Wno-invalid-offsetof' is valid for C++/ObjC++ but not for C
[  0%] Linking C static library libsqlite3.a
[  0%] Built target sqlite3
[  0%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KAutoBuffer.cpp.o
[  0%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KDechunkContext.cpp.o
[  0%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KDechunkEngine.cpp.o
[  1%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KHttp2.cpp.o
[  1%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KHttp2Table.cpp.o
[  1%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KHttp2Upstream.cpp.o
[  1%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KHttp2WriteBuffer.cpp.o
[  2%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KHttp3.cpp.o
[  2%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KHttp3Connection.cpp.o
[  2%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KHttp3Sink.cpp.o
[  2%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KHttpKeyValue.cpp.o
[  3%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KHttpLib.cpp.o
[  3%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KHttpParser.cpp.o
[  3%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KHttpServer.cpp.o
[  3%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KHttpSink.cpp.o
[  4%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KHttpUpstream.cpp.o
[  4%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KHuffDecode.cpp.o
[  4%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KHuffEncode.cpp.o
[  5%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KPoolableSocketContainer.cpp.o
[  5%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KPreRequest.cpp.o
[  5%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KProxy.cpp.o
[  5%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KRequest.cpp.o
[  6%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KSink.cpp.o
In file included from /root/kangle/kasync/include/klist.h:4,
                 from /root/kangle/kasync/include/kselector.h:6,
                 from /root/kangle/khttpd/include/KSink.h:3,
                 from /root/kangle/khttpd/src/KSink.cpp:1:
/root/kangle/khttpd/src/KSink.cpp: In function 'kev_result ksink_iterator(KOPAQUE, void*, int)':
/root/kangle/khttpd/src/KSink.cpp:32:58: warning: 'offsetof' within non-standard-layout type 'KSink' is conditionally-supported [-Winvalid-offsetof]
   32 |                 KSink* sink = (KSink*)kgl_list_data(pos, KSink, queue);
/root/kangle/khttpd/src/KSink.cpp:32:39: note: in expansion of macro 'kgl_list_data'
   32 |                 KSink* sink = (KSink*)kgl_list_data(pos, KSink, queue);
      |                                       ^~~~~~~~~~~~~
[  6%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KSockPoolHelper.cpp.o
[  6%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KStream.cpp.o
[  6%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KStringBuf.cpp.o
[  7%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KTcpSink.cpp.o
[  7%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KTcpUpstream.cpp.o
[  7%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KTsUpstream.cpp.o
[  7%] Building CXX object khttpd/CMakeFiles/khttpd.dir/src/KUpstream.cpp.o
[  8%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kaddr.c.o
[  8%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kasync_file.c.o
[  8%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kasync_worker.c.o
[  9%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kbuf.c.o
[  9%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kconnection.c.o
[  9%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kepoll_selector.c.o
[  9%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kfiber.c.o
[ 10%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kfiber_sync.c.o
[ 10%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kfile.c.o
[ 10%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kgl_ssl.c.o
[ 10%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kiocp_selector.c.o
[ 11%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kiouring_selector.c.o
[ 11%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kkqueue_selector.c.o
[ 11%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/klib.c.o
[ 11%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/klog.c.o
[ 13%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kmalloc.c.o
[ 13%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kmalloc_debug.c.o
[ 13%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kmalloc_debug_win32.c.o
[ 14%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/krbtree.c.o
[ 14%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kselectable.c.o
[ 14%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kselector.c.o
[ 14%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kselector_manager.c.o
[ 15%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kserver.c.o
[ 15%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/ksocket.c.o
[ 15%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kssl_bio.c.o
[ 15%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kthread.c.o
[ 16%] Building CXX object khttpd/CMakeFiles/khttpd.dir/__/kasync/src/kudp.c.o
[ 16%] Linking CXX static library libkhttpd.a
[ 16%] Built target khttpd
[ 16%] Building CXX object CMakeFiles/kangle.dir/module/whm/KWhmService.cpp.o
[ 17%] Building CXX object CMakeFiles/kangle.dir/module/whm/WhmCallMap.cpp.o
[ 17%] Building CXX object CMakeFiles/kangle.dir/module/whm/WhmCommand.cpp.o
[ 17%] Building CXX object CMakeFiles/kangle.dir/module/whm/WhmContext.cpp.o
[ 18%] Building CXX object CMakeFiles/kangle.dir/module/whm/WhmDso.cpp.o
[ 18%] Building CXX object CMakeFiles/kangle.dir/module/whm/WhmExtend.cpp.o
[ 18%] Building CXX object CMakeFiles/kangle.dir/module/whm/WhmLog.cpp.o
[ 18%] Building CXX object CMakeFiles/kangle.dir/module/whm/WhmPackage.cpp.o
[ 19%] Building CXX object CMakeFiles/kangle.dir/module/whm/WhmPackageManage.cpp.o
[ 19%] Building CXX object CMakeFiles/kangle.dir/module/whm/WhmShell.cpp.o
[ 19%] Building CXX object CMakeFiles/kangle.dir/module/whm/WhmShellProcess.cpp.o
[ 19%] Building CXX object CMakeFiles/kangle.dir/module/whm/WhmShellSession.cpp.o
[ 20%] Building CXX object CMakeFiles/kangle.dir/module/whm/WhmUrl.cpp.o
[ 20%] Building CXX object CMakeFiles/kangle.dir/module/whm/dllmain.cpp.o
[ 20%] Building CXX object CMakeFiles/kangle.dir/module/whm/stdafx.cpp.o
[ 20%] Building CXX object CMakeFiles/kangle.dir/module/whm/whm.cpp.o
[ 21%] Building CXX object CMakeFiles/kangle.dir/module/whm/whmdso/core/core.cpp.o
[ 21%] Building CXX object CMakeFiles/kangle.dir/src/EdcodeUtils.cpp.o
[ 21%] Building CXX object CMakeFiles/kangle.dir/src/HttpCore.cpp.o
[ 21%] Building CXX object CMakeFiles/kangle.dir/src/HttpFiber.cpp.o
[ 22%] Building CXX object CMakeFiles/kangle.dir/src/KAccess.cpp.o
[ 22%] Building CXX object CMakeFiles/kangle.dir/src/KAccessDso.cpp.o
[ 22%] Building CXX object CMakeFiles/kangle.dir/src/KAccessDsoSupport.cpp.o
[ 23%] Building CXX object CMakeFiles/kangle.dir/src/KAccessParser.cpp.o
[ 23%] Building CXX object CMakeFiles/kangle.dir/src/KAcserver.cpp.o
[ 23%] Building CXX object CMakeFiles/kangle.dir/src/KAcserverManager.cpp.o
[ 23%] Building CXX object CMakeFiles/kangle.dir/src/KAjpFetchObject.cpp.o
[ 25%] Building CXX object CMakeFiles/kangle.dir/src/KAjpMessage.cpp.o
[ 25%] Building CXX object CMakeFiles/kangle.dir/src/KApacheVirtualHost.cpp.o
[ 25%] Building CXX object CMakeFiles/kangle.dir/src/KApiDso.cpp.o
[ 25%] Building CXX object CMakeFiles/kangle.dir/src/KApiEnv.cpp.o
[ 26%] Building CXX object CMakeFiles/kangle.dir/src/KApiFastcgiFetchObject.cpp.o
[ 26%] Building CXX object CMakeFiles/kangle.dir/src/KApiFetchObject.cpp.o
[ 26%] Building CXX object CMakeFiles/kangle.dir/src/KApiPipeStream.cpp.o
[ 26%] Building CXX object CMakeFiles/kangle.dir/src/KApiProcess.cpp.o
[ 27%] Building CXX object CMakeFiles/kangle.dir/src/KApiRedirect.cpp.o
[ 27%] Building CXX object CMakeFiles/kangle.dir/src/KAsyncFetchObject.cpp.o
[ 27%] Building CXX object CMakeFiles/kangle.dir/src/KAsyncFileApi.cpp.o
[ 28%] Building CXX object CMakeFiles/kangle.dir/src/KAsyncWorker.cpp.o
[ 28%] Building CXX object CMakeFiles/kangle.dir/src/KAuthMark.cpp.o
[ 28%] Building CXX object CMakeFiles/kangle.dir/src/KBaseVirtualHost.cpp.o
[ 28%] Building CXX object CMakeFiles/kangle.dir/src/KBigObject.cpp.o
[ 29%] Building CXX object CMakeFiles/kangle.dir/src/KBigObjectContext.cpp.o
[ 29%] Building CXX object CMakeFiles/kangle.dir/src/KBigObjectManager.cpp.o
[ 29%] Building CXX object CMakeFiles/kangle.dir/src/KBigObjectStream.cpp.o
[ 29%] Building CXX object CMakeFiles/kangle.dir/src/KBrotli.cpp.o
[ 30%] Building CXX object CMakeFiles/kangle.dir/src/KBuffer.cpp.o
[ 30%] Building CXX object CMakeFiles/kangle.dir/src/KBufferFetchObject.cpp.o
[ 30%] Building CXX object CMakeFiles/kangle.dir/src/KBugReport.cpp.o
[ 30%] Building CXX object CMakeFiles/kangle.dir/src/KCache.cpp.o
[ 31%] Building CXX object CMakeFiles/kangle.dir/src/KCacheStream.cpp.o
[ 31%] Building CXX object CMakeFiles/kangle.dir/src/KCdnContainer.cpp.o
[ 31%] Building CXX object CMakeFiles/kangle.dir/src/KCdnRedirect.cpp.o
[ 32%] Building CXX object CMakeFiles/kangle.dir/src/KCdnRewriteMark.cpp.o
[ 32%] Building CXX object CMakeFiles/kangle.dir/src/KCgiEnv.cpp.o
[ 32%] Building CXX object CMakeFiles/kangle.dir/src/KChain.cpp.o
[ 32%] Building CXX object CMakeFiles/kangle.dir/src/KChunked.cpp.o
[ 33%] Building CXX object CMakeFiles/kangle.dir/src/KCloudIpAcl.cpp.o
[ 33%] Building CXX object CMakeFiles/kangle.dir/src/KCmdPoolableRedirect.cpp.o
[ 33%] Building CXX object CMakeFiles/kangle.dir/src/KCmdProcess.cpp.o
[ 33%] Building CXX object CMakeFiles/kangle.dir/src/KCompressStream.cpp.o
[ 34%] Building CXX object CMakeFiles/kangle.dir/src/KConfig.cpp.o
[ 34%] Building CXX object CMakeFiles/kangle.dir/src/KConfigBuilder.cpp.o
[ 34%] Building CXX object CMakeFiles/kangle.dir/src/KConfigParser.cpp.o
[ 34%] Building CXX object CMakeFiles/kangle.dir/src/KConnectionLimit.cpp.o
[ 35%] Building CXX object CMakeFiles/kangle.dir/src/KContentMark.cpp.o
[ 35%] Building CXX object CMakeFiles/kangle.dir/src/KContentTransfer.cpp.o
[ 35%] Building CXX object CMakeFiles/kangle.dir/src/KContentType.cpp.o
[ 35%] Building CXX object CMakeFiles/kangle.dir/src/KContext.cpp.o
[ 36%] Building CXX object CMakeFiles/kangle.dir/src/KDefaultFetchObject.cpp.o
[ 36%] Building CXX object CMakeFiles/kangle.dir/src/KDirectoryFetchObject.cpp.o
[ 36%] Building CXX object CMakeFiles/kangle.dir/src/KDiskCache.cpp.o
[ 38%] Building CXX object CMakeFiles/kangle.dir/src/KDiskCacheIndex.cpp.o
[ 38%] Building CXX object CMakeFiles/kangle.dir/src/KDiskCacheStream.cpp.o
[ 38%] Building CXX object CMakeFiles/kangle.dir/src/KDomainUser.cpp.o
[ 38%] Building CXX object CMakeFiles/kangle.dir/src/KDsoAsyncFetchObject.cpp.o
[ 39%] Building CXX object CMakeFiles/kangle.dir/src/KDsoConfigParser.cpp.o
[ 39%] Building CXX object CMakeFiles/kangle.dir/src/KDsoExtend.cpp.o
[ 39%] Building CXX object CMakeFiles/kangle.dir/src/KDsoExtendManage.cpp.o
[ 39%] Building CXX object CMakeFiles/kangle.dir/src/KDsoFilter.cpp.o
[ 40%] Building CXX object CMakeFiles/kangle.dir/src/KDsoModule.cpp.o
[ 40%] Building CXX object CMakeFiles/kangle.dir/src/KDsoNextUpstream.cpp.o
[ 40%] Building CXX object CMakeFiles/kangle.dir/src/KDsoRedirect.cpp.o
[ 40%] Building CXX object CMakeFiles/kangle.dir/src/KDynamicListen.cpp.o
[ 41%] Building CXX object CMakeFiles/kangle.dir/src/KDynamicString.cpp.o
[ 41%] Building CXX object CMakeFiles/kangle.dir/src/KEnvInterface.cpp.o
[ 41%] Building CXX object CMakeFiles/kangle.dir/src/KExtendProgram.cpp.o
[ 42%] Building CXX object CMakeFiles/kangle.dir/src/KFastcgiFetchObject.cpp.o
[ 42%] Building CXX object CMakeFiles/kangle.dir/src/KFastcgiUtils.cpp.o
[ 42%] Building CXX object CMakeFiles/kangle.dir/src/KFdStream.cpp.o
[ 42%] Building CXX object CMakeFiles/kangle.dir/src/KFetchBigObject.cpp.o
[ 43%] Building CXX object CMakeFiles/kangle.dir/src/KFetchObject.cpp.o
[ 43%] Building CXX object CMakeFiles/kangle.dir/src/KFileMsg.cpp.o
[ 43%] Building CXX object CMakeFiles/kangle.dir/src/KFileName.cpp.o
[ 43%] Building CXX object CMakeFiles/kangle.dir/src/KFixHeaderMark.cpp.o
[ 44%] Building CXX object CMakeFiles/kangle.dir/src/KFormInputFilter.cpp.o
[ 44%] Building CXX object CMakeFiles/kangle.dir/src/KGeoMark.cpp.o
[ 44%] Building CXX object CMakeFiles/kangle.dir/src/KGzip.cpp.o
[ 44%] Building CXX object CMakeFiles/kangle.dir/src/KHtAccess.cpp.o
[ 45%] Building CXX object CMakeFiles/kangle.dir/src/KHtModule.cpp.o
[ 45%] Building CXX object CMakeFiles/kangle.dir/src/KHtRewriteModule.cpp.o
[ 45%] Building CXX object CMakeFiles/kangle.dir/src/KHtmlSupport.cpp.o
[ 46%] Building CXX object CMakeFiles/kangle.dir/src/KHtmlSupportException.cpp.o
[ 46%] Building CXX object CMakeFiles/kangle.dir/src/KHtmlTagFilter.cpp.o
[ 46%] Building CXX object CMakeFiles/kangle.dir/src/KHttpAuth.cpp.o
[ 46%] Building CXX object CMakeFiles/kangle.dir/src/KHttpBasicAuth.cpp.o
[ 47%] Building CXX object CMakeFiles/kangle.dir/src/KHttpClient.cpp.o
[ 47%] Building CXX object CMakeFiles/kangle.dir/src/KHttpDigestAuth.cpp.o
[ 47%] Building CXX object CMakeFiles/kangle.dir/src/KHttpExt.cpp.o
[ 47%] Building CXX object CMakeFiles/kangle.dir/src/KHttpField.cpp.o
[ 48%] Building CXX object CMakeFiles/kangle.dir/src/KHttpFilterContext.cpp.o
[ 48%] Building CXX object CMakeFiles/kangle.dir/src/KHttpFilterDso.cpp.o
[ 48%] Building CXX object CMakeFiles/kangle.dir/src/KHttpFilterDsoManage.cpp.o
[ 48%] Building CXX object CMakeFiles/kangle.dir/src/KHttpFilterHook.cpp.o
[ 50%] Building CXX object CMakeFiles/kangle.dir/src/KHttpFilterHookCollectRequest.cpp.o
[ 50%] Building CXX object CMakeFiles/kangle.dir/src/KHttpFilterHookCollectResponse.cpp.o
[ 50%] Building CXX object CMakeFiles/kangle.dir/src/KHttpFilterManage.cpp.o
[ 50%] Building CXX object CMakeFiles/kangle.dir/src/KHttpFilterStream.cpp.o
[ 51%] Building CXX object CMakeFiles/kangle.dir/src/KHttpManage.cpp.o
[ 51%] Building CXX object CMakeFiles/kangle.dir/src/KHttpObject.cpp.o
[ 51%] Building CXX object CMakeFiles/kangle.dir/src/KHttpObjectApi.cpp.o
[ 52%] Building CXX object CMakeFiles/kangle.dir/src/KHttpObjectHash.cpp.o
[ 52%] Building CXX object CMakeFiles/kangle.dir/src/KHttpObjectSwaping.cpp.o
[ 52%] Building CXX object CMakeFiles/kangle.dir/src/KHttpPost.cpp.o
[ 52%] Building CXX object CMakeFiles/kangle.dir/src/KHttpProxyFetchObject.cpp.o
[ 53%] Building CXX object CMakeFiles/kangle.dir/src/KHttpRequest.cpp.o
[ 53%] Building CXX object CMakeFiles/kangle.dir/src/KHttpResponseParser.cpp.o
[ 53%] Building CXX object CMakeFiles/kangle.dir/src/KHttpServerParser.cpp.o
[ 53%] Building CXX object CMakeFiles/kangle.dir/src/KHttpTransfer.cpp.o
[ 54%] Building CXX object CMakeFiles/kangle.dir/src/KISAPIServiceProvider.cpp.o
[ 54%] Building CXX object CMakeFiles/kangle.dir/src/KInputFilter.cpp.o
[ 54%] Building CXX object CMakeFiles/kangle.dir/src/KIpList.cpp.o
[ 54%] Building CXX object CMakeFiles/kangle.dir/src/KIpMap.cpp.o
[ 55%] Building CXX object CMakeFiles/kangle.dir/src/KIpSpeedLimitMark.cpp.o
[ 55%] Building CXX object CMakeFiles/kangle.dir/src/KLang.cpp.o
[ 55%] Building CXX object CMakeFiles/kangle.dir/src/KLangParser.cpp.o
[ 56%] Building CXX object CMakeFiles/kangle.dir/src/KLineFile.cpp.o
[ 56%] Building CXX object CMakeFiles/kangle.dir/src/KListNode.cpp.o
[ 56%] Building CXX object CMakeFiles/kangle.dir/src/KListenConfigParser.cpp.o
[ 56%] Building CXX object CMakeFiles/kangle.dir/src/KLogDrill.cpp.o
[ 57%] Building CXX object CMakeFiles/kangle.dir/src/KLogElement.cpp.o
[ 57%] Building CXX object CMakeFiles/kangle.dir/src/KLogHandle.cpp.o
[ 57%] Building CXX object CMakeFiles/kangle.dir/src/KLogManage.cpp.o
[ 57%] Building CXX object CMakeFiles/kangle.dir/src/KMapRedirectMark.cpp.o
[ 58%] Building CXX object CMakeFiles/kangle.dir/src/KMultiAcserver.cpp.o
[ 58%] Building CXX object CMakeFiles/kangle.dir/src/KMultiHostAcl.cpp.o
[ 58%] Building CXX object CMakeFiles/kangle.dir/src/KMultiPartInputFilter.cpp.o
[ 58%] Building CXX object CMakeFiles/kangle.dir/src/KObjectList.cpp.o
[ 59%] Building CXX object CMakeFiles/kangle.dir/src/KPathRedirect.cpp.o
[ 59%] Building CXX object CMakeFiles/kangle.dir/src/KPerIpAcl.cpp.o
[ 59%] Building CXX object CMakeFiles/kangle.dir/src/KPipeStream.cpp.o
[ 60%] Building CXX object CMakeFiles/kangle.dir/src/KProcess.cpp.o
[ 60%] Building CXX object CMakeFiles/kangle.dir/src/KProcessManage.cpp.o
[ 60%] Building CXX object CMakeFiles/kangle.dir/src/KPushGate.cpp.o
[ 60%] Building CXX object CMakeFiles/kangle.dir/src/KQueueMark.cpp.o
[ 61%] Building CXX object CMakeFiles/kangle.dir/src/KReadWriteBuffer.cpp.o
[ 61%] Building CXX object CMakeFiles/kangle.dir/src/KRedirect.cpp.o
[ 61%] Building CXX object CMakeFiles/kangle.dir/src/KRedirectMark.cpp.o
[ 61%] Building CXX object CMakeFiles/kangle.dir/src/KReg.cpp.o
[ 63%] Building CXX object CMakeFiles/kangle.dir/src/KReplaceContentFilter.cpp.o
[ 63%] Building CXX object CMakeFiles/kangle.dir/src/KReplaceContentMark.cpp.o
[ 63%] Building CXX object CMakeFiles/kangle.dir/src/KReplaceUrlMark.cpp.o
[ 63%] Building CXX object CMakeFiles/kangle.dir/src/KReportIp.cpp.o
[ 64%] Building CXX object CMakeFiles/kangle.dir/src/KRequestQueue.cpp.o
[ 64%] Building CXX object CMakeFiles/kangle.dir/src/KResponseContext.cpp.o
[ 64%] Building CXX object CMakeFiles/kangle.dir/src/KRewriteMark.cpp.o
[ 64%] Building CXX object CMakeFiles/kangle.dir/src/KRewriteMarkEx.cpp.o
[ 65%] Building CXX object CMakeFiles/kangle.dir/src/KSSLSniContext.cpp.o
[ 65%] Building CXX object CMakeFiles/kangle.dir/src/KSboFile.cpp.o
[ 65%] Building CXX object CMakeFiles/kangle.dir/src/KSequence.cpp.o
[ 66%] Building CXX object CMakeFiles/kangle.dir/src/KServerListen.cpp.o
[ 66%] Building CXX object CMakeFiles/kangle.dir/src/KServiceProvider.cpp.o
[ 66%] Building CXX object CMakeFiles/kangle.dir/src/KSharedBigObject.cpp.o
[ 66%] Building CXX object CMakeFiles/kangle.dir/src/KSimulateRequest.cpp.o
[ 67%] Building CXX object CMakeFiles/kangle.dir/src/KSingleAcserver.cpp.o
[ 67%] Building CXX object CMakeFiles/kangle.dir/src/KSingleProgram.cpp.o
[ 67%] Building CXX object CMakeFiles/kangle.dir/src/KSocketApi.cpp.o
[ 67%] Building CXX object CMakeFiles/kangle.dir/src/KSocketBuffer.cpp.o
[ 68%] Building CXX object CMakeFiles/kangle.dir/src/KSqliteDiskCacheIndex.cpp.o
[ 68%] Building CXX object CMakeFiles/kangle.dir/src/KStaticFetchObject.cpp.o
[ 68%] Building CXX object CMakeFiles/kangle.dir/src/KSubVirtualHost.cpp.o
[ 68%] Building CXX object CMakeFiles/kangle.dir/src/KTable.cpp.o
[ 69%] Building CXX object CMakeFiles/kangle.dir/src/KTargetRate.cpp.o
[ 69%] Building CXX object CMakeFiles/kangle.dir/src/KTempFile.cpp.o
[ 69%] Building CXX object CMakeFiles/kangle.dir/src/KTempleteVirtualHost.cpp.o
[ 70%] Building CXX object CMakeFiles/kangle.dir/src/KTimeMatch.cpp.o
[ 70%] Building CXX object CMakeFiles/kangle.dir/src/KTimer.cpp.o
[ 70%] Building CXX object CMakeFiles/kangle.dir/src/KUdpSelector.cpp.o
[ 70%] Building CXX object CMakeFiles/kangle.dir/src/KUrlParser.cpp.o
[ 71%] Building CXX object CMakeFiles/kangle.dir/src/KUrlValue.cpp.o
[ 71%] Building CXX object CMakeFiles/kangle.dir/src/KUserManageInterface.cpp.o
[ 71%] Building CXX object CMakeFiles/kangle.dir/src/KVary.cpp.o
[ 71%] Building CXX object CMakeFiles/kangle.dir/src/KVectorBufferFetchObject.cpp.o
[ 72%] Building CXX object CMakeFiles/kangle.dir/src/KVirtualHost.cpp.o
[ 72%] Building CXX object CMakeFiles/kangle.dir/src/KVirtualHostContainer.cpp.o
[ 72%] Building CXX object CMakeFiles/kangle.dir/src/KVirtualHostDatabase.cpp.o
[ 72%] Building CXX object CMakeFiles/kangle.dir/src/KVirtualHostManage.cpp.o
[ 73%] Building CXX object CMakeFiles/kangle.dir/src/KVirtualHostProcess.cpp.o
[ 73%] Building CXX object CMakeFiles/kangle.dir/src/KWhiteList.cpp.o
[ 73%] Building CXX object CMakeFiles/kangle.dir/src/KWinCgiEnv.cpp.o
[ 75%] Building CXX object CMakeFiles/kangle.dir/src/KWriteBack.cpp.o
[ 75%] Building CXX object CMakeFiles/kangle.dir/src/KWriteBackManager.cpp.o
[ 75%] Building CXX object CMakeFiles/kangle.dir/src/KXmlSupport.cpp.o
[ 75%] Building CXX object CMakeFiles/kangle.dir/src/cache.cpp.o
[ 76%] Building CXX object CMakeFiles/kangle.dir/src/crash.cpp.o
[ 76%] Building CXX object CMakeFiles/kangle.dir/src/directory.cpp.o
[ 76%] Building CXX object CMakeFiles/kangle.dir/src/garbage_c.cpp.o
[ 76%] Building CXX object CMakeFiles/kangle.dir/src/kmd5.cpp.o
[ 77%] Building CXX object CMakeFiles/kangle.dir/src/log.cpp.o
[ 77%] Building CXX object CMakeFiles/kangle.dir/src/main.cpp.o
[ 77%] Building CXX object CMakeFiles/kangle.dir/src/process.cpp.o
[ 77%] Building CXX object CMakeFiles/kangle.dir/src/ssl_utils.cpp.o
[ 78%] Building CXX object CMakeFiles/kangle.dir/src/test.cpp.o
/root/kangle/src/test.cpp: In function 'void test_suffix_corrupt()':
/root/kangle/src/test.cpp:365:19: warning: 'void* __builtin_memcpy(void*, const void*, long unsigned int)' writing 5 bytes into a region of size 4 overflows the destination [-Wstringop-overflow=]
  365 |         kgl_memcpy(str,"test1",5);
/root/kangle/src/test.cpp:364:31: note: destination object of size 4 allocated by 'operator new []'
  364 |         char *str = new char[4];
      |                               ^
/root/kangle/src/test.cpp: In function 'void test_prefix_corrupt()':
/root/kangle/src/test.cpp:371:19: warning: 'void* __builtin_memcpy(void*, const void*, long unsigned int)' writing 4 bytes into a region of size 0 overflows the destination [-Wstringop-overflow=]
  371 |         kgl_memcpy(pp,"test",4);
/root/kangle/src/test.cpp:369:35: note: at offset -1 into destination object of size 4 allocated by 'malloc'
  369 |         char *str = (char *)malloc(4);
      |                             ~~~~~~^~~
[ 78%] Building CXX object CMakeFiles/kangle.dir/src/time_utils.cpp.o
[ 78%] Building CXX object CMakeFiles/kangle.dir/src/utils.cpp.o
[ 78%] Building CXX object CMakeFiles/kangle.dir/src/win_iconv.cpp.o
[ 79%] Building CXX object CMakeFiles/kangle.dir/src/windows_service.cpp.o
[ 79%] Building CXX object CMakeFiles/kangle.dir/src/work.cpp.o
[ 79%] Building CXX object CMakeFiles/kangle.dir/khttpd/davlib/KXml.cpp.o
[ 80%] Building CXX object CMakeFiles/kangle.dir/khttpd/davlib/KXmlContext.cpp.o
[ 80%] Building CXX object CMakeFiles/kangle.dir/khttpd/davlib/KXmlException.cpp.o
[ 80%] Linking CXX executable kangle
/usr/lib/gcc/x86_64-alpine-linux-musl/12.2.1/../../../../x86_64-alpine-linux-musl/bin/ld: khttpd/libkhttpd.a(kfiber.c.o): in function `__kfiber_switch(_kfiber*, _kfiber*)':
+
+# target to generate assembly for a file
+arch/x86_64/makecontext.c.s:
+       $(MAKE) $(MAKESILENT) -f CMakeFiles/ucontext.dir/build.make CMakeFiles/ucontext.dir/arch/x86_64/makecontext.c.s
+.PHONY : arch/x86_64/makecontext.c.s
+
+arch/x86_64/setcontext.o: arch/x86_64/setcontext.S.o
+.PHONY : arch/x86_64/setcontext.o
+
+# target to build an object file
+arch/x86_64/setcontext.S.o:
+       $(MAKE) $(MAKESILENT) -f CMakeFiles/ucontext.dir/build.make CMakeFiles/ucontext.dir/arch/x86_64/setcontext.S.o
+.PHONY : arch/x86_64/setcontext.S.o
+
+arch/x86_64/swapcontext.o: arch/x86_64/swapcontext.S.o
+.PHONY : arch/x86_64/swapcontext.o
+
+# target to build an object file
+arch/x86_64/swapcontext.S.o:
+       $(MAKE) $(MAKESILENT) -f CMakeFiles/ucontext.dir/build.make CMakeFiles/ucontext.dir/arch/x86_64/swapcontext.S.o
+.PHONY : arch/x86_64/swapcontext.S.o
+
+arch/x86_64/trampoline.o: arch/x86_64/trampoline.c.o
+.PHONY : arch/x86_64/trampoline.o
+
+# target to build an object file
+arch/x86_64/trampoline.c.o:
+       $(MAKE) $(MAKESILENT) -f CMakeFiles/ucontext.dir/build.make CMakeFiles/ucontext.dir/arch/x86_64/trampoline.c.o
+.PHONY : arch/x86_64/trampoline.c.o
+
+arch/x86_64/trampoline.i: arch/x86_64/trampoline.c.i
+.PHONY : arch/x86_64/trampoline.i
+
+# target to preprocess a source file
+arch/x86_64/trampoline.c.i:
+       $(MAKE) $(MAKESILENT) -f CMakeFiles/ucontext.dir/build.make CMakeFiles/ucontext.dir/arch/x86_64/trampoline.c.i
+.PHONY : arch/x86_64/trampoline.c.i
+
+arch/x86_64/trampoline.s: arch/x86_64/trampoline.c.s
+.PHONY : arch/x86_64/trampoline.s
+
+# target to generate assembly for a file
+arch/x86_64/trampoline.c.s:
+       $(MAKE) $(MAKESILENT) -f CMakeFiles/ucontext.dir/build.make CMakeFiles/ucontext.dir/arch/x86_64/trampoline.c.s
+.PHONY : arch/x86_64/trampoline.c.s
+
+# Help Target
+help:
+       @echo "The following are some of the valid targets for this Makefile:"
+       @echo "... all (the default if no target is provided)"
+       @echo "... clean"
+       @echo "... depend"
+       @echo "... edit_cache"
+       @echo "... rebuild_cache"
+       @echo "... ucontext"
+       @echo "... arch/x86_64/getcontext.o"
+       @echo "... arch/x86_64/makecontext.o"
+       @echo "... arch/x86_64/makecontext.i"
+       @echo "... arch/x86_64/makecontext.s"
+       @echo "... arch/x86_64/setcontext.o"
+       @echo "... arch/x86_64/swapcontext.o"
+       @echo "... arch/x86_64/trampoline.o"
+       @echo "... arch/x86_64/trampoline.i"
+       @echo "... arch/x86_64/trampoline.s"
+.PHONY : help
+
+
+
+#=============================================================================
+# Special targets to cleanup operation of make.
+
+# Special rule to run CMake to check the build system integrity.
+# No rule that depends on this can have commands that come from listfiles
+# because they might be regenerated.
+cmake_check_build_system:
+       $(CMAKE_COMMAND) -S$(CMAKE_SOURCE_DIR) -B$(CMAKE_BINARY_DIR) --check-build-system CMakeFiles/Makefile.cmake 0
+.PHONY : cmake_check_build_system

-.PHONY: check dist

~
localhost:~/kangle/kasync/libucontext# git checkout -f
Your branch is up to date with 'origin/master'.
localhost:~/kangle/kasync/libucontext# mkdir build
localhost:~/kangle/kasync/libucontext# cd build
localhost:~/kangle/kasync/libucontext/build# cmake ..
-- arch is x86_64
-- include dir=/root/kangle/kasync/libucontext/include;/root/kangle/kasync/libucontext/arch/x86_64;/root/kangle/kasync/libucontext/arch/common
-- Configuring done
-- Generating done
-- Build files have been written to: /root/kangle/kasync/libucontext
localhost:~/kangle/kasync/libucontext/build# make
make: *** No targets specified and no makefile found.  Stop.
localhost:~/kangle/kasync/libucontext/build# make
make: *** No targets specified and no makefile found.  Stop.
localhost:~/kangle/kasync/libucontext/build# ls
localhost:~/kangle/kasync/libucontext/build# cd ..
localhost:~/kangle/kasync/libucontext# ls
CMakeCache.txt            LICENSE                   README.md                 build                     examples                  libucontext.pc.in         meson_options.txt
CMakeFiles                Makefile                  VERSION                   cmake_install.cmake       include                   libucontext_posix.c       test_libucontext.c
CMakeLists.txt            NEWS                      arch                      doc                       libucontext.a             meson.build               test_libucontext_posix.c
localhost:~/kangle/kasync/libucontext# ls -l
total 108
-rw-r--r--    1 root     root         15731 Dec 28 18:12 CMakeCache.txt
drwxr-xr-x    6 root     root          4096 Dec 28 18:12 CMakeFiles
-rw-r--r--    1 root     root          1411 Dec 28 18:12 CMakeLists.txt
-rw-r--r--    1 root     root           454 Dec 28 18:12 LICENSE
-rw-r--r--    1 root     root          7537 Dec 28 18:12 Makefile
-rw-r--r--    1 root     root          3683 Dec 28 18:12 NEWS
-rw-r--r--    1 root     root          5529 Dec 28 18:12 README.md
-rw-r--r--    1 root     root             3 Dec 28 18:12 VERSION
drwxr-xr-x   18 root     root          4096 Dec 28 18:12 arch
drwxr-xr-x    2 root     root          4096 Dec 28 18:12 build
-rw-r--r--    1 root     root          1632 Dec 28 18:12 cmake_install.cmake
drwxr-xr-x    2 root     root          4096 Dec 28 18:12 doc
drwxr-xr-x    2 root     root          4096 Dec 28 18:12 examples
drwxr-xr-x    3 root     root          4096 Dec 28 18:12 include
-rw-r--r--    1 root     root          7678 Dec 28 18:12 libucontext.a
-rw-r--r--    1 root     root           225 Dec 28 18:12 libucontext.pc.in
-rw-r--r--    1 root     root          1417 Dec 28 18:12 libucontext_posix.c
-rw-r--r--    1 root     root          3490 Dec 28 18:12 meson.build
-rw-r--r--    1 root     root           339 Dec 28 18:12 meson_options.txt
-rw-r--r--    1 root     root          2232 Dec 28 18:12 test_libucontext.c
-rw-r--r--    1 root     root          2097 Dec 28 18:12 test_libucontext_posix.c
localhost:~/kangle/kasync/libucontext# cd build
localhost:~/kangle/kasync/libucontext/build# cmake ..
-- arch is x86_64
-- include dir=/root/kangle/kasync/libucontext/include;/root/kangle/kasync/libucontext/arch/x86_64;/root/kangle/kasync/libucontext/arch/common
-- Configuring done
-- Generating done
-- Build files have been written to: /root/kangle/kasync/libucontext
localhost:~/kangle/kasync/libucontext/build# make
make: *** No targets specified and no makefile found.  Stop.
localhost:~/kangle/kasync/libucontext/build# make clean
make: *** No rule to make target 'clean'.  Stop.
localhost:~/kangle/kasync/libucontext/build# rm -f CMakeCache.txt
localhost:~/kangle/kasync/libucontext/build# cmake ..
-- arch is x86_64
-- include dir=/root/kangle/kasync/libucontext/include;/root/kangle/kasync/libucontext/arch/x86_64;/root/kangle/kasync/libucontext/arch/common
-- Configuring done
-- Generating done
-- Build files have been written to: /root/kangle/kasync/libucontext
localhost:~/kangle/kasync/libucontext/build# ls
localhost:~/kangle/kasync/libucontext/build# make
make: *** No targets specified and no makefile found.  Stop.
localhost:~/kangle/kasync/libucontext/build# ls
localhost:~/kangle/kasync/libucontext/build# ls
localhost:~/kangle/kasync/libucontext/build# cd ..
localhost:~/kangle/kasync/libucontext# ls
CMakeCache.txt            LICENSE                   README.md                 build                     examples                  libucontext.pc.in         meson_options.txt
CMakeFiles                Makefile                  VERSION                   cmake_install.cmake       include                   libucontext_posix.c       test_libucontext.c
CMakeLists.txt            NEWS                      arch                      doc                       libucontext.a             meson.build               test_libucontext_posix.c
localhost:~/kangle/kasync/libucontext# ls -l
total 108
-rw-r--r--    1 root     root         15731 Dec 28 18:12 CMakeCache.txt
drwxr-xr-x    6 root     root          4096 Dec 28 18:13 CMakeFiles
-rw-r--r--    1 root     root          1411 Dec 28 18:12 CMakeLists.txt
-rw-r--r--    1 root     root           454 Dec 28 18:12 LICENSE
-rw-r--r--    1 root     root          7537 Dec 28 18:13 Makefile
-rw-r--r--    1 root     root          3683 Dec 28 18:12 NEWS
-rw-r--r--    1 root     root          5529 Dec 28 18:12 README.md
-rw-r--r--    1 root     root             3 Dec 28 18:12 VERSION
drwxr-xr-x   18 root     root          4096 Dec 28 18:12 arch
drwxr-xr-x    2 root     root          4096 Dec 28 18:12 build
-rw-r--r--    1 root     root          1632 Dec 28 18:12 cmake_install.cmake
drwxr-xr-x    2 root     root          4096 Dec 28 18:12 doc
drwxr-xr-x    2 root     root          4096 Dec 28 18:12 examples
drwxr-xr-x    3 root     root          4096 Dec 28 18:12 include
-rw-r--r--    1 root     root          7678 Dec 28 18:12 libucontext.a
-rw-r--r--    1 root     root           225 Dec 28 18:12 libucontext.pc.in
-rw-r--r--    1 root     root          1417 Dec 28 18:12 libucontext_posix.c
-rw-r--r--    1 root     root          3490 Dec 28 18:12 meson.build
-rw-r--r--    1 root     root           339 Dec 28 18:12 meson_options.txt
-rw-r--r--    1 root     root          2232 Dec 28 18:12 test_libucontext.c
-rw-r--r--    1 root     root          2097 Dec 28 18:12 test_libucontext_posix.c
localhost:~/kangle/kasync/libucontext# rm -f CMakeCache.txt
localhost:~/kangle/kasync/libucontext# cd build
localhost:~/kangle/kasync/libucontext/build# cmake ..
-- The C compiler identification is GNU 12.2.1
-- The CXX compiler identification is GNU 12.2.1
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/bin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Check for working CXX compiler: /usr/bin/c++ - skipped
-- Detecting CXX compile features
-- Detecting CXX compile features - done
-- arch is x86_64
-- The ASM compiler identification is GNU
-- Found assembler: /usr/bin/cc
-- include dir=/root/kangle/kasync/libucontext/include;/root/kangle/kasync/libucontext/arch/x86_64;/root/kangle/kasync/libucontext/arch/common
-- Configuring done
-- Generating done
-- Build files have been written to: /root/kangle/kasync/libucontext/build
localhost:~/kangle/kasync/libucontext/build# ls
CMakeCache.txt       CMakeFiles           Makefile             cmake_install.cmake
localhost:~/kangle/kasync/libucontext/build# make
Scanning dependencies of target ucontext
[ 16%] Building C object CMakeFiles/ucontext.dir/arch/x86_64/makecontext.c.o
[ 33%] Building C object CMakeFiles/ucontext.dir/arch/x86_64/trampoline.c.o
[ 50%] Building ASM object CMakeFiles/ucontext.dir/arch/x86_64/getcontext.S.o
[ 66%] Building ASM object CMakeFiles/ucontext.dir/arch/x86_64/setcontext.S.o
[ 83%] Building ASM object CMakeFiles/ucontext.dir/arch/x86_64/swapcontext.S.o
[100%] Linking C static library libucontext.a
[100%] Built target ucontext
localhost:~/kangle/kasync/libucontext/build# ls
CMakeCache.txt       CMakeFiles           Makefile             cmake_install.cmake  libucontext.a
localhost:~/kangle/kasync/libucontext/build# ls -l
total 40
-rw-r--r--    1 root     root         15755 Dec 28 18:13 CMakeCache.txt
drwxr-xr-x    6 root     root          4096 Dec 28 18:14 CMakeFiles
-rw-r--r--    1 root     root          7561 Dec 28 18:13 Makefile
-rw-r--r--    1 root     root          1638 Dec 28 18:13 cmake_install.cmake
-rw-r--r--    1 root     root          7678 Dec 28 18:14 libucontext.a
localhost:~/kangle/kasync/libucontext/build# cd ..
localhost:~/kangle/kasync/libucontext# ls
CMakeFiles                LICENSE                   NEWS                      VERSION                   build                     doc                       include                   libucontext.pc.in         meson.build               test_libucontext.c
CMakeLists.txt            Makefile                  README.md                 arch                      cmake_install.cmake       examples                  libucontext.a             libucontext_posix.c       meson_options.txt         test_libucontext_posix.c
localhost:~/kangle/kasync/libucontext# cd ..
localhost:~/kangle/kasync# git pull
remote: Enumerating objects: 7, done.
remote: Counting objects: 100% (7/7), done.
remote: Compressing objects: 100% (4/4), done.
remote: Total 4 (delta 3), reused 0 (delta 0), pack-reused 0
Unpacking objects: 100% (4/4), 392 bytes | 196.00 KiB/s, done.
From https://gitee.com/keengo/kasync
   0f4f7bb..4fe1adf  main       -> origin/main
You are not currently on a branch.
Please specify which branch you want to merge with.
See git-pull(1) for details.

    git pull <remote> <branch>

localhost:~/kangle/kasync# git checkout main
Switched to branch 'main'
Your branch is behind 'origin/main' by 1 commit, and can be fast-forwarded.
  (use "git pull" to update your local branch)
localhost:~/kangle/kasync# git pull
Updating 0f4f7bb..4fe1adf
Fast-forward
 src/kudp.c | 2 ++
 1 file changed, 2 insertions(+)
localhost:~/kangle/kasync# cd ..
localhost:~/kangle# ls
CMakeLists.txt  LICENSE         README.md       build           cmake           docs            etc             extworker       include         kasync          khttpd          module          src             test            third_party     webadmin
localhost:~/kangle# cd khttpd/
localhost:~/kangle/khttpd# git pull
remote: Enumerating objects: 11, done.
remote: Counting objects: 100% (11/11), done.
remote: Compressing objects: 100% (6/6), done.
remote: Total 6 (delta 4), reused 0 (delta 0), pack-reused 0
Unpacking objects: 100% (6/6), 537 bytes | 179.00 KiB/s, done.
From https://gitee.com/keengo/khttpd
   aa13717..9be4028  master     -> origin/master
You are not currently on a branch.
Please specify which branch you want to merge with.
See git-pull(1) for details.

    git pull <remote> <branch>

localhost:~/kangle/khttpd# git checkout master
Switched to branch 'master'
Your branch is behind 'origin/master' by 1 commit, and can be fast-forwarded.
  (use "git pull" to update your local branch)
localhost:~/kangle/khttpd# git pull
Updating aa13717..9be4028
Fast-forward
 example/server.cpp | 6 ++++++
 src/KHttp3.cpp     | 2 +-
 2 files changed, 7 insertions(+), 1 deletion(-)
localhost:~/kangle/khttpd# git pull
Already up to date.
localhost:~/kangle/khttpd# ls
CMakeLists.txt  LICENSE         README.md       davlib          example         include         src
localhost:~/kangle/khttpd# cd kasy
-ash: cd: can't cd to kasy: No such file or directory
localhost:~/kangle/khttpd# cd ../kasync/
localhost:~/kangle/kasync# ls
CMakeLists.txt  LICENSE         README.md       example         include         libucontext     src             test
localhost:~/kangle/kasync# vim CMakeLists.txt
-ash: vim: not found
localhost:~/kangle/kasync# vi CMakeLists.txt
localhost:~/kangle/kasync# apk add vim
(1/2) Installing xxd (9.0.0999-r0)
(2/2) Installing vim (9.0.0999-r0)
Executing busybox-1.35.0-r29.trigger
OK: 509 MiB in 100 packages
localhost:~/kangle/kasync# vim CMakeLists.txt
localhost:~/kangle/kasync# vim CMakeLists.txt
localhost:~/kangle/kasync# vim CMakeLists.txt
localhost:~/kangle/kasync# cd ../build/
localhost:~/kangle/build# cmake ..
CMake Warning (dev) at kasync/CMakeLists.txt:49:
  Syntax Warning in cmake code at column 5

  Argument not separated from preceding token by whitespace.
This warning is for project developers.  Use -Wno-dev to suppress it.

-- system is Linux
-- OPENSSL_DIR is
-- Could NOT find OpenSSL, try to set the path to OpenSSL root folder in the system variable OPENSSL_ROOT_DIR (mis                                                                                                                                                              sing: OPENSSL_CRYPTO_LIBRARY OPENSSL_INCLUDE_DIR)
-- cann't find openssl
-- Performing Test ,HAVE_UCONTEXT
-- Performing Test ,HAVE_UCONTEXT - Failed
FATALNOT FOUND ucontext
-- /root/kangle/kasync/include
-- build as static libary
-- KHTTP_LIBS pthread
-- Could NOT find SQLite3 (missing: SQLite3_INCLUDE_DIR SQLite3_LIBRARY)
-- kangle version 3.6.0
-- Configuring done
-- Generating done
-- Build files have been written to: /root/kangle/build
localhost:~/kangle/build# vim ../kasync/CMakeLists.txt
localhost:~/kangle/build# cmake ..
CMake Warning (dev) at kasync/CMakeLists.txt:49:
  Syntax Warning in cmake code at column 5

  Argument not separated from preceding token by whitespace.
This warning is for project developers.  Use -Wno-dev to suppress it.

-- system is Linux
-- OPENSSL_DIR is
-- Could NOT find OpenSSL, try to set the path to OpenSSL root folder in the system variable OPENSSL_ROOT_DIR (mis                                                                                                                                                              sing: OPENSSL_CRYPTO_LIBRARY OPENSSL_INCLUDE_DIR)
-- cann't find openssl
CMake Error at kasync/CMakeLists.txt:51 (message):
  NOT FOUND ucontext


-- Configuring incomplete, errors occurred!
See also "/root/kangle/build/CMakeFiles/CMakeOutput.log".
See also "/root/kangle/build/CMakeFiles/CMakeError.log".
localhost:~/kangle/build# vim ../kasync/CMakeLists.txt
localhost:~/kangle/build# vim ../kasync/CMakeLists.txt
cmake_minimum_required(VERSION 2.8.12)
# set the project name
project(kasync)
set(KASYNC_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
file(GLOB KASYNC_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)
file(GLOB KASYNC_INCLUDE_FILES ${KASYNC_INCLUDE_DIR}/*.h)
message(STATUS "system is ${CMAKE_SYSTEM_NAME}")
#message(STATUS "current source dir is ${CMAKE_CURRENT_SOURCE_DIR}")
#message(STATUS "current binary dir is ${CMAKE_CURRENT_BINARY_DIR}")
option(KASYNC_TEST "build for test" OFF)
option(KSOCKET_SSL "enable tls/ssl support" ON)
option(KSOCKET_SSL_BIO "enable tls/ssl bio support")
#option(OPENSSL_DIR "set openssl dir")
include(CheckIncludeFile)
include(CheckSymbolExists)

if (KSOCKET_SSL)
    message(STATUS "OPENSSL_DIR is ${OPENSSL_DIR}")
    if (NOT OPENSSL_DIR)
            include(FindOpenSSL)
            if (NOT OPENSSL_FOUND)
                        message(STATUS "cann't find openssl")
                        set(KSOCKET_SSL OFF)
            else()
                        message(STATUS "build with tls/ssl support ${OPENSSL_INCLUDE_DIR}")
                        list(APPEND KASYNC_INCLUDE_DIR ${OPENSSL_INCLUDE_DIR})
                        list(APPEND KASYNC_LIBS  OpenSSL::SSL OpenSSL::Crypto)
                        set_target_properties(OpenSSL::SSL OpenSSL::Crypto PROPERTIES IMPORTED_GLOBAL True)
            endif()
    else()
                list(APPEND KASYNC_INCLUDE_DIR ${OPENSSL_DIR}/include)
                list(APPEND KASYNC_LIBS ssl crypto)
    endif()
else()
    unset(KSOCKET_SSL_BIO)
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Windows")
        list(APPEND KASYNC_LIBS ws2_32)
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_BINARY_DIR})
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_BINARY_DIR})
else()
    include(CheckCSourceCompiles)
    check_c_source_compiles(
        "#include<ucontext.h>
        int main(){
                ucontext_h ucp;
                getcontext(&ucp);
                }",HAVE_UCONTEXT)
    if (NOT HAVE_UCONTEXT)
            message(FATAL_ERROR "NOT FOUND ucontext")
        endif()
    check_symbol_exists(accept4 sys/socket.h HAVE_ACCEPT4)
    check_symbol_exists(memalign malloc.h HAVE_MEMALIGN)
    check_symbol_exists(setmntent mntent.h HAVE_SETMNTENT)
endif()
#message(STATUS "cmake_system_name is ${CMAKE_SYSTEM_NAME}")
if (CMAKE_SYSTEM_NAME MATCHES "Linux" OR CMAKE_SYSTEM_NAME MATCHES "Android")
    list(APPEND CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
    set(LINUX ON)
    option(LINUX_IOURING "enable io_uring")
    if (LINUX_IOURING)
        message(STATUS "linux use io_uring")
        list(APPEND KASYNC_LIBS uring)
    endif()
    if (CMAKE_SYSTEM_NAME MATCHES "Linux")
            list(APPEND KASYNC_LIBS pthread)
        endif()
elseif (CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
        set(FREEBSD ON)
    list(APPEND KASYNC_LIBS pthread)
elseif (CMAKE_SYSTEM_NAME MATCHES "Darwin")
        set(DARWIN ON)
    set(ENABLE_LIBUCONTEXT ON)
    set(LIBUCONTEXT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libucontext)
    set(LIBUCONTEXT_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/libucontext/libucontext.a)
    add_custom_target(libucontext.a ALL
        COMMAND ${CMAKE_MAKE_PROGRAM} FREESTANDING=yes libucontext.a
        WORKING_DIRECTORY ${LIBUCONTEXT_DIR}
        COMMENT "Original metis makefile target")
    add_library(ucontext STATIC IMPORTED GLOBAL)
-- VISUAL --                                                                                                                                                                                                                                        10        43,2          Top
