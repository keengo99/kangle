#!/usr/bin/env python3
"""Check WebSocket through a CDN (reverse proxy) site over TLS.

Clients connect with wss over HTTP/1.1 and over HTTP/2 extended CONNECT
(RFC 8441, what browsers use when the server advertises it). The origin is a
strict WebSocket echo server, reached over plain TCP and over TLS.

Usage: python3 test/websocket_proxy_integration.py /absolute/path/to/kangle
Needs the h2 package. Uses an isolated prefix and does not change the
installed configuration.
"""
import base64
import hashlib
import os
import socket
import socketserver
import ssl
import struct
import subprocess
import sys
import tempfile
import threading
import time

import h2.config
import h2.connection
import h2.errors
import h2.events
import h2.settings

from runtime_fixture import copy_server, stop_server

GUID = b'258EAFA5-E914-47DA-95CA-C5AB0DC85B11'
CLOSE = b'\x03\xe8'
DOMAIN = 'ws.example.test'


def ws_frame(payload, opcode=1, mask=True):
    head = bytes([0x80 | opcode])
    n = len(payload)
    m = 0x80 if mask else 0
    if n < 126:
        head += bytes([m | n])
    elif n < 65536:
        head += bytes([m | 126]) + struct.pack('!H', n)
    else:
        head += bytes([m | 127]) + struct.pack('!Q', n)
    if not mask:
        return head + payload
    key = os.urandom(4)
    return head + key + bytes(b ^ key[i % 4] for i, b in enumerate(payload))


class FrameReader:
    """Parse WebSocket frames from bytes fed by a transport."""

    def __init__(self):
        self.buf = b''

    def feed(self, data):
        self.buf += data

    def frame(self):
        b = self.buf
        if len(b) < 2:
            return None
        n = b[1] & 0x7f
        pos = 2
        if n == 126:
            if len(b) < 4:
                return None
            n = struct.unpack('!H', b[2:4])[0]
            pos = 4
        elif n == 127:
            if len(b) < 10:
                return None
            n = struct.unpack('!Q', b[2:10])[0]
            pos = 10
        key = None
        if b[1] & 0x80:
            if len(b) < pos+4:
                return None
            key = b[pos:pos+4]
            pos += 4
        if len(b) < pos+n:
            return None
        payload = b[pos:pos+n]
        if key:
            payload = bytes(c ^ key[i % 4] for i, c in enumerate(payload))
        self.buf = b[pos+n:]
        return b[0] & 0x0f, payload


class Origin(socketserver.ThreadingMixIn, socketserver.TCPServer):
    daemon_threads = True
    allow_reuse_address = True


class OriginHandler(socketserver.BaseRequestHandler):
    """Strict RFC 6455 server: needs Upgrade, Connection, Version and Key."""

    def handle(self):
        sock = self.request
        if self.server.tls:
            sock = self.server.tls.wrap_socket(sock, server_side=True)
        data = b''
        while b'\r\n\r\n' not in data:
            got = sock.recv(65536)
            if not got:
                return
            data += got
        head, rest = data.split(b'\r\n\r\n', 1)
        lines = head.decode('latin-1').split('\r\n')
        headers = {}
        for line in lines[1:]:
            k, v = line.split(':', 1)
            headers[k.strip().lower()] = v.strip()
        self.server.seen.append((lines[0], headers))
        key = headers.get('sec-websocket-key', '')
        ok = (lines[0].startswith('GET ') and
              headers.get('upgrade', '').lower() == 'websocket' and
              'upgrade' in headers.get('connection', '').lower() and
              headers.get('sec-websocket-version') == '13' and
              len(base64.b64decode(key + '==')) == 16 if key else False)
        if not ok:
            body = b'bad websocket handshake'
            sock.sendall(b'HTTP/1.1 400 Bad Request\r\nContent-Length: %d\r\n'
                         b'Connection: close\r\n\r\n%s' % (len(body), body))
            return
        accept = base64.b64encode(hashlib.sha1(key.encode() + GUID).digest())
        # The greeting rides in the same packet as the 101 response.
        response = (b'HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n'
                    b'Connection: Upgrade\r\nSec-WebSocket-Accept: ' + accept +
                    b'\r\n\r\n' + ws_frame(b'hello', mask=False))
        if lines[0].split(' ')[1].startswith('/close'):
            # The origin ends the session at once: greeting, close, FIN.
            sock.sendall(response + ws_frame(CLOSE, 8, mask=False))
            return
        sock.sendall(response)
        reader = FrameReader()
        reader.feed(rest)
        while True:
            f = reader.frame()
            if f is None:
                got = sock.recv(65536)
                if not got:
                    return
                reader.feed(got)
                continue
            opcode, payload = f
            if opcode == 8:
                sock.sendall(ws_frame(payload, 8, mask=False))
                return
            sock.sendall(ws_frame(payload, opcode, mask=False))


