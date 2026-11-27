#!/usr/bin/env python3
"""Check the geo mark: lookups, rule edits, file changes and url downloads.

The client address comes from X-Real-Ip (replace_ip mark). Each geo label
found for it is turned into a response header X-Geo-<label> by later chains.

1. Ranges, single addresses, overlapping ranges, multi-label groups, IPv6
   (CIDR, min-max ranges across 32-bit word borders, other spellings).
   Overlapping ranges are resolved in file order (a later range wins
   inside an earlier one).
2. Editing the geo rule keeps the data (no gap while the rule is replaced).
3. Editing the rule after the file changed: the old data stays until the
   new file is loaded, then the new file is used. This file has no
   overlapping ranges, which loads without the range tree, and CRLF line
   ends.
4. A rule with url and a missing file downloads the file from the url and
   loads it; a cut-off download does not replace the file; a later
   download with new content is loaded.

Usage: python3 test/geo_mark_regression.py /absolute/path/to/kangle
Uses an isolated prefix and does not change the installed configuration.
"""
import http.client
import http.server
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time
from runtime_fixture import copy_server, stop_server

LABELS = 'ABCDEFGHIJKNPQ'
GEO = '''*A
10.0.0.0/8
*B
10.1.0.0/16
*C*D
192.168.1.10-192.168.1.20
*E
2001:db8::/32
*F
1.2.3.4
*I
2001:db8:0:1::-2001:db8:0:1:ffff:ffff:ffff:ffff
*J
2001:db8:0:1::1:0/112
*K
2001:db8:1:0:ffff:ffff:ffff:fff0-2001:db8:1:1::f
*N
2001:db9:0:0:0:0:ffff:ffff
'''
# same answers as GEO for CASES with B renamed to G, but no ranges overlap
# and the lines are not in address order; written with CRLF line ends
GEO_SPLIT = '''*A
10.0.0.0/16
10.2.0.0/15
10.4.0.0/14
10.8.0.0/13
10.16.0.0/12
10.32.0.0/11
10.64.0.0/10
10.128.0.0/9
*E
2001:db8::/32
*G
10.1.0.0/16
*C*D
192.168.1.10-192.168.1.20
*F
1.2.3.4
*K
2001:db9:0:0:ffff:ffff:ffff:fff0-2001:db9:0:1::f
'''
CASES = [
    ('10.2.3.4', 'A'),
    ('10.255.255.255', 'A'),
    ('10.1.2.3', 'B'),
    ('10.0.255.255', 'A'),
    ('10.2.0.0', 'A'),
    ('192.168.1.10', 'CD'),
    ('192.168.1.20', 'CD'),
    ('192.168.1.21', ''),
    ('192.168.1.9', ''),
    ('1.2.3.4', 'F'),
    ('1.2.3.5', ''),
    ('2001:db8::1', 'E'),
    ('2001:db9::1', ''),
    ('11.0.0.1', ''),
]
CASES6 = [
    ('2001:db8:0:1::5', 'I'),
    ('2001:db8:0:1::', 'I'),
    ('2001:db8:0:0:ffff:ffff:ffff:ffff', 'E'),
    ('2001:db8:0:1:ffff:ffff:ffff:ffff', 'I'),
    ('2001:db8:0:2::', 'E'),
    ('2001:db8:0:1::1:0', 'J'),
    ('2001:db8:0:1::1:ffff', 'J'),
    ('2001:db8:0:1::2:0', 'I'),
    ('2001:db8:0:1::0:ffff', 'I'),
    ('2001:db8:1:0:ffff:ffff:ffff:ffef', 'E'),
    ('2001:db8:1:0:ffff:ffff:ffff:fff0', 'K'),
    ('2001:db8:1:1::', 'K'),
    ('2001:db8:1:1::f', 'K'),
    ('2001:db8:1:1::10', 'E'),
    ('2001:db8:ffff:ffff:ffff:ffff:ffff:ffff', 'E'),
    ('2001:db9::ffff:ffff', 'N'),
    ('2001:db9::1:0:0', ''),
    ('2001:db9::ffff:fffe', ''),
    ('2001:0DB8:0000:0001:0000:0000:0001:0005', 'J'),
    ('[2001:db8:0:1::1:5]', 'J'),
    ('2001:db7:ffff:ffff:ffff:ffff:ffff:ffff', ''),
    ('::1', ''),
]
CASES_SPLIT6 = [
    ('2001:db9:0:0:ffff:ffff:ffff:ffef', ''),
    ('2001:db9:0:0:ffff:ffff:ffff:fff0', 'K'),
    ('2001:db9:0:1::f', 'K'),
    ('2001:db9:0:1::10', ''),
    ('2001:db8:0:1::1:5', 'E'),
]


