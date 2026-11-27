#!/usr/bin/env python3
"""Build the QUIC probe and run H3 regressions in an isolated server prefix.

Requires Go 1.23 and Python with h2>=3.2. Does not change the installed config.
Usage: python3 test/http3_integration.py /absolute/path/to/kangle
"""
import os
import shutil
import signal
import socket
import socketserver
import subprocess
import sys
import tempfile
import threading
import time
from runtime_fixture import copy_runtime
from http_protocol_integration import Server, H2Origin, fields, exact, chunks, port
from early_hints_integration import HttpOrigin as HintHttpOrigin, H2Origin as HintH2Origin


class HttpOrigin(socketserver.StreamRequestHandler):
    def handle(self):
        self.request.settimeout(12)
        while True:
            first = self.rfile.readline()
            if not first:
                return
            method, path, _ = first.split(b' ', 2)
            headers = fields(self.rfile)
            if path in (b'/hold', b'/early-response'):
                with self.server.lock:
                    self.server.held += 1
                if path == b'/early-response':
                    self.request.sendall(b'HTTP/1.1 413 Too Large\r\nContent-Length: 2\r\n\r\nno')
                try:
                    while self.request.recv(1024):
                        pass
                except (OSError, socket.timeout):
                    pass
                with self.server.lock:
                    self.server.canceled += 1
                return
            # Status and reload endpoints also receive chunked H3 requests.
            # Consume their framing before reusing the HTTP/1 connection.
            if headers.get(b'expect') == b'100-continue':
                self.request.sendall(b'HTTP/1.1 100 Continue\r\n\r\n')
            if headers.get(b'transfer-encoding') == b'chunked':
                body, trailers = chunks(self.rfile)
            else:
                length = int(headers.get(b'content-length', b'0'))
                if path == b'/form-streaming' and length:
                    body = exact(self.rfile, 1)
                    with self.server.lock:
                        self.server.form_started += 1
                        self.server.form_prefix = b'|'.join([str(self.server.form_started).encode(),
                            headers.get(b'content-length', b''), headers.get(b'transfer-encoding', b'')])
                    body += exact(self.rfile, length-1)
                else:
                    body = exact(self.rfile, length)
                trailers = {}
            if path == b'/form-framing':
                body = b'|'.join([headers.get(b'content-length', b''), headers.get(b'transfer-encoding', b''),
                                  body, trailers.get(b'x-upload', b'')])
            elif path == b'/form-stream-prefix':
                with self.server.lock:
                    body = self.server.form_prefix
            elif path == b'/cancel-count':
                with self.server.lock:
                    body = str(self.server.canceled).encode()
            elif path == b'/hold-count':
                with self.server.lock:
                    body = str(self.server.held).encode()
            elif path in (b'/reload', b'/reload-in-place', b'/reload-sni'):
                self.server.reload(path)
                body = b'ok'
            elif path == b'/h2-reset-count':
                with self.server.h2_origin.lock:
                    body = str(self.server.h2_origin.resets).encode()
            else:
                if path in (b'/upload', b'/trailers', b'/empty'):
                    payload = body if path == b'/upload' else (b'abc' if path == b'/trailers' else b'')
                    self.request.sendall(b'HTTP/1.1 200 OK\r\nCache-Control: no-store\r\nTransfer-Encoding: chunked\r\nTrailer: X-Meta\r\n\r\n')
                    if payload:
                        self.request.sendall(('%x\r\n' % len(payload)).encode()+payload+b'\r\n')
                    self.request.sendall(b'0\r\nX-Meta: '+trailers.get(b'x-upload', b'ok')+b'\r\n\r\n')
                    continue
                if method != b'POST':
                    body = b'ok'
            self.request.sendall(b'HTTP/1.1 200 OK\r\nCache-Control: no-store\r\nContent-Length: '+str(len(body)).encode()+b'\r\n\r\n')
            if method != b'HEAD':
                self.request.sendall(body)