def start_origin(tls_ctx):
    srv = Origin(('127.0.0.2', 0), OriginHandler)
    srv.tls = tls_ctx
    srv.seen = []
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    return srv


def client_ctx(alpn):
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    ctx.check_hostname = False
    ctx.verify_mode = ssl.CERT_NONE
    ctx.set_alpn_protocols(alpn)
    return ctx


MESSAGES = [b'ping', b'x' * 1000, os.urandom(200000)]


def exchange(send, recv_frame):
    """Expect the greeting, then echo each message and close cleanly."""
    f = recv_frame()
    assert f == (1, b'hello'), ('no greeting', f)
    for m in MESSAGES:
        opcode = 1 if m[:1] in (b'p', b'x') else 2
        send(ws_frame(m, opcode))
        f = recv_frame()
        assert f == (opcode, m), ('bad echo', len(m), f and (f[0], len(f[1])))
    send(ws_frame(b'\x03\xe8', 8))
    f = recv_frame()
    assert f and f[0] == 8, ('no close', f)


def wss_h1(port, path):
    sock = client_ctx(['http/1.1']).wrap_socket(
        socket.create_connection(('127.0.0.1', port), timeout=5), server_hostname=DOMAIN)
    try:
        key = base64.b64encode(os.urandom(16)).decode()
        sock.sendall(('GET %s HTTP/1.1\r\nHost: %s:%d\r\nUpgrade: websocket\r\n'
                      'Connection: Upgrade\r\nSec-WebSocket-Key: %s\r\n'
                      'Sec-WebSocket-Version: 13\r\nOrigin: https://%s\r\n\r\n'
                      % (path, DOMAIN, port, key, DOMAIN)).encode())
        data = b''
        while b'\r\n\r\n' not in data:
            got = sock.recv(65536)
            assert got, ('closed before response', data)
            data += got
        head, rest = data.split(b'\r\n\r\n', 1)
        status = head.split(b'\r\n')[0]
        assert b' 101 ' in status, head.decode('latin-1')
        expect = base64.b64encode(hashlib.sha1(key.encode() + GUID).digest())
        assert b'sec-websocket-accept: ' + expect.lower() in head.lower(), head
        reader = FrameReader()
        reader.feed(rest)

        def recv_frame():
            while True:
                f = reader.frame()
                if f is not None:
                    return f
                got = sock.recv(65536)
                if not got:
                    return None
                reader.feed(got)
        exchange(sock.sendall, recv_frame)
        return key
    finally:
        sock.close()


