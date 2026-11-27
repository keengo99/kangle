#!/usr/bin/env python3
"""Check upstream connection defaults against local origins.

1. A <server> without life_time keeps HTTP connections alive, so several
   requests through kangle reuse one origin connection.
2. A close-delimited HTTPS response that ends without TLS close_notify is
   treated as truncated: it is not cached. The same response ended with
   close_notify is complete and is cached.
3. A <host dir='http://origin/'> CDN binding without life_time also keeps
   the origin connection alive; ?life_time=0 still opens one per request.

Usage: python3 test/upstream_defaults_regression.py /absolute/path/to/kangle
Uses an isolated prefix and does not change the installed configuration.
"""
import http.client
import http.server
import os
import shutil
import socket
import socketserver
import ssl
import subprocess
import sys
import tempfile
import threading
import time
from runtime_fixture import copy_server, stop_server

BODY = b'x' * 3000


class KeepAliveOrigin(http.server.BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def setup(self):
        super().setup()
        self.server.connections += 1

    def do_GET(self):
        body = b'keepalive'
        self.send_response(200)
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args):
        pass


class TlsOrigin(threading.Thread):
    """Answers each request with a close-delimited, cacheable response."""

    def __init__(self, cert, key):
        super().__init__(daemon=True)
        self.ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        self.ctx.load_cert_chain(cert, key)
        self.sock = socket.socket()
        self.sock.bind(('127.0.0.1', 0))
        self.sock.listen(16)
        self.port = self.sock.getsockname()[1]
        self.requests = {}

    def run(self):
        while True:
            raw, _ = self.sock.accept()
            threading.Thread(target=self.serve, args=(raw,), daemon=True).start()

    def serve(self, raw):
        try:
            conn = self.ctx.wrap_socket(raw, server_side=True)
            data = b''
            while b'\r\n\r\n' not in data:
                chunk = conn.recv(4096)
                if not chunk:
                    return
                data += chunk
            path = data.split(b' ', 2)[1].decode()
            self.requests[path] = self.requests.get(path, 0) + 1
            conn.sendall(b'HTTP/1.1 200 OK\r\nCache-Control: max-age=60\r\n'
                         b'Content-Type: text/plain\r\nConnection: close\r\n\r\n' + BODY)
            if path.startswith('/clean'):
                conn = conn.unwrap()  # sends close_notify
            # closing the TLS socket directly sends no close_notify
            conn.close()
        except (OSError, ssl.SSLError):
            pass


def wait_port(proc, port, root):
    for _ in range(100):
        try:
            socket.create_connection(('127.0.0.1', port), timeout=.1).close()
            return
        except OSError:
            assert proc.poll() is None, 'server exited: ' + root
            time.sleep(.05)
    raise AssertionError('server did not start: ' + root)


def fetch(port, path):
    conn = http.client.HTTPConnection('127.0.0.1', port, timeout=5)
    try:
        conn.request('GET', path)
        res = conn.getresponse()
        try:
            return res.status, res.read()
        except http.client.IncompleteRead as e:
            return res.status, b'incomplete:' + e.partial
    except (OSError, http.client.HTTPException) as e:
        return 0, repr(e).encode()
    finally:
        conn.close()


def fetch_host(port, host, path):
    conn = http.client.HTTPConnection('127.0.0.1', port, timeout=5)
    try:
        conn.request('GET', path, headers={'Host': host})
        res = conn.getresponse()
        return res.status, res.read()
    finally:
        conn.close()


def origin_connections(plain, port, host, tag):
    before = plain.connections
    for i in range(5):
        status, body = fetch_host(port, host, '/%s/%d?nocache=%d' % (tag, i, time.time() * 1000))
        assert status == 200 and body == b'keepalive', (host, status, body)
    return plain.connections - before


def free_port():
    s = socket.socket()
    s.bind(('127.0.0.1', 0))
    port = s.getsockname()[1]
    s.close()
    return port


