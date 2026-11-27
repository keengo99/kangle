#!/usr/bin/env python3
"""URL-to-path mapping and chunked body framing regressions.

map_url_path() maps urls that did not come from the request line (WebDAV
Destination, X-Accel-Redirect). It must collapse ".." before the path redirect
check and before an alias appends the rest of the path, or a request escapes
the mapped location or the alias directory. A chunked request body must not
carry unbounded framing (chunk sizes, leading zeros, extensions) per data byte.

usage: map_url_path_regression.py <kangle binary>
"""
import http.client
import os
import shutil
import socket
import socketserver
import subprocess
import sys
import tempfile
import time
from upstream_functional_regression import free_port, start_server
from runtime_fixture import exe_name, module_name, stop_server


class AccelOrigin(socketserver.StreamRequestHandler):
    """Answers /xaccel/<n> with the n-th X-Accel-Redirect target."""
    targets = []

    def handle(self):
        self.request.settimeout(5)
        while True:
            line = self.rfile.readline()
            if not line:
                return
            path = line.decode().split(' ')[1]
            while self.rfile.readline() not in (b'\r\n', b''):
                pass
            target = self.targets[int(path.rsplit('/', 1)[-1])]
            self.request.sendall(('HTTP/1.1 200 OK\r\nCache-Control: no-store\r\n'
                                  'X-Accel-Redirect: %s\r\nContent-Length: 0\r\n\r\n' % target).encode())