def wss_h2(port, path):
    sock = client_ctx(['h2']).wrap_socket(
        socket.create_connection(('127.0.0.1', port), timeout=5), server_hostname=DOMAIN)
    try:
        assert sock.selected_alpn_protocol() == 'h2'
        conn = h2.connection.H2Connection(h2.config.H2Configuration(
            client_side=True, validate_outbound_headers=False,
            normalize_outbound_headers=False, validate_inbound_headers=False))
        conn.initiate_connection()
        sock.sendall(conn.data_to_send())
        settings = {}
        state = {'headers': None, 'ended': False, 'reset': None}
        reader = FrameReader()

        def pump():
            got = sock.recv(65536)
            if not got:
                raise AssertionError('connection closed')
            for ev in conn.receive_data(got):
                if isinstance(ev, h2.events.RemoteSettingsChanged):
                    for k, v in ev.changed_settings.items():
                        settings[int(k)] = v.new_value
                elif isinstance(ev, h2.events.ResponseReceived):
                    state['headers'] = dict(ev.headers)
                elif isinstance(ev, h2.events.DataReceived):
                    reader.feed(ev.data)
                    conn.acknowledge_received_data(ev.flow_controlled_length, ev.stream_id)
                elif isinstance(ev, h2.events.StreamEnded):
                    state['ended'] = True
                elif isinstance(ev, h2.events.StreamReset):
                    state['reset'] = ev.error_code
            out = conn.data_to_send()
            if out:
                sock.sendall(out)
        while 8 not in settings:
            pump()
        assert settings[8] == 1, ('extended CONNECT not advertised', settings)
        sid = conn.get_next_available_stream_id()
        # Browsers send no Sec-WebSocket-Key over HTTP/2 (RFC 8441 section 5).
        conn.send_headers(sid, [
            (':method', 'CONNECT'), (':protocol', 'websocket'), (':scheme', 'https'),
            (':path', path), (':authority', '%s:%d' % (DOMAIN, port)),
            ('sec-websocket-version', '13'), ('origin', 'https://' + DOMAIN)])
        sock.sendall(conn.data_to_send())
        while state['headers'] is None and state['reset'] is None:
            pump()
        assert state['reset'] is None, ('stream reset', state['reset'])
        status = state['headers'].get(b':status')
        assert status == b'200', ('status', state['headers'])
        assert b'sec-websocket-accept' not in state['headers'], state['headers']

        def send(data):
            while data:
                n = min(len(data), conn.local_flow_control_window(sid),
                        conn.max_outbound_frame_size)
                if n <= 0:
                    pump()
                    continue
                conn.send_data(sid, data[:n])
                data = data[n:]
                sock.sendall(conn.data_to_send())

        def recv_frame():
            while True:
                f = reader.frame()
                if f is not None:
                    return f
                if state['ended'] or state['reset'] is not None:
                    return None
                pump()
        exchange(send, recv_frame)
        return None
    finally:
        sock.close()


def h1_tunnel(port, path):
    """Open a wss tunnel over HTTP/1.1; return (socket, recv_frame)."""
    sock = client_ctx(['http/1.1']).wrap_socket(
        socket.create_connection(('127.0.0.1', port), timeout=5), server_hostname=DOMAIN)
    key = base64.b64encode(os.urandom(16)).decode()
    sock.sendall(('GET %s HTTP/1.1\r\nHost: %s:%d\r\nUpgrade: websocket\r\n'
                  'Connection: Upgrade\r\nSec-WebSocket-Key: %s\r\n'
                  'Sec-WebSocket-Version: 13\r\n\r\n' % (path, DOMAIN, port, key)).encode())
    data = b''
    while b'\r\n\r\n' not in data:
        got = sock.recv(65536)
        assert got, ('closed before response', data)
        data += got
    head, rest = data.split(b'\r\n\r\n', 1)
    assert b' 101 ' in head.split(b'\r\n')[0], head.decode('latin-1')
    reader = FrameReader()
    reader.feed(rest)

    def recv_frame():
        while True:
            f = reader.frame()
            if f is not None:
                return f
            got = sock.recv(65536)
            if not got:
                assert not reader.buf, ('partial frame at EOF', reader.buf)
                return None
            reader.feed(got)
    return sock, recv_frame