def run(binary):
    root = tempfile.mkdtemp(prefix='kangle-upstream-defaults-')
    proc = None
    try:
        for name in ('etc', 'ext', 'bin', 'var', 'tmp', 'log', 'www'):
            os.makedirs(root + '/' + name)
        cert, key = root + '/etc/origin.crt', root + '/etc/origin.key'
        subprocess.check_call(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes',
                               '-keyout', key, '-out', cert, '-days', '1', '-subj', '/CN=localhost'],
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        plain = http.server.ThreadingHTTPServer(('127.0.0.1', 0), KeepAliveOrigin) \
            if hasattr(http.server, 'ThreadingHTTPServer') else \
            type('T', (socketserver.ThreadingMixIn, http.server.HTTPServer), {})(('127.0.0.1', 0), KeepAliveOrigin)
        plain.daemon_threads = True
        plain.connections = 0
        threading.Thread(target=plain.serve_forever, daemon=True).start()
        # life_time is not part of the CDN pool key, so ?life_time=0 gets its own origin port
        plain0 = type(plain)(('127.0.0.1', 0), KeepAliveOrigin)
        plain0.daemon_threads = True
        plain0.connections = 0
        threading.Thread(target=plain0.serve_forever, daemon=True).start()
        tls = TlsOrigin(cert, key)
        tls.start()

        port = free_port()
        executable = copy_server(binary, root + '/bin')
        cfg = "<config><worker_thread>2</worker_thread><timeout rw='5' connect='2'/>"
        cfg += "<listen ip='127.0.0.1' port='%d' type='http'/>" % port
        cfg += "<request action='allow'/><response action='allow'/>"
        cfg += "<cache default='1' memory='10M' disk='0' max_cache_size='1M'/>"
        cfg += "<server name='plain' host='127.0.0.1' port='%d' proto='http'/>" % plain.server_port
        cfg += "<server name='tls' host='127.0.0.1' port='%ds' proto='http' life_time='0'/>" % tls.port
        cfg += "<vh name='t' doc_root='www' inherit='on'><host>*</host>"
        cfg += "<map path='/ka/' extend='server:plain' confirm_file='0' allow_method='*'/>"
        cfg += "<map path='/clean' extend='server:tls' confirm_file='0' allow_method='*'/>"
        cfg += "<map path='/cut' extend='server:tls' confirm_file='0' allow_method='*'/>"
        cfg += "</vh>"
        origin = 'http://127.0.0.1:%d/' % plain.server_port
        cfg += "<vh name='cdn' doc_root='www' inherit='on'>"
        cfg += "<host dir='%s'>cdn.test</host>" % origin
        cfg += "<host dir='http://127.0.0.1:%d/?life_time=0'>cdn0.test</host>" % plain0.server_port
        cfg += "</vh></config>"
        with open(root + '/etc/config.xml', 'w') as f:
            f.write(cfg)
        with open(root + '/console.log', 'wb') as log:
            proc = subprocess.Popen([executable, '-n', '-g'], stdout=log, stderr=log)
        wait_port(proc, port, root)

        # 1. keepalive by default: no-cache paths so every request reaches the origin
        for i in range(5):
            status, body = fetch(port, '/ka/%d?nocache=%d' % (i, time.time() * 1000))
            assert status == 200 and body == b'keepalive', (status, body)
        assert plain.connections == 1, 'origin connections: %d (expected 1)' % plain.connections

        # 2. TLS close_notify decides whether a close-delimited body is complete
        status, body = fetch(port, '/clean')
        assert status == 200 and body == BODY, ('clean', status, body[:80])
        fetch(port, '/clean')
        assert tls.requests.get('/clean') == 1, 'complete response was not cached: %r' % tls.requests

        # the client may still get every byte; what matters is that kangle
        # does not store the response as complete
        fetch(port, '/cut')
        fetch(port, '/cut')
        assert tls.requests.get('/cut') == 2, 'truncated response was cached: %r' % tls.requests
        # 3. http:// CDN bindings use the same default as <server>
        got = origin_connections(plain, port, 'cdn.test', 'cdn')
        assert got == 1, 'http:// without life_time: %d origin connections (expected 1)' % got
        got = origin_connections(plain0, port, 'cdn0.test', 'cdn0')
        assert got == 5, 'http:// with life_time=0: %d origin connections (expected 5)' % got
        print('upstream defaults regression passed')
    finally:
        if proc is not None:
            stop_server(proc, executable)
        shutil.rmtree(root, ignore_errors=True)


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(2)
    run(os.path.abspath(sys.argv[1]))
