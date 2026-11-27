#!/usr/bin/env python3
"""Targeted upstream regressions; run on Linux with a freshly built kangle.

Usage: python3 test/upstream_functional_regression.py /path/to/build/kangle
Uses an isolated configuration and ephemeral loopback ports, not the live config.
"""
import http.client
import json
import os
import select
import shutil
import signal
import socket
import socketserver
import struct
import subprocess
import sys
import tempfile
import threading
import time
from xml.sax.saxutils import quoteattr


def exact(sock, size):
    result = b''
    while len(result) < size:
        part = sock.recv(size - len(result))
        if not part:
            raise EOFError()
        result += part
    return result


def free_port():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True

    def handle_error(self, request, address):
        # Peers deliberately close during no-body and failure tests.
        pass


def start_server(handler, port=0, label='ok'):
    server = Server(('127.0.0.1', port), handler)
    server.label = label
    server.failed_posts = 0
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server


class HttpOrigin(socketserver.StreamRequestHandler):
    def handle(self):
        self.request.settimeout(5)
        while True:
            line = self.rfile.readline()
            if not line:
                return
            method, path, _ = line.decode().strip().split(' ', 2)
            headers = {}
            while True:
                line = self.rfile.readline()
                if line == b'\r\n':
                    break
                if not line:
                    return
                key, val = line.decode().split(':', 1)
                headers[key.lower()] = val.strip()
            count = int(path.rsplit('/', 1)[-1]) if path.startswith('/interim/') else 0
            if count:
                packet = b''.join(b'HTTP/1.1 103 Early Hints\r\nX-Interim: secret\r\n\r\n' for _ in range(count))
            else:
                packet = b''
            premature = False
            if path == '/partial-post' and method == 'POST':
                self.server.failed_posts += 1
                if self.server.failed_posts == 1:
                    self.rfile.read(4096)
                    self.request.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', 1, 0))
                    return
            if path == '/expect':
                self.request.sendall(b'HTTP/1.1 103 Early Hints\r\n\r\n')
                premature = bool(select.select([self.request], [], [], 0.2)[0])
                self.request.sendall(b'HTTP/1.1 100 Continue\r\n\r\n')
            body = self.rfile.read(int(headers.get('content-length', '0')))
            content = body if method == 'POST' else self.server.label.encode()
            if premature:
                content = b'premature-body-after-103'
            packet += ('HTTP/1.1 200 OK\r\nCache-Control: no-store\r\nContent-Length: %d\r\n\r\n' % len(content)).encode() + content
            self.request.sendall(packet)


def fcgi_record(kind, data):
    return struct.pack('!BBHHBB', 1, kind, 1, len(data), 0, 0) + data


def fcgi_params(data):
    result = {}
    pos = 0
    while pos < len(data):
        lengths = []
        for _ in range(2):
            length = data[pos]
            pos += 1
            if length & 128:
                length = ((length & 127) << 24) | int.from_bytes(data[pos:pos + 3], 'big')
                pos += 3
            lengths.append(length)
        n, v = lengths
        result[data[pos:pos + n].decode()] = data[pos + n:pos + n + v].decode()
        pos += n + v
    return result


class FastcgiOrigin(socketserver.BaseRequestHandler):
    def handle(self):
        self.request.settimeout(10)
        while True:
            params = b''
            while True:
                try:
                    head = exact(self.request, 8)
                except EOFError:
                    return
                _, kind, _, length, padding, _ = struct.unpack('!BBHHBB', head)
                data = exact(self.request, length)
                exact(self.request, padding)
                if kind == 4:
                    params += data
                if kind == 5 and length == 0:
                    break
            env = fcgi_params(params)
            path = env.get('REQUEST_URI', '')
            code = 204 if path.endswith('/204') else 304 if path.endswith('/304') else 200
            payload = json.dumps({'pid': os.getpid(), 'uid': os.getuid(), 'gid': os.getgid(), 'groups': os.getgroups()}).encode()
            headers = ('Status: %d OK\r\nCache-Control: no-store\r\nContent-Length: %d\r\n\r\n' % (code, len(payload))).encode()
            if path.endswith('/sendfile'):
                headers = b'Status: 200 OK\r\nCache-Control: no-store\r\nX-Accel-Redirect: /static/file.txt\r\n\r\n'
            self.request.sendall(fcgi_record(6, headers))
            # Illegal data on HEAD/204/304, after headers, must be discarded safely.
            time.sleep(0.03)
            self.request.sendall(fcgi_record(6, payload))
            if path.endswith('/abort'):
                return  # Missing END_REQUEST must never make this socket reusable.
            self.request.sendall(fcgi_record(6, b'') + fcgi_record(3, b'\0' * 8))


def ajp_string(data):
    return struct.pack('!H', len(data)) + data + b'\0'