class H2Tunnels:
    """WebSocket streams (RFC 8441) on one HTTP/2 connection."""

    def __init__(self, port):
        self.sock = client_ctx(['h2']).wrap_socket(
            socket.create_connection(('127.0.0.1', port), timeout=5), server_hostname=DOMAIN)
        assert self.sock.selected_alpn_protocol() == 'h2'
        self.port = port
        self.conn = h2.connection.H2Connection(h2.config.H2Configuration(
            client_side=True, validate_outbound_headers=False,
            normalize_outbound_headers=False, validate_inbound_headers=False))
        self.conn.initiate_connection()
        self.sock.sendall(self.conn.data_to_send())
        self.settings = {}
        self.streams = {}
        while 8 not in self.settings:
            self.pump()
        assert self.settings[8] == 1, ('extended CONNECT not advertised', self.settings)

    def pump(self):
        got = self.sock.recv(65536)
        assert got, 'connection closed'
        for ev in self.conn.receive_data(got):
            st = self.streams.get(getattr(ev, 'stream_id', None))
            if isinstance(ev, h2.events.RemoteSettingsChanged):
                for k, v in ev.changed_settings.items():
                    self.settings[int(k)] = v.new_value
            elif isinstance(ev, h2.events.ResponseReceived) and st:
                st['headers'] = dict(ev.headers)
            elif isinstance(ev, h2.events.DataReceived):
                if st:
                    st['reader'].feed(ev.data)
                if ev.flow_controlled_length:
                    self.conn.acknowledge_received_data(ev.flow_controlled_length, ev.stream_id)
            elif isinstance(ev, h2.events.StreamEnded) and st:
                st['ended'] = True
            elif isinstance(ev, h2.events.StreamReset) and st:
                st['reset'] = ev.error_code
        out = self.conn.data_to_send()
        if out:
            self.sock.sendall(out)

    def open(self, path):
        sid = self.conn.get_next_available_stream_id()
        st = self.streams[sid] = {'headers': None, 'ended': False, 'reset': None, 'reader': FrameReader()}
        self.conn.send_headers(sid, [
            (':method', 'CONNECT'), (':protocol', 'websocket'), (':scheme', 'https'),
            (':path', path), (':authority', '%s:%d' % (DOMAIN, self.port)),
            ('sec-websocket-version', '13')])
        self.sock.sendall(self.conn.data_to_send())
        while st['headers'] is None and st['reset'] is None:
            self.pump()
        assert st['reset'] is None, ('stream reset', st['reset'])
        assert st['headers'].get(b':status') == b'200', st['headers']
        return sid

    def send(self, sid, data, end=False):
        self.conn.send_data(sid, data, end_stream=end)
        self.sock.sendall(self.conn.data_to_send())

    def recv_frame(self, sid):
        st = self.streams[sid]
        while True:
            f = st['reader'].frame()
            if f is not None:
                return f
            if st['ended'] or st['reset'] is not None:
                return None
            self.pump()

    def close(self):
        self.sock.close()


def origin_close_h1(port):
    """The origin sends its greeting and close frame, then closes."""
    sock, recv_frame = h1_tunnel(port, '/close')
    try:
        assert recv_frame() == (1, b'hello'), 'greeting lost'
        f = recv_frame()
        assert f == (8, CLOSE), ('close frame lost', f)
        assert recv_frame() is None, 'tunnel not closed'
    finally:
        sock.close()


def origin_close_h2(port, clean=True):
    c = H2Tunnels(port)
    try:
        for _ in range(3):
            sid = c.open('/close')
            assert c.recv_frame(sid) == (1, b'hello'), 'greeting lost'
            f = c.recv_frame(sid)
            assert f == (8, CLOSE), ('close frame lost', f)
            assert c.recv_frame(sid) is None, 'stream not ended'
            # A clean origin close ends the stream; a reset would make the
            # next HTTP/2 hop drop data it has not relayed yet.
            assert not clean or c.streams[sid]['ended'], ('reset instead of END_STREAM', c.streams[sid]['reset'])
        # The connection is still usable after the tunnels ended.
        sid = c.open('/chat')
        assert c.recv_frame(sid) == (1, b'hello')
    finally:
        c.close()


def client_end_stream_h2(port):
    """The client sends its close frame with END_STREAM (a half close):
    the origin's close frame must still come back."""
    c = H2Tunnels(port)
    try:
        for _ in range(3):
            sid = c.open('/chat')
            assert c.recv_frame(sid) == (1, b'hello')
            c.send(sid, ws_frame(b'last'))
            c.send(sid, ws_frame(CLOSE, 8), end=True)
            assert c.recv_frame(sid) == (1, b'last'), 'echo lost after END_STREAM'
            f = c.recv_frame(sid)
            assert f and f[0] == 8, ('close frame lost after END_STREAM', f, c.streams[sid]['reset'])
    finally:
        c.close()


def client_end_stream_bare_h2(port):
    """END_STREAM without a close frame: the strict origin only closes when
    it reads EOF, so the end must reach it (TLS origins included)."""
    c = H2Tunnels(port)
    try:
        sid = c.open('/chat')
        assert c.recv_frame(sid) == (1, b'hello')
        c.send(sid, ws_frame(b'bye'), end=True)
        assert c.recv_frame(sid) == (1, b'bye'), 'echo lost after END_STREAM'
        assert c.recv_frame(sid) is None, 'stream not ended'
        assert c.streams[sid]['ended'], ('reset instead of END_STREAM', c.streams[sid]['reset'])
    finally:
        c.close()