def free_port():
    s = socket.socket()
    s.bind(('127.0.0.1', 0))
    port = s.getsockname()[1]
    s.close()
    return port


def labels_for(port, ip):
    conn = http.client.HTTPConnection('127.0.0.1', port, timeout=5)
    try:
        conn.request('GET', '/t.txt', headers={'X-Real-Ip': ip})
        response = conn.getresponse()
        response.read()
        assert response.status == 200, (ip, response.status)
        return ''.join(l for l in LABELS if response.getheader('X-Geo-' + l))
    finally:
        conn.close()


def check_cases(port, cases, what):
    for ip, expected in cases:
        got = labels_for(port, ip)
        assert got == expected, '%s: %s got [%s] expected [%s]' % (what, ip, got, expected)
    print('PASS: %s (%d addresses)' % (what, len(cases)), flush=True)


def wait_for(port, ip, expected, what, timeout=15):
    deadline = time.time() + timeout
    got = None
    while time.time() < deadline:
        got = labels_for(port, ip)
        if got == expected:
            print('PASS: %s' % what, flush=True)
            return
        time.sleep(.1)
    raise AssertionError('%s: %s got [%s] expected [%s]' % (what, ip, got, expected))


CONFIG_WRITES = [0]


def write_config(root, port, admin, geo_attrs):
    chains = ''.join(
        "<chain action='continue'><acl module='header' header='_geo' val='^%s$'/>"
        "<mark module='add_response_header' attr='X-Geo-%s' val='1'/></chain>" % (l, l)
        for l in LABELS)
    cfg = '''<config><worker_thread>2</worker_thread>
<listen ip="127.0.0.1" port="%d" type="http"/>
<listen ip="127.0.0.1" port="%d" type="manage"/>
<admin user="probe" password="probe" admin_ips="127.0.0.1"/>
<api name="whm" file="buildin:whm" type="mt"/>
<cache default="0" memory="0" disk="0"/>
<request action="allow"><table name="BEGIN">
<chain action="continue"><mark module="replace_ip" header="X-Real-Ip"/></chain>
<chain action="continue"><mark module="geo" name="_geo" %s/></chain>
%s
</table></request><response action="allow"/>
<vhs><mime_type ext="*" type="text/plain"/></vhs>
<vh name="t" doc_root="www" inherit="on"><host>*</host></vh>
</config>''' % (port, admin, geo_attrs, chains)
    with open(root + '/etc/config.xml', 'w') as f:
        f.write(cfg)
    # reload skips files whose mtime did not change
    CONFIG_WRITES[0] += 1
    stamp = time.time() + CONFIG_WRITES[0] * 2
    os.utime(root + '/etc/config.xml', (stamp, stamp))


def reload(admin):
    conn = http.client.HTTPConnection('127.0.0.1', admin, timeout=10)
    try:
        conn.request('GET', '/core.whm?whm_call=reload',
                     headers={'Authorization': 'Basic cHJvYmU6cHJvYmU='})
        response = conn.getresponse()
        body = response.read()
        assert response.status == 200 and b'status="200"' in body, (response.status, body[:200])
    finally:
        conn.close()


class GeoOrigin(http.server.BaseHTTPRequestHandler):
    """Serves the geo file for url downloads; cut sends less than its
    Content-Length and closes the connection."""
    body = b''
    cut = False
    hits = 0

    def do_GET(self):
        cls = type(self)
        cls.hits += 1
        self.send_response(200)
        self.send_header('Content-Length', str(len(cls.body) + (100 if cls.cut else 0)))
        self.end_headers()
        self.wfile.write(cls.body)
        self.close_connection = True

    def log_message(self, *args):
        pass


def wait_hits(count, what, timeout=15):
    deadline = time.time() + timeout
    while GeoOrigin.hits < count:
        assert time.time() < deadline, '%s: %d downloads, expected %d' % (what, GeoOrigin.hits, count)
        time.sleep(.1)