def run(binary):
    root = tempfile.mkdtemp(prefix='kangle-http3-')
    proc, servers = None, []
    try:
        for name in ('etc', 'ext', 'bin', 'var', 'tmp', 'log', 'www'):
            os.makedirs(root+'/'+name)
        shutil.copy2(binary, root+'/bin/kangle')
        copy_runtime(binary, root+'/bin')
        for handler in (HttpOrigin, H2Origin, HintHttpOrigin, HintH2Origin):
            server = Server(('127.0.0.1', 0), handler)
            server.lock, server.canceled, server.resets, server.held = threading.Lock(), 0, 0, 0
            server.form_started, server.form_prefix = 0, b'0||'
            servers.append(server)
            threading.Thread(target=server.serve_forever, daemon=True).start()
        servers[0].h2_origin = servers[1]
        for suffix in ('a', 'b', 'c', 'd', 'e'):
            subprocess.check_call(['openssl', 'req', '-x509', '-nodes', '-newkey', 'rsa:2048',
                '-keyout', root+'/etc/key-'+suffix+'.pem', '-out', root+'/etc/cert-'+suffix+'.pem',
                '-days', '2', '-subj', '/CN=localhost-'+suffix], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        tlsport = port()
        cfg = "<config><worker_thread>2</worker_thread><timeout rw='3' connect='2'/>"
        cfg += "<listen ip='127.0.0.1' port='%d' type='https' certificate='etc/cert-a.pem' certificate_key='etc/key-a.pem' http2='1' http3='1'/>" % tlsport
        cfg += "<cache default='0' memory='16M' disk='0'/><request action='allow'/><response action='allow'/>"
        for name, server in zip(('h1', 'h2', 'hint1', 'hint2'), servers):
            cfg += "<server name='%s' host='127.0.0.1' port='%d%s' proto='http' life_time='30'/>" % (name, server.server_address[1], 'h' if name in ('h2', 'hint2') else '')
            for mode in ('on', 'off', 'h2'):
                cert = " certificate='-etc/cert-d.pem' certificate_key='-etc/key-d.pem'" if name == 'h2' and mode == 'on' else ''
                cfg += "<vh name='%s-%s' doc_root='www' inherit='on' early_hints='%s'%s><host>%s.%s.test</host><map path='/' extend='server:%s' confirm_file='0' allow_method='*'/></vh>" % (name, mode, mode, cert, name, mode, name)
        cfg += '</config>'
        config_file = root+'/etc/config.xml'
        with open(config_file, 'w') as f:
            f.write(cfg)
        log = open(root+'/console.log', 'wb')
        proc = subprocess.Popen([root+'/bin/kangle', '-n', '-g'], stdout=log, stderr=log)
        current_config = [cfg]
        def reload_certificate(path):
            if path == b'/reload':
                current_config[0] = current_config[0].replace('cert-a.pem', 'cert-b.pem').replace('key-a.pem', 'key-b.pem')
            elif path == b'/reload-sni':
                current_config[0] = current_config[0].replace('cert-d.pem', 'cert-e.pem').replace('key-d.pem', 'key-e.pem')
            else:
                shutil.copyfile(root+'/etc/cert-c.pem', root+'/etc/cert-b.pem')
                shutil.copyfile(root+'/etc/key-c.pem', root+'/etc/key-b.pem')
            with open(config_file, 'w') as f:
                f.write(current_config[0])
            os.kill(proc.pid, signal.SIGHUP)
        servers[0].reload = reload_certificate
        for _ in range(100):
            try:
                socket.create_connection(('127.0.0.1', tlsport), timeout=.1).close()
                break
            except OSError:
                if proc.poll() is not None:
                    raise AssertionError('H3 server exited: '+root)
                time.sleep(.05)
        framework = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'test_framework')
        probe = root+'/http3-probe'
        subprocess.check_call(['go', 'build', '-o', probe, './http3_upgrade'], cwd=framework)
        subprocess.check_call([probe, '127.0.0.1:'+str(tlsport)])
        print('H3 integration passed; artifacts: '+root, flush=True)
    finally:
        if proc and proc.poll() is None:
            proc.send_signal(signal.SIGTERM)
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
        for server in servers:
            server.shutdown()
        print('H3 test artifacts: '+root, flush=True)


if __name__ == '__main__':
    run(os.path.abspath(sys.argv[1]))