class H2ResetOriginHandler(socketserver.BaseRequestHandler):
    """HTTP/2 origin that ends each WebSocket stream with DATA and an
    immediate RST_STREAM(CANCEL), as kangle did before it sent END_STREAM."""

    def handle(self):
        sock = self.server.tls.wrap_socket(self.request, server_side=True)
        conn = h2.connection.H2Connection(h2.config.H2Configuration(
            client_side=False, validate_inbound_headers=False))
        conn.initiate_connection()
        conn.update_settings({h2.settings.SettingCodes.ENABLE_CONNECT_PROTOCOL: 1})
        sock.sendall(conn.data_to_send())
        while True:
            got = sock.recv(65536)
            if not got:
                return
            for ev in conn.receive_data(got):
                if isinstance(ev, h2.events.RequestReceived):
                    headers = dict(ev.headers)
                    sid = ev.stream_id
                    if headers.get(b':method') == b'CONNECT':
                        conn.send_headers(sid, [(':status', '200')])
                        conn.send_data(sid, ws_frame(b'hello', mask=False) + ws_frame(CLOSE, 8, mask=False))
                        conn.reset_stream(sid, h2.errors.ErrorCodes.CANCEL)
                    else:
                        conn.send_headers(sid, [(':status', '200'), ('content-length', '2')])
                        conn.send_data(sid, b'ok', end_stream=True)
                elif isinstance(ev, h2.events.DataReceived) and ev.flow_controlled_length:
                    conn.acknowledge_received_data(ev.flow_controlled_length, ev.stream_id)
            out = conn.data_to_send()
            if out:
                sock.sendall(out)


def start_h2_reset_origin(tls_ctx):
    srv = Origin(('127.0.0.2', 0), H2ResetOriginHandler)
    srv.tls = tls_ctx
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    return srv


