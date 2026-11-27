#!/usr/bin/env python3
"""Targeted ACL/filter/API/WHM/WebDAV tests with isolated config."""
import concurrent.futures
import hashlib
import http.client
import json
import os
import shutil
import signal
import socket
import socketserver
import struct
import subprocess
import sys
import tempfile
import time
import urllib.parse
from xml.sax.saxutils import escape
from upstream_functional_regression import exact, free_port, start_server, HttpOrigin, fcgi_record
from runtime_fixture import exe_name, module_name, stop_server


class MapOrigin(socketserver.BaseRequestHandler):
    def handle(self):
        self.request.settimeout(10)
        def packet():
            _, kind, _, length, padding, _ = struct.unpack('!BBHHBB', exact(self.request, 8))
            data = exact(self.request, length)
            exact(self.request, padding)
            return kind, data
        while True:
            kind, data = packet()
            if kind == 5 and not data:
                break
        def mapped(path):
            record = fcgi_record(108, path.encode())
            self.request.sendall(record[:11])
            time.sleep(0.05)
            self.request.sendall(record[11:])
            kind, result = packet()
            if kind != 154 or not result.replace(b'\\', b'/').endswith(path.encode()):
                raise AssertionError((kind, result, path))
        mapped('/map/before-complete-packet')
        self.request.sendall(fcgi_record(6, b'Status: 200 OK\r\nContent-Length: 9\r\n\r\nfirst'))
        mapped('/map/after-complete-packet')
        self.request.sendall(fcgi_record(6, b'last') + fcgi_record(6, b'') + fcgi_record(3, b'\0' * 8))