def ajp_record(data):
    return b'AB' + struct.pack('!H', len(data)) + data


class AjpOrigin(socketserver.BaseRequestHandler):
    def handle(self):
        self.request.settimeout(10)
        identity = str(self.request.getpeername()[1]).encode()
        while True:
            try:
                head = exact(self.request, 4)
            except EOFError:
                return
            data = exact(self.request, struct.unpack('!H', head[2:])[0])
            if not data or data[0] != 2:
                continue
            pos = 2
            size = struct.unpack('!H', data[pos:pos + 2])[0]
            pos += size + 3
            size = struct.unpack('!H', data[pos:pos + 2])[0]
            path = data[pos + 2:pos + 2 + size].decode()
            code = 204 if path.endswith('/204') else 304 if path.endswith('/304') else 200
            headers = [(b'Content-Length', str(len(identity)).encode()), (b'Cache-Control', b'no-store'), (b'X-Connection', identity)]
            response = b'\x04' + struct.pack('!H', code) + ajp_string(b'OK') + struct.pack('!H', len(headers))
            response += b''.join(ajp_string(k) + ajp_string(v) for k, v in headers)
            chunk = ajp_record(b'\x03' + struct.pack('!H', len(identity)) + identity + b'\0')
            end = ajp_record(b'\x05' + (b'\0' if path.endswith('/noreuse') else b'\x01'))
            if path.endswith('/delayed'):
                self.request.sendall(ajp_record(response))
                time.sleep(0.15)
                self.request.sendall(chunk + end)
            else:
                self.request.sendall(ajp_record(response) + chunk + end)


def child_main(port):
    if port:
        server = Server(('127.0.0.1', port), FastcgiOrigin)
    else:
        # MP mode hands the listening socket to the child as stdin.
        server = Server(('127.0.0.1', 0), FastcgiOrigin, bind_and_activate=False)
        server.socket.close()
        server.socket = socket.socket(fileno=0)
        server.server_address = server.socket.getsockname()
    server.serve_forever()