def make_cert(root, name):
    subprocess.check_call([os.environ.get('KANGLE_TEST_OPENSSL', 'openssl'), 'req', '-x509',
                           '-nodes', '-newkey', 'rsa:2048', '-keyout', root+'/etc/'+name+'.key',
                           '-out', root+'/etc/'+name+'.pem', '-days', '2', '-subj', '/CN='+DOMAIN],
                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def free_port():
    s = socket.socket()
    s.bind(('127.0.0.1', 0))
    port = s.getsockname()[1]
    s.close()
    return port


def start_kangle(binary, root, hosts, port):
    for name in ('ext', 'bin', 'var', 'tmp', 'log', 'www'):
        os.makedirs(root+'/'+name)
    executable = copy_server(binary, root+'/bin')
    cfg = "<config><worker_thread>2</worker_thread><timeout rw='5' connect='2'/>"
    cfg += ("<listen ip='127.0.0.1' port='%d' type='https' certificate='etc/server.pem' "
            "certificate_key='etc/server.key' http2='1'/>" % port)
    cfg += "<request action='vhs'/><response action='allow'/><cache default='0' memory='0' disk='0'/>"
    for name, url in hosts:
        cfg += "<vh name='%s' doc_root='www' inherit='on'><host dir='%s'>%s.%s</host></vh>" % (
            name, url.replace('&', '&amp;'), name, DOMAIN)
    cfg += '</config>'
    with open(root+'/etc/config.xml', 'w') as f:
        f.write(cfg)
    with open(root+'/console.log', 'wb') as log:
        proc = subprocess.Popen([executable, '-n', '-g'], stdout=log, stderr=log)
    for _ in range(100):
        try:
            socket.create_connection(('127.0.0.1', port), timeout=.1).close()
            return proc, executable
        except OSError:
            if proc.poll() is not None:
                break
            time.sleep(.05)
    stop_server(proc, executable)
    raise AssertionError('server did not start: '+root)


def warm_h2_pool(port, host):
    """A plain request leaves a pooled HTTP/2 connection to the next hop."""
    sock = client_ctx(['http/1.1']).wrap_socket(
        socket.create_connection(('127.0.0.1', port), timeout=5), server_hostname=host)
    try:
        sock.sendall(('GET /warm HTTP/1.1\r\nHost: %s:%d\r\nConnection: close\r\n\r\n'
                      % (host, port)).encode())
        while sock.recv(65536):
            pass
    finally:
        sock.close()


def run(binary):
    global DOMAIN
    base = DOMAIN
    root = tempfile.mkdtemp(prefix='kangle-websocket-')
    edge_root, inner_root = root+'/edge', root+'/inner'
    for r in (edge_root, inner_root):
        os.makedirs(r+'/etc')
        make_cert(r, 'server')
    make_cert(edge_root, 'origin')
    origin_tls = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    origin_tls.load_cert_chain(edge_root+'/etc/origin.pem', edge_root+'/etc/origin.key')
    origin_tls.set_alpn_protocols(['http/1.1'])
    plain = start_origin(None)
    tls = start_origin(origin_tls)
    reset_tls = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    reset_tls.load_cert_chain(edge_root+'/etc/origin.pem', edge_root+'/etc/origin.key')
    reset_tls.set_alpn_protocols(['h2'])
    h2_reset = start_h2_reset_origin(reset_tls)
    plain_url = "http://127.0.0.2:%d/" % plain.server_address[1]
    edge_port, inner_port = free_port(), free_port()
    # 'h2' goes through a second kangle over HTTP/2, which speaks HTTP/1.1 to the origin.
    hosts = [
        ('plain', plain_url, plain),
        ('tls', "http://127.0.0.2:%d/?ssl=s" % tls.server_address[1], tls),
        ('h2', "http://127.0.0.1:%d/?ssl=sp&life_time=60" % inner_port, plain),
    ]
    reset_url = "http://127.0.0.2:%d/?ssl=sp&life_time=60" % h2_reset.server_address[1]
    procs = []
    failed = []

    def check(label, fn):
        try:
            fn(edge_port)
            print('PASS: %s' % label, flush=True)
        except Exception as e:
            failed.append(label)
            print('FAIL: %s: %r' % (label, e), flush=True)
    try:
        procs.append(start_kangle(binary, inner_root, [('h2', plain_url)], inner_port))
        procs.append(start_kangle(binary, edge_root, [(n, u) for n, u, _ in hosts] +
                                  [('h2reset', reset_url)], edge_port))
        for name, _, origin in hosts:
            DOMAIN = name + '.' + base
            if name == 'h2':
                warm_h2_pool(edge_port, DOMAIN)
            for proto, fn in (('h1', wss_h1), ('h2', wss_h2)):
                before = len(origin.seen)
                try:
                    key = fn(edge_port, '/chat?room=1')
                    seen = origin.seen[before:]
                    assert len(seen) == 1, ('origin requests', seen)
                    origin_key = seen[0][1].get('sec-websocket-key')
                    if key and name != 'h2':
                        assert origin_key == key, ('key changed', key, origin_key)
                    if key and name == 'h2':
                        # Proves the HTTP/2 hop was used: the inner kangle made a new key.
                        assert origin_key and origin_key != key, ('no HTTP/2 hop', key, origin_key)
                    print('PASS: wss %s -> %s origin' % (proto, name), flush=True)
                except Exception as e:
                    failed.append((proto, name))
                    print('FAIL: wss %s -> %s origin: %r; origin saw %r' % (
                        proto, name, e, origin.seen[before:]), flush=True)
            # Data in flight when one side ends the tunnel must still arrive.
            check('wss h1 -> %s origin closes first' % name, origin_close_h1)
            check('wss h2 -> %s origin closes first' % name, origin_close_h2)
            check('wss h2 -> %s client ends stream first' % name, client_end_stream_h2)
            check('wss h2 -> %s client ends stream without close frame' % name, client_end_stream_bare_h2)
            DOMAIN = base
        # An HTTP/2 next hop that resets right after its last DATA frame.
        DOMAIN = 'h2reset.' + base
        warm_h2_pool(edge_port, DOMAIN)
        check('wss h1 -> h2 next hop resets after data', origin_close_h1)
        check('wss h2 -> h2 next hop resets after data', lambda p: origin_close_h2(p, clean=False))
        DOMAIN = base
        for proc, _ in procs:
            assert proc.poll() is None, 'kangle exited'
    finally:
        DOMAIN = base
        for proc, executable in procs:
            stop_server(proc, executable)
        plain.shutdown()
        tls.shutdown()
        h2_reset.shutdown()
    if failed:
        print('WebSocket proxy regression FAILED: %r (%s)' % (failed, root), flush=True)
        sys.exit(1)
    print('WebSocket proxy regression passed', flush=True)


if __name__ == '__main__':
    run(os.path.abspath(sys.argv[1]))
