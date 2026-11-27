#!/usr/bin/env python3
"""Check that TLS responses are not split into many tiny records.

Every SSL_write makes one TLS record. Response headers, chunked framing and
HTTP/2 frame headers come as small iovecs; written one by one they made a
record each (Windows IOCP, which uses a memory BIO, sent 20 records for a
4 KB HTTP/1.1 response). This test reads the raw TLS 1.2 stream, counts
application-data records and checks the decrypted bodies:

1. HTTP/1.1 4 KB file: at most 2 records.
2. HTTP/1.1 300 KB file: body intact, no tiny records.
3. HTTP/1.1 chunked proxy response: no extra records for chunk framing.
4. HTTP/2 4 KB file: frame headers do not get records of their own.

Usage: python3 test/tls_record_coalesce_regression.py /absolute/path/to/kangle
Uses an isolated prefix and does not change the installed configuration.
"""
import hashlib
import http.server
import os
import shutil
import socket
import socketserver
import ssl
import struct
import subprocess
import sys
import tempfile
import threading
import time
from runtime_fixture import copy_server, stop_server

CHUNKS = 20
CHUNK = b'c' * 1000


class ChunkedOrigin(http.server.BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def do_GET(self):
        self.send_response(200)
        self.send_header('Content-Type', 'text/plain')
        self.send_header('Cache-Control', 'no-store')
        self.send_header('Transfer-Encoding', 'chunked')
        self.end_headers()
        for _ in range(CHUNKS):
            self.wfile.write(b'%x\r\n%s\r\n' % (len(CHUNK), CHUNK))
            self.wfile.flush()
            time.sleep(0.005)
        self.wfile.write(b'0\r\n\r\n')

    def log_message(self, *args):
        pass


def free_port():
    s = socket.socket()
    s.bind(('127.0.0.1', 0))
    port = s.getsockname()[1]
    s.close()
    return port


class TlsClient:
    """TLS 1.2 client over a memory BIO, so the raw records stay visible."""

    def __init__(self, port, alpn=None):
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
        ctx.check_hostname = False
        ctx.verify_mode = ssl.CERT_NONE
        # TLS 1.3 sends session tickets as application-data records
        ctx.options |= getattr(ssl, 'OP_NO_TLSv1_3', 0)
        if alpn:
            ctx.set_alpn_protocols(alpn)
        self.inc, self.out = ssl.MemoryBIO(), ssl.MemoryBIO()
        self.obj = ctx.wrap_bio(self.inc, self.out, server_hostname='localhost')
        self.sock = socket.create_connection(('127.0.0.1', port), timeout=5)
        while True:
            try:
                self.obj.do_handshake()
                break
            except ssl.SSLWantReadError:
                self.flush()
                data = self.sock.recv(65536)
                assert data, 'handshake: connection closed'
                self.inc.write(data)
        self.flush()

    def flush(self):
        data = self.out.read()
        if data:
            self.sock.sendall(data)

    def send(self, data):
        self.obj.write(data)
        self.flush()

    def receive(self, done, timeout=5):
        """Read until done(plain) is true; return (record sizes, plaintext)."""
        raw = b''
        plain = b''
        deadline = time.time() + timeout
        while not done(plain):
            assert time.time() < deadline, 'timeout, got %d bytes' % len(plain)
            self.sock.settimeout(max(deadline - time.time(), 0.1))
            try:
                data = self.sock.recv(65536)
            except socket.timeout:
                continue
            if not data:
                break
            raw += data
            self.inc.write(data)
            try:
                while True:
                    plain += self.obj.read(65536)
            except (ssl.SSLWantReadError, ssl.SSLZeroReturnError):
                pass
        records = []
        i = 0
        while i + 5 <= len(raw):
            length = int.from_bytes(raw[i + 3:i + 5], 'big')
            if raw[i] == 0x17:
                records.append(length)
            i += 5 + length
        return records, plain

    def close(self):
        self.sock.close()


def http1_done(plain):
    end = plain.find(b'\r\n\r\n')
    if end < 0:
        return False
    head = plain[:end].lower()
    if b'transfer-encoding: chunked' in head:
        return plain.endswith(b'\r\n0\r\n\r\n')
    for line in head.split(b'\r\n'):
        if line.startswith(b'content-length:'):
            return len(plain) >= end + 4 + int(line.split(b':')[1])
    return False


def http1_get(port, path):
    client = TlsClient(port)
    try:
        client.send(('GET %s HTTP/1.1\r\nHost: localhost\r\nAccept-Encoding: identity\r\n\r\n' % path).encode())
        records, plain = client.receive(http1_done)
    finally:
        client.close()
    head, _, body = plain.partition(b'\r\n\r\n')
    assert head.startswith(b'HTTP/1.1 200'), head[:100]
    if b'chunked' in head.lower():
        decoded = b''
        while True:
            size_line, _, body = body.partition(b'\r\n')
            size = int(size_line, 16)
            if size == 0:
                break
            decoded += body[:size]
            body = body[size + 2:]
        body = decoded
    return records, body


def h2_frame(typ, flags, stream, payload):
    return struct.pack('>I', len(payload))[1:] + bytes([typ, flags]) + struct.pack('>I', stream) + payload


def h2_frames(plain):
    frames = []
    i = 0
    while i + 9 <= len(plain):
        length = int.from_bytes(plain[i:i + 3], 'big')
        if i + 9 + length > len(plain):
            break
        frames.append((plain[i + 3], plain[i + 4], int.from_bytes(plain[i + 5:i + 9], 'big') & 0x7fffffff,
                       plain[i + 9:i + 9 + length]))
        i += 9 + length
    return frames


def h2_get(port, path):
    client = TlsClient(port, ['h2'])
    try:
        assert client.obj.selected_alpn_protocol() == 'h2', client.obj.selected_alpn_protocol()
        client.send(b'PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n' + h2_frame(4, 0, 0, b''))
        client.receive(lambda p: any(f[0] == 4 and not f[1] & 1 for f in h2_frames(p)))
        client.send(h2_frame(4, 1, 0, b''))
        # the server acknowledges our SETTINGS; keep that record out of the count
        client.receive(lambda p: any(f[0] == 4 and f[1] & 1 for f in h2_frames(p)), timeout=1)
        value = path.encode()
        headers = b'\x82\x87' + bytes([0x44, len(value)]) + value + b'\x41\x09localhost'
        client.send(h2_frame(1, 0x5, 1, headers))
        records, plain = client.receive(
            lambda p: any(f[2] == 1 and f[1] & 1 and f[0] in (0, 1) for f in h2_frames(p)))
    finally:
        client.close()
    body = b''.join(f[3] for f in h2_frames(plain) if f[0] == 0 and f[2] == 1)
    return records, body


def run(binary):
    origin = http.server.ThreadingHTTPServer(('127.0.0.1', 0), ChunkedOrigin) \
        if hasattr(http.server, 'ThreadingHTTPServer') else \
        type('T', (socketserver.ThreadingMixIn, http.server.HTTPServer), {})(('127.0.0.1', 0), ChunkedOrigin)
    origin.daemon_threads = True
    threading.Thread(target=origin.serve_forever, daemon=True).start()
    root = tempfile.mkdtemp(prefix='kangle-tls-records-')
    proc = None
    executable = None
    try:
        for name in ('etc', 'ext', 'bin', 'var', 'tmp', 'log', 'www'):
            os.makedirs(root + '/' + name)
        subprocess.check_call(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes',
                               '-keyout', root + '/etc/key.pem', '-out', root + '/etc/cert.pem',
                               '-days', '1', '-subj', '/CN=localhost'],
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        small = b''.join(b'line %04d of the kangle tls record test\n' % i for i in range(200))[:4096]
        big = os.urandom(300000)
        with open(root + '/www/4k.txt', 'wb') as f:
            f.write(small)
        with open(root + '/www/big.bin', 'wb') as f:
            f.write(big)
        port = free_port()
        executable = copy_server(binary, root + '/bin')
        cfg = "<config><worker_thread>2</worker_thread><timeout rw='5' connect='2'/>"
        cfg += ("<listen ip='127.0.0.1' port='%d' type='https' certificate='etc/cert.pem' "
                "certificate_key='etc/key.pem' http2='1'/>") % port
        cfg += "<vhs><mime_type ext='*' type='application/octet-stream'/></vhs>"
        cfg += "<request action='allow'/><response action='allow'/>"
        cfg += "<cache default='0' memory='0' disk='0'/>"
        cfg += "<server name='origin' host='127.0.0.1' port='%d' proto='http'/>" % origin.server_port
        cfg += "<vh name='t' doc_root='www' inherit='on'><host>*</host>"
        cfg += "<map path='/chunked' extend='server:origin' confirm_file='0' allow_method='*'/>"
        cfg += "</vh></config>"
        with open(root + '/etc/config.xml', 'w') as f:
            f.write(cfg)
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

        records, body = http1_get(port, '/4k.txt')
        assert body == small, 'HTTP/1.1 4 KB body differs'
        assert len(records) <= 2, 'HTTP/1.1 4 KB: %d records %s' % (len(records), records)
        print('PASS: HTTP/1.1 4 KB response in %d records' % len(records), flush=True)

        records, body = http1_get(port, '/big.bin')
        assert hashlib.md5(body).digest() == hashlib.md5(big).digest(), 'HTTP/1.1 300 KB body differs'
        tiny = [r for r in records if r < 100]
        limit = len(big) // 8192 + 4
        assert len(tiny) <= 1 and len(records) <= limit,             'HTTP/1.1 300 KB: %d records (limit %d), %d tiny' % (len(records), limit, len(tiny))
        print('PASS: HTTP/1.1 300 KB response in %d records' % len(records), flush=True)

        records, body = http1_get(port, '/chunked')
        assert body == CHUNK * CHUNKS, 'chunked body differs (%d bytes)' % len(body)
        tiny = [r for r in records if r < 100]
        assert len(tiny) <= 2, 'chunked: %d records under 100 bytes of %d %s' % (len(tiny), len(records), records)
        print('PASS: chunked response in %d records, %d tiny' % (len(records), len(tiny)), flush=True)

        records, body = h2_get(port, '/4k.txt')
        assert body == small, 'HTTP/2 4 KB body differs (%d bytes)' % len(body)
        tiny = [r for r in records if r < 100]
        assert len(tiny) <= 1, 'HTTP/2 4 KB: %d records under 100 bytes %s' % (len(tiny), records)
        print('PASS: HTTP/2 4 KB response in %d records' % len(records), flush=True)
        print('TLS record regression passed', flush=True)
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