def run(binary):
    from runtime_fixture import copy_runtime
    servers = []
    proc = None
    children = set()
    root = tempfile.mkdtemp(prefix='kangle-upstream-regression-')
    os.chmod(root, 0o755)
    checks = [0]

    def check(condition, message):
        if not condition:
            raise AssertionError(message)
        checks[0] += 1
        print('PASS: ' + message, flush=True)

    try:
        for name in ['etc', 'ext', 'bin', 'var', 'tmp', 'log', 'www/static']:
            os.makedirs(os.path.join(root, name))
        shutil.copy2(binary, root + '/bin/kangle')
        copy_runtime(binary, root+'/bin')
        for name in ['extworker', 'webdav.so']:
            shutil.copy2(os.path.join(os.path.dirname(binary), name), root + '/bin/' + name)
        shutil.copy2(__file__, root + '/bin/origin.py')
        with open(root + '/www/static/file.txt', 'w') as f:
            f.write('sendfile-ok')
        origin = start_server(HttpOrigin)
        fcgi = start_server(FastcgiOrigin)
        ajp = start_server(AjpOrigin)
        live = start_server(HttpOrigin, label='LIVE')
        backup = start_server(HttpOrigin, label='BACKUP')
        partial = start_server(HttpOrigin)
        servers.extend([origin, fcgi, ajp, live, backup, partial])
        frontend, sp_port, failed_port = free_port(), free_port(), free_port()
        cfg = '''<config>
<worker_thread>2</worker_thread><timeout rw='5' connect='1'/>
<listen ip='127.0.0.1' port='{frontend}' type='http'/>
<cache default='0' memory='16M' disk='0'/><response action='allow'/>
<vhs><mime_type ext='*' type='text/plain'/></vhs>
<server name='http' host='127.0.0.1' port='{http}' proto='http' life_time='30'/>
<server name='fcgi' host='127.0.0.1' port='{fcgi}' proto='fastcgi' life_time='30'/>
<server name='ajp' host='127.0.0.1' port='{ajp}' proto='ajp' life_time='30'/>
<server name='partial' host='127.0.0.1' port='{partial}' proto='http' life_time='30'/>
<server name='lb' proto='http' cookie_stick='1' max_error_count='1' error_try_time='5'>
 <node host='127.0.0.1' port='{failed}' weight='2' life_time='10'/>
 <node host='127.0.0.1' port='{backup}' weight='0' life_time='10'/>
 <node host='127.0.0.1' port='{live}' weight='1' life_time='10'/>
</server>
<cmd name='sp' proto='fastcgi' type='sp' port='{sp}' chuser='1' idle_time='0' life_time='30' file={sp_cmd}/>
<cmd name='mp' proto='fastcgi' type='mp' worker='0' chuser='1' idle_time='0' life_time='30' file={mp_cmd}/>
<api name='webdav' file='bin/webdav.so' type='sp' idle_time='0' life_time='30'/>
<request action='allow'/>
<vh name='test' doc_root='www' inherit='on' user='#65534' group='#65534'>
 <host>localhost</host><map path='/fcgi' extend='server:fcgi' confirm_file='0' allow_method='*'/>
 <map path='/ajp' extend='server:ajp' confirm_file='0' allow_method='*'/>
 <map path='/lb' extend='server:lb' confirm_file='0' allow_method='*'/>
 <map path='/partial-post' extend='server:partial' confirm_file='0' allow_method='*'/>
 <map path='/sp' extend='cmd:sp' confirm_file='0' allow_method='*'/>
 <map path='/mp' extend='cmd:mp' confirm_file='0' allow_method='*'/>
 <map path='/api' extend='api:webdav' confirm_file='0' allow_method='*'/>
 <map path='/static' extend='default' confirm_file='0' allow_method='*'/>
 <map path='/' extend='server:http' confirm_file='0' allow_method='*'/>
</vh></config>'''.format(frontend=frontend, http=origin.server_address[1], fcgi=fcgi.server_address[1],
                       ajp=ajp.server_address[1], failed=failed_port, live=live.server_address[1],
                       backup=backup.server_address[1], sp=sp_port,
                       partial=partial.server_address[1],
                       sp_cmd=quoteattr(sys.executable + ' ' + root + '/bin/origin.py --fcgi-child ' + str(sp_port)),
                       mp_cmd=quoteattr(sys.executable + ' ' + root + '/bin/origin.py --fcgi-child 0'))
        with open(root + '/etc/config.xml', 'w') as f:
            f.write(cfg)
        log = open(root + '/console.log', 'wb')
        proc = subprocess.Popen([root + '/bin/kangle', '-n', '-g'], stdout=log, stderr=log)

        def request(path, method='GET', body=None, headers=None):
            cn = http.client.HTTPConnection('127.0.0.1', frontend, timeout=10)
            cn.request(method, path, body=body, headers=dict({'Host': 'localhost'}, **(headers or {})))
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
            raise AssertionError('isolated kangle did not start; see ' + root)
        for count in [1, 8, 9]:
            code, headers, body = request('/interim/' + str(count))
            check(code == (200 if count <= 8 else 504), 'interim response count %d' % count)
            if count <= 8:
                check(body == b'ok' and 'X-Interim' not in headers, 'coalesced 1xx/final headers isolated')
        code, _, body = request('/expect', 'POST', 'complete-body', {'Expect': '100-continue'})
        check(code == 200 and body == b'complete-body', '103 does not authorize Expect body; 100 does')
        check(request('/partial-post')[0] == 200, 'pooled upload warm-up')
        try:
            code, _, _ = request('/partial-post', 'POST', b'x' * (8 * 1024 * 1024))
            check(code != 200, 'partial upload fails explicitly')
        except (OSError, http.client.HTTPException):
            check(True, 'partial upload fails explicitly (connection closed)')
        time.sleep(0.1)
        check(partial.failed_posts == 1, 'partially consumed POST is not retried on a new upstream')
        for suffix, method, status in [('head', 'HEAD', 200), ('204', 'GET', 204), ('304', 'GET', 304)]:
            code, _, body = request('/fcgi/' + suffix, method)
            check(code == status and not body, 'FastCGI no-body ' + suffix)
            check(request('/fcgi/normal')[0] == 200, 'FastCGI healthy after discarded stdout ' + suffix)
        check(request('/fcgi/sendfile')[2] == b'sendfile-ok', 'FastCGI X-Accel-Redirect discards remaining stdout')
        request('/fcgi/abort', 'HEAD')
        check(request('/fcgi/normal')[0] == 200, 'FastCGI missing END_REQUEST not pooled')
        for suffix, method, status in [('head', 'HEAD', 200), ('204', 'GET', 204), ('304', 'GET', 304), ('delayed', 'HEAD', 200)]:
            code, headers, body = request('/ajp/' + suffix, method)
            check(code == status and not body, 'AJP no-body ' + suffix)
            next_code, next_headers, next_body = request('/ajp/normal')
            check(next_code == 200 and next_body == next_headers.get('X-Connection', '').encode(), 'AJP no residual response ' + suffix)
            if suffix != 'delayed':
                check(headers.get('X-Connection') == next_headers.get('X-Connection'), 'AJP complete END_RESPONSE reusable ' + suffix)
        _, before, _ = request('/ajp/noreuse')
        _, after, _ = request('/ajp/normal')
        check(before.get('X-Connection') != after.get('X-Connection'), 'AJP reuse=0 closes connection')
        request('/lb', headers={'Cookie': 'kangle_runat=1'})
        code, headers, body = request('/lb', headers={'Cookie': 'kangle_runat=1'})
        check(code == 200 and body == b'LIVE', 'weighted failover skips weight-zero backup')
        cookie = headers.get('Set-Cookie', '').split(';')[0]
        check(cookie == 'kangle_runat=3', 'sticky cookie identifies selected virtual node')
        check(request('/lb', headers={'Cookie': cookie})[2] == b'LIVE', 'sticky follow-up selects same backend')
        recovered = start_server(HttpOrigin, failed_port, label='RECOVERED')
        servers.append(recovered)
        # Configuration clamps error_try_time to a minimum of five seconds.
        time.sleep(5.2)
        check(request('/lb', headers={'Cookie': 'kangle_runat=1'})[2] == b'RECOVERED', 'disabled primary recovers without monitor')
        for name in ['sp', 'mp']:
            code, _, body = request('/' + name + '/normal')
            check(code == 200, name + ' command starts')
            info = json.loads(body.decode())
            children.add(info['pid'])
            check(info['uid'] == 65534 and info['gid'] == 65534 and info['groups'] == [65534], name + ' child has no root supplementary groups')
            time.sleep(5.5)  # Cross at least one five-second process-GC refresh.
            check(json.loads(request('/' + name + '/normal')[2].decode())['pid'] == info['pid'], name + ' process persists with idle_time=0')
            os.kill(info['pid'], signal.SIGKILL)
            children.discard(info['pid'])
            replacement = None
            for _ in range(20):
                code, _, body = request('/' + name + '/normal')
                if code == 200:
                    replacement = json.loads(body.decode())
                    children.add(replacement['pid'])
                    break
                time.sleep(0.4)
            check(replacement is not None and replacement['pid'] != info['pid'], name + ' recovers after process crash')
        def api_pids():
            result = []
            for entry in os.listdir('/proc'):
                if entry.isdigit():
                    try:
                        with open('/proc/' + entry + '/cmdline', 'rb') as f:
                            if f.read().split(b'\0')[0] == (root + '/bin/extworker').encode():
                                result.append(int(entry))
                    except (FileNotFoundError, ProcessLookupError):
                        pass
            return result
        check(request('/api/', 'OPTIONS')[0] == 200, 'SP API child starts with new extworker')
        pids = api_pids()
        check(len(pids) == 1, 'one isolated SP API worker')
        children.update(pids)
        with open('/proc/%d/status' % pids[0]) as f:
            status = dict(line.split(':', 1) for line in f if ':' in line)
        check(status['Uid'].split() == ['65534'] * 4 and status['Groups'].split() == ['65534'], 'API child setuid clears root groups')
        os.kill(pids[0], signal.SIGKILL)
        children.discard(pids[0])
        for _ in range(20):
            if request('/api/', 'OPTIONS')[0] == 200:
                break
            time.sleep(0.4)
        replacement_pids = api_pids()
        children.update(replacement_pids)
        check(len(replacement_pids) == 1 and replacement_pids[0] != pids[0], 'SP API worker restarts after crash')
        invalid_root = root + '/invalid-run-as'
        for name in ['etc', 'var', 'tmp', 'bin']:
            os.makedirs(invalid_root + '/' + name)
        shutil.copy2(binary, invalid_root + '/bin/kangle')
        copy_runtime(binary, invalid_root+'/bin')
        with open(invalid_root + '/etc/config.xml', 'w') as f:
            f.write("<config><worker_thread>1</worker_thread><listen ip='127.0.0.1' port='%d' type='http'/>"
                    "<run_as user='kangle-regression-user-not-present'/></config>" % free_port())
        failed = subprocess.run([invalid_root + '/bin/kangle', '-n', '-g'],
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=10)
        check(failed.returncode == 127, 'invalid main run_as fails closed rather than serving as root (exit=%d, output=%s)' %
              (failed.returncode, failed.stdout.decode(errors='replace').strip()))
        check(proc.poll() is None and request('/alive')[0] == 200, 'kangle survives all upstream regressions')
        print('SUCCESS: %d checks; artifacts: %s' % (checks[0], root), flush=True)
    except Exception:
        print('FAILED; artifacts preserved: ' + root, file=sys.stderr)
        raise
    finally:
        if proc is not None:
            if proc.poll() is None:
                proc.send_signal(signal.SIGTERM)
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
        for pid in children:
            try:
                # Only PIDs returned by our isolated child are in this set.
                os.kill(pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
        for server in servers:
            server.shutdown()
            server.server_close()


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == '--fcgi-child':
        child_main(int(sys.argv[2]))
    elif len(sys.argv) == 2:
        run(os.path.abspath(sys.argv[1]))
    else:
        sys.exit(__doc__)