def main():
    build = os.path.dirname(os.path.abspath(sys.argv[1]))
    root = tempfile.mkdtemp(prefix='kangle-access-regression-').replace('\\', '/')
    os.chmod(root, 0o755)
    proc = None
    checks = 0
    servers = []
    def check(ok, label):
        nonlocal checks
        if not ok:
            raise AssertionError(label + '; artifacts: ' + root)
        checks += 1
        print('PASS:', label, flush=True)
    try:
        for name in ['bin', 'etc', 'ext', 'var', 'tmp', 'www', 'www/api', 'www/dav']:
            os.makedirs(root + '/' + name, exist_ok=True)
        for name in [exe_name('kangle'), exe_name('extworker'), module_name('webdav'), module_name('api_functional_fixture')]:
            shutil.copy2(build + '/' + name, root + '/bin/' + name)
        with open(root + '/www/ready', 'w') as f:
            f.write('ready')
        # A child writes more than a pipe capacity before reading stdin.
        with open(root + '/bin/duplex.py', 'w') as f:
            f.write("import sys, hashlib\nsys.stdout.write('x'*262144)\nsys.stdout.flush()\ndata=sys.stdin.buffer.read()\nprint('DONE:'+str(len(data))+':'+hashlib.md5(data).hexdigest())\n")
        command = escape(sys.executable + ' ' + root + '/bin/duplex.py')
        with open(root + '/www/shell.whm', 'w') as f:
            f.write("<package><extend name='sync' type='shell' async='0'><commands runas='system' curdir='" + root + "/www'><command>" + command + "</command></commands></extend>"
                    "<extend name='async' type='shell' async='1'><include extend='sync'/></extend>"
                    "<call name='sync' extend='sync' scope='public'/><call name='async' extend='async' scope='public'/></package>")
        origin = start_server(HttpOrigin)
        mapping = start_server(MapOrigin)
        servers.extend([origin, mapping])
        port = free_port()
        cfg = """<config><worker_thread>2</worker_thread><timeout rw='10' connect='2'/>
<listen ip='127.0.0.1' port='{port}' type='http'/><cache default='0' disk='0'/>
<vhs><mime_type ext='*' type='text/plain'/></vhs><response action='allow'/>
<server name='http' host='127.0.0.1' port='{http}' proto='http'/>
<server name='mapping' host='127.0.0.1' port='{mapping}' proto='fastcgi'/>
<api name='fixture-sp' file='bin/api_functional_fixture.so' type='sp' idle_time='0' life_time='30'/>
<api name='fixture-mt' file='bin/api_functional_fixture.so' type='mt'/>
<api name='dav' file='bin/webdav.so' type='sp' idle_time='0' life_time='30'/>
<api name='whm' file='buildin:whm'/>
<request action='allow'><table name='BEGIN'>
<chain action='deny'><acl_path path='/filter'/><mark_param name='^blocked$' value='^yes$' get='1' post='1'/></chain>
<chain action='continue'><acl_path path='/headers'/><mark_remove_header attr='X-Remove'/><mark_replace_header attr='X-Append' replace='new'/></chain>
<chain action='continue'><acl_path path='/replace-ip'/><mark_replace_ip header='X-Real-IP'/><mark_replace_header attr='X-Append' replace='new'/></chain>
</table></request>
<vh name='sp' doc_root='www' inherit='on'><host>localhost</host>
<map path='/api' extend='api:fixture-sp' confirm_file='0' allow_method='*'/>
<map path='/map' extend='server:mapping' confirm_file='0' allow_method='*'/>
<map path='/dav' extend='api:dav' confirm_file='0' allow_method='*'/>
<map path='/shell.whm' extend='api:whm' confirm_file='1' allow_method='*'/>
<map path='/filter' extend='server:http' confirm_file='0' allow_method='*'/>
<map path='/headers' extend='server:http' confirm_file='0' allow_method='*'/>
<map path='/replace-ip' extend='server:http' confirm_file='0' allow_method='*'/>
</vh>
<vh name='mt' doc_root='www' inherit='on'><host>mt.test</host><map path='/api' extend='api:fixture-mt' confirm_file='0' allow_method='*'/></vh>
</config>""".format(port=port, http=origin.server_address[1], mapping=mapping.server_address[1])
        cfg = cfg.replace('bin/api_functional_fixture.so', 'bin/' + module_name('api_functional_fixture'))
        cfg = cfg.replace('bin/webdav.so', 'bin/' + module_name('webdav'))
        with open(root + '/etc/config.xml', 'w') as f:
            f.write(cfg)
        log = open(root + '/console.log', 'wb')
        executable = root + '/bin/' + exe_name('kangle')
        proc = subprocess.Popen([executable, '-n', '-g'], stdout=log, stderr=log)
        def request(path, method='GET', body=None, headers=None, host='localhost'):
            cn = http.client.HTTPConnection('127.0.0.1', port, timeout=15)
            cn.request(method, path, body=body, headers=dict({'Host': host}, **(headers or {})))
            resp = cn.getresponse()
            result = resp.status, dict(resp.getheaders()), resp.read()
            cn.close()
            return result
        for _ in range(100):
            try:
                if request('/ready')[0] == 200:
                    break
            except (OSError, http.client.HTTPException):
                time.sleep(0.05)
        else:
            raise AssertionError('startup failed; ' + root)
        for host in ['localhost', 'mt.test']:
            for mode in ['packed', 'split']:
                code, headers, data = request('/api/test?' + mode, host=host)
                check(code == 200 and data == mode.encode(), host + ' API headers/body ' + mode)
                check(not ('Content-Length' in headers and 'Transfer-Encoding' in headers), host + ' API response framing unambiguous')
            body = b'0123456789abcdef' * 8192
            for mode in ['map-before', 'map-after']:
                code, _, data = request('/api/test?' + mode, 'POST', body, host=host)
                check(code == 200 and data == (b'prefix:' if mode == 'map-after' else b'') + body,
                      host + ' MAP_PATH preserves request body ' + mode)
        check(request('/map/start')[2] == b'firstlast', 'fragmented MAP_PATH before and after response headers')
        for suffix, name in [('/headers', 'X-Remove'), ('/replace-ip', 'X-Real-IP')]:
            # Send the removable header last, so the tail path is exercised.
            with socket.create_connection(('127.0.0.1', port), timeout=10) as sock:
                sock.sendall(('GET ' + suffix + ' HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n' + name + ': 127.0.0.2\r\n\r\n').encode())
                data = b''
                while True:
                    part = sock.recv(4096)
                    if not part: break
                    data += part
            check(data.startswith(b'HTTP/1.1 200'), suffix + ' tail removal followed by header insertion')
        for content_type in ['application/x-www-form-urlencoded', 'application/x-www-form-urlencoded; x=' + 'a'*350]:
            check(request('/filter', 'POST', b'safe=ok\0&blocked=yes', {'Content-Type': content_type})[0] == 403,
                  'urlencoded NUL and Content-Type length=' + str(len(content_type)))
            check(request('/filter', 'POST', b'safe=yes&empty', {'Content-Type': content_type})[0] == 200,
                  'ordinary allowed urlencoded parameters')
        boundary = 'split-boundary-123456'
        body = ('--'+boundary+'\r\nContent-Disposition: form-data; name="upload"; filename="ok.txt"\r\n\r\n'+ 'x'*100 + '\r\n--'+boundary+'\r\nContent-Disposition: form-data; name="blocked"\r\n\r\nyes\r\n--'+boundary+'--\r\n').encode()
        split = body.index(b'\n--'+boundary.encode(), 80)
        # Force every position in the second boundary across HTTP chunk reads.
        for offset in range(len(boundary) + 3):
            at = split + offset
            with socket.create_connection(('127.0.0.1', port), timeout=10) as sock:
                sock.sendall(('POST /filter HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\nTransfer-Encoding: chunked\r\nContent-Type: multipart/form-data; boundary='+boundary+'\r\n\r\n').encode())
                for part in [body[:at], body[at:]]:
                    sock.sendall(('%x\r\n'%len(part)).encode()+part+b'\r\n')
                    time.sleep(0.015)
                sock.sendall(b'0\r\n\r\n')
                data = sock.recv(4096)
            check(data.startswith(b'HTTP/1.1 403'), 'multipart split boundary position %d'%offset)
        content_type = 'multipart/form-data; padding=' + 'a'*350 + '; boundary='+boundary
        check(request('/filter', 'POST', body, {'Content-Type': content_type})[0] == 403, 'long multipart Content-Type still filtered')
        check(request('/dav/source', 'PUT', b'dav-data')[0] == 201, 'WebDAV source PUT')
        for method, destination in [('COPY', '/dav/copied%20file'), ('MOVE', 'http://localhost/dav/moved%20file')]:
            code, _, _ = request('/dav/source' if method == 'COPY' else '/dav/copied%20file', method, headers={'Destination': destination})
            filename = 'copied file' if method == 'COPY' else 'moved file'
            check(code == 201 and open(root+'/www/dav/'+filename, 'rb').read() == b'dav-data', 'WebDAV '+method+' decodes spaces')
        code, _, _ = request('/dav/source', 'COPY', headers={'Destination': '/dav/%2e%2e/%2e%2e/escape'})
        check(code in [201, 400, 403] and not os.path.exists(root+'/escape'), 'encoded traversal cannot leave document root')
        check(request('/dav/source', 'COPY', headers={'Destination': '/dav/truncated%00file'})[0] == 400,
              'WebDAV rejects decoded NUL rather than truncating file name')
        input_data = b'z'*262144
        form = urllib.parse.urlencode({'-': input_data.decode()}).encode()
        digest = hashlib.md5(input_data).hexdigest()
        def whm(call, body=None):
            return json.loads(request('/shell.whm?format=json&' + call, 'POST' if body is not None else 'GET', body,
                                      {'Content-Type': 'application/x-www-form-urlencoded'} if body else None)[2].decode())
        result = whm('whm_call=sync', form)
        output = result['result']['out']
        if isinstance(output, list): output = output[0]
        check(result['status'] == '200' and output.rstrip('\r\n').endswith('DONE:262144:'+digest), 'WHM large duplex stdin/stdout does not deadlock')
        for _ in range(12):
            result = whm('whm_call=async', form)
            session = result['result']['session']
            if isinstance(session, list): session = session[0]
            completed = False
            output = ''
            for attempt in range(100):
                with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:
                    results = list(executor.map(lambda x: whm('whm_call=query&session='+urllib.parse.quote(session)), range(4)))
                for item in results:
                    part = item.get('result', {}).get('out', '')
                    output += part[0] if isinstance(part, list) else part
                    completed = completed or item['status'] == '200'
                if completed: break
                time.sleep(0.02)
            check(completed and 'DONE:262144:'+digest in output, 'WHM async concurrent query lifecycle')
        check(proc.poll() is None and request('/ready')[0] == 200, 'service survives targeted tests')
        print('SUCCESS: %d checks; artifacts: %s' % (checks, root), flush=True)
    finally:
        if proc:
            stop_server(proc, executable)
        for server in servers:
            server.shutdown()
            server.server_close()


if __name__ == '__main__':
    main()