def main():
    build = os.path.dirname(os.path.abspath(sys.argv[1]))
    root = tempfile.mkdtemp(prefix='kangle-map-url-path-').replace('\\', '/')
    os.chmod(root, 0o755)
    proc = None
    executable = None
    servers = []
    checks = 0

    def check(ok, label):
        nonlocal checks
        if not ok:
            raise AssertionError(label + '; artifacts: ' + root)
        checks += 1
        print('PASS:', label, flush=True)

    try:
        for name in ['bin', 'etc', 'ext', 'var', 'tmp', 'www', 'www/dav', 'davdata', 'files']:
            os.makedirs(root + '/' + name, exist_ok=True)
        for name in [exe_name('kangle'), exe_name('extworker'), module_name('webdav')]:
            shutil.copy2(build + '/' + name, root + '/bin/' + name)
        for path, data in [('www/ready', 'ready'), ('secret.txt', 'secret'), ('www/secret.txt', 'docroot-secret'),
                           ('files/allowed.txt', 'allowed')]:
            with open(root + '/' + path, 'w') as f:
                f.write(data)
        AccelOrigin.targets = ['/files/allowed.txt', '/files/../secret.txt', '/files/..%2f..%2fsecret.txt',
                               '/../../secret.txt']
        origin = start_server(AccelOrigin)
        servers.append(origin)
        port = free_port()
        cfg = """<config><worker_thread>2</worker_thread><timeout rw='10' connect='2'/>
<listen ip='127.0.0.1' port='{port}' type='http'/><cache default='0' disk='0'/>
<vhs><mime_type ext='*' type='text/plain'/></vhs><request action='allow'/><response action='allow'/>
<server name='accel' host='127.0.0.1' port='{accel}' proto='http'/>
<api name='dav' file='bin/{dav}' type='sp' idle_time='0' life_time='30'/>
<vh name='plain' doc_root='www' inherit='on'><host>plain.test</host>
<map path='/dav' extend='api:dav' confirm_file='0' allow_method='*'/>
<map path='/xaccel' extend='server:accel' confirm_file='0' allow_method='*'/>
<alias path='/files/' to='{root}/files/'/>
</vh>
<vh name='aliased' doc_root='www' inherit='on'><host>aliased.test</host>
<map path='/dav' extend='api:dav' confirm_file='0' allow_method='*'/>
<alias path='/dav/' to='{root}/davdata/'/>
</vh>
</config>""".format(port=port, accel=origin.server_address[1], dav=module_name('webdav'), root=root)
        with open(root + '/etc/config.xml', 'w') as f:
            f.write(cfg)
        log = open(root + '/console.log', 'wb')
        executable = root + '/bin/' + exe_name('kangle')
        proc = subprocess.Popen([executable, '-n', '-g'], stdout=log, stderr=log)

        def request(path, method='GET', body=None, headers=None, host='plain.test'):
            cn = http.client.HTTPConnection('127.0.0.1', port, timeout=15)
            cn.request(method, path, body=body, headers=dict({'Host': host}, **(headers or {})))
            resp = cn.getresponse()
            result = resp.status, resp.read()
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

        # WebDAV COPY: the destination must stay inside the /dav location.
        check(request('/dav/source', 'PUT', b'dav-data')[0] == 201, 'plain WebDAV PUT')
        check(request('/dav/source', 'COPY', headers={'Destination': '/dav/sub/../copy'})[0] == 201
              and open(root + '/www/dav/copy', 'rb').read() == b'dav-data', 'plain COPY with ".." inside /dav still works')
        for destination in ['/dav/../outside', '/dav/%2e%2e/outside', 'http://plain.test/dav/..%2foutside']:
            request('/dav/source', 'COPY', headers={'Destination': destination})
            check(not os.path.exists(root + '/www/outside'), 'plain COPY ' + destination + ' does not leave /dav')
        # With an absolute alias, ".." must not climb out of the alias target.
        check(request('/dav/source', 'PUT', b'alias-data', host='aliased.test')[0] == 201
              and open(root + '/davdata/source', 'rb').read() == b'alias-data', 'aliased WebDAV PUT lands in alias target')
        for destination in ['/dav/../pwned', '/dav/%2e%2e/pwned', '/dav/..%5cpwned']:
            request('/dav/source', 'COPY', headers={'Destination': destination}, host='aliased.test')
            check(not os.path.exists(root + '/pwned') and not os.path.exists(root + '/www/pwned'),
                  'aliased COPY ' + destination + ' does not escape the alias directory')
        request('/dav/source', 'MOVE', headers={'Destination': '/dav/../pwned'}, host='aliased.test')
        check(not os.path.exists(root + '/pwned') and os.path.exists(root + '/davdata/source'),
              'aliased MOVE /dav/../pwned does not escape the alias directory')

        # X-Accel-Redirect from a backend.
        check(request('/xaccel/0') == (200, b'allowed'), 'X-Accel-Redirect through an alias')
        for index in range(1, len(AccelOrigin.targets)):
            code, body = request('/xaccel/%d' % index)
            check(body != b'secret', 'X-Accel-Redirect ' + AccelOrigin.targets[index] + ' cannot read outside (got %d)' % code)

        # Chunked request body framing overhead.
        def send_chunked(chunk, count):
            """Send count copies of chunk; return the response status line or None if the server cut us off."""
            with socket.create_connection(('127.0.0.1', port), timeout=15) as sock:
                sock.sendall(b'PUT /dav/chunked HTTP/1.1\r\nHost: plain.test\r\nConnection: close\r\n'
                             b'Transfer-Encoding: chunked\r\n\r\n')
                batch = chunk * 64
                try:
                    for _ in range(count // 64):
                        sock.sendall(batch)
                    sock.sendall(b'0\r\n\r\n')
                except OSError:
                    return None
                try:
                    data = sock.recv(4096)
                except OSError:
                    return None
                return data.split(b'\r\n', 1)[0] if data else None

        def stored(status):
            return status is not None and (b' 201' in status or b' 204' in status)

        status = send_chunked(b'1\r\nX\r\n', 64 * 1024)
        check(stored(status), 'ordinary 1-byte chunks are accepted')
        check(os.path.getsize(root + '/www/dav/chunked') == 64 * 1024, 'ordinary chunked body is stored completely')
        os.remove(root + '/www/dav/chunked')
        flood = b'0' * 16000 + b'1\r\nX\r\n'
        status = send_chunked(flood, 1024)  # about 16 MB of framing for 1 KB of data
        check(not stored(status), 'chunked framing flood is rejected')
        check(not os.path.exists(root + '/www/dav/chunked') or os.path.getsize(root + '/www/dav/chunked') < 1024,
              'flooded chunked body is not stored completely')
        check(proc.poll() is None and request('/ready')[0] == 200, 'service survives')
        print('SUCCESS: %d checks; artifacts: %s' % (checks, root), flush=True)
    finally:
        if proc:
            stop_server(proc, executable)
        for server in servers:
            server.shutdown()
            server.server_close()


if __name__ == '__main__':
    main()