def run(binary):
    root = tempfile.mkdtemp(prefix='kangle-geo-')
    origin = http.server.HTTPServer(('127.0.0.1', 0), GeoOrigin)
    threading.Thread(target=origin.serve_forever, daemon=True).start()
    proc = None
    executable = None
    try:
        for name in ('etc', 'ext', 'bin', 'var', 'tmp', 'log', 'www', 'webadmin'):
            os.makedirs(root + '/' + name)
        with open(root + '/webadmin/core.whm', 'w') as f:
            f.write('<whm version="1.0"><extend name="core" type="core"/>'
                    '<call name="reload" extend="core"/></whm>')
        with open(root + '/www/t.txt', 'w') as f:
            f.write('ok')
        with open(root + '/etc/geo.txt', 'wb') as f:
            f.write(GEO.encode())
        port, admin = free_port(), free_port()
        executable = copy_server(binary, root + '/bin')
        write_config(root, port, admin, 'file="etc/geo.txt" flush_time="86400"')
        with open(root + '/console.log', 'wb') as log:
            proc = subprocess.Popen([executable, '-n', '-g'], stdout=log, stderr=log)
        for _ in range(100):
            try:
                socket.create_connection(('127.0.0.1', port), timeout=.1).close()
                break
            except OSError:
                assert proc.poll() is None, 'server exited: ' + root
                time.sleep(.05)
        else:
            raise AssertionError('server did not start: ' + root)

        check_cases(port, CASES, 'lookups after startup')
        check_cases(port, CASES6, 'IPv6 lookups after startup')

        write_config(root, port, admin, 'file="etc/geo.txt" flush_time="86401"')
        reload(admin)
        check_cases(port, CASES + CASES6, 'lookups right after the rule was edited')

        with open(root + '/etc/geo.txt', 'wb') as f:
            f.write(GEO_SPLIT.replace('\n', '\r\n').encode())
        future = time.time() + 100
        os.utime(root + '/etc/geo.txt', (future, future))
        write_config(root, port, admin, 'file="etc/geo.txt" flush_time="86402"')
        reload(admin)
        wait_for(port, '10.1.2.3', 'G', 'changed file is used after a rule edit')
        check_cases(port, [(ip, l.replace('B', 'G')) for ip, l in CASES] + CASES_SPLIT6,
                    'lookups in the changed file')

        url = 'http://127.0.0.1:%d/geo.txt' % origin.server_port
        GeoOrigin.body = b'*H\n172.16.0.0/12\n2001:db8:aaaa::/48\n'
        write_config(root, port, admin, 'file="etc/geo_dl.txt" url="%s" flush_time="1"' % url)
        reload(admin)
        wait_for(port, '172.20.1.1', 'H', 'downloaded file is loaded')
        check_cases(port, [('2001:db8:aaaa:1::1', 'H'), ('10.2.3.4', '')], 'lookups in the downloaded file')
        with open(root + '/etc/geo_dl.txt', 'rb') as f:
            assert f.read() == GeoOrigin.body, 'downloaded file differs'

        GeoOrigin.cut = True
        GeoOrigin.body = b'*Q\n172.16.0.0/12\n'
        hits = GeoOrigin.hits
        wait_hits(hits + 2, 'cut-off downloads')
        time.sleep(.5)
        check_cases(port, [('172.20.1.1', 'H')], 'a cut-off download keeps the old data')
        with open(root + '/etc/geo_dl.txt', 'rb') as f:
            assert f.read().startswith(b'*H\n'), 'a cut-off download replaced the file'

        GeoOrigin.cut = False
        GeoOrigin.body = b'*P\n172.16.0.0/12\n'
        wait_for(port, '172.20.1.1', 'P', 'a later download with new content is loaded')
        print('geo mark regression passed', flush=True)
    except Exception:
        try:
            with open(root + '/console.log', 'rb') as f:
                print(f.read()[-4000:].decode('utf-8', 'replace'))
        except OSError:
            pass
        raise
    finally:
        if proc is not None:
            stop_server(proc, executable)
        origin.shutdown()
        origin.server_close()
        shutil.rmtree(root, ignore_errors=True)


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(2)
    run(os.path.abspath(sys.argv[1]))
