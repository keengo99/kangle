#!/usr/bin/env python3
"""Check that wss through a CDN (reverse proxy) site is not delayed by TCP_CORK.

kangle corks the client socket while it reads a request head. A WebSocket
tunnel used to keep that cork, and HTTP/2 streams only uncorked on the final
frame, so on Linux every handshake and every message waited for the 200ms
cork ceiling. Clients that open many wss connections in a row (or several
streams on one HTTP/2 connection) then looked stuck.

Clients connect with wss over HTTP/1.1 and over HTTP/2 extended CONNECT
(RFC 8441), with TCP_NODELAY like browsers, to a plain origin and through a
second kangle reached over HTTP/2.

Usage: python3 test/websocket_latency_regression.py /absolute/path/to/kangle
Needs the h2 package. Uses an isolated prefix and does not change the
installed configuration.
"""
import base64
import os
import socket
import sys
import tempfile
import time

import h2.config
import h2.connection
import h2.events
import h2.exceptions

import websocket_proxy_integration as wsp
from runtime_fixture import stop_server

CONNECTIONS = 8
MESSAGES = 5
# The cork ceiling is 200ms; a fixed build answers in a few milliseconds.
LIMIT = 0.1


def tls(port, host, alpn):
    raw = socket.create_connection(('127.0.0.1', port), timeout=5)
    raw.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    return wsp.client_ctx(alpn).wrap_socket(raw, server_hostname=host)


def h1_session(port, host, times):
    t = time.time()
    sock = tls(port, host, ['http/1.1'])
    try:
        key = base64.b64encode(os.urandom(16)).decode()
        sock.sendall(('GET /chat HTTP/1.1\r\nHost: %s:%d\r\nUpgrade: websocket\r\n'
                      'Connection: Upgrade\r\nSec-WebSocket-Key: %s\r\n'
                      'Sec-WebSocket-Version: 13\r\n\r\n' % (host, port, key)).encode())
        reader = wsp.FrameReader()
        data = b''
        while b'\r\n\r\n' not in data:
            got = sock.recv(65536)
            assert got, ('closed before response', data)
            data += got
        head, rest = data.split(b'\r\n\r\n', 1)
        assert b' 101 ' in head.split(b'\r\n')[0], head
        reader.feed(rest)

        def recv_frame():
            while True:
                f = reader.frame()
                if f is not None:
                    return f
                got = sock.recv(65536)
                assert got, 'connection closed'
                reader.feed(got)
        assert recv_frame() == (1, b'hello')
        times.append(time.time() - t)
        for i in range(MESSAGES):
            m = b'm%d' % i
            t = time.time()
            sock.sendall(wsp.ws_frame(m))
            assert recv_frame() == (1, m)
            times.append(time.time() - t)
        sock.sendall(wsp.ws_frame(b'\x03\xe8', 8))
        assert recv_frame()[0] == 8
    finally:
        sock.close()


def h2_session(port, host, times):
    """Several WebSocket streams opened one after another on one connection."""
    sock = tls(port, host, ['h2'])
    try:
        assert sock.selected_alpn_protocol() == 'h2'
        conn = h2.connection.H2Connection(h2.config.H2Configuration(
            client_side=True, validate_outbound_headers=False,
            normalize_outbound_headers=False, validate_inbound_headers=False))
        conn.initiate_connection()
        sock.sendall(conn.data_to_send())
        settings = {}
        streams = {}

        def pump():
            got = sock.recv(65536)
            assert got, 'connection closed'
            for ev in conn.receive_data(got):
                st = streams.get(getattr(ev, 'stream_id', None))
                if isinstance(ev, h2.events.RemoteSettingsChanged):
                    for k, v in ev.changed_settings.items():
                        settings[int(k)] = v.new_value
                elif isinstance(ev, h2.events.ResponseReceived) and st:
                    st['headers'] = dict(ev.headers)
                elif isinstance(ev, h2.events.DataReceived):
                    if st:
                        st['reader'].feed(ev.data)
                    if ev.flow_controlled_length:
                        conn.acknowledge_received_data(ev.flow_controlled_length, ev.stream_id)
                elif isinstance(ev, h2.events.StreamReset) and st:
                    st['reset'] = ev.error_code
            out = conn.data_to_send()
            if out:
                sock.sendall(out)
        while 8 not in settings:
            pump()
        assert settings[8] == 1, ('extended CONNECT not advertised', settings)
        for _ in range(CONNECTIONS):
            t = time.time()
            sid = conn.get_next_available_stream_id()
            st = streams[sid] = {'headers': None, 'reset': None, 'reader': wsp.FrameReader()}
            conn.send_headers(sid, [
                (':method', 'CONNECT'), (':protocol', 'websocket'), (':scheme', 'https'),
                (':path', '/chat'), (':authority', '%s:%d' % (host, port)),
                ('sec-websocket-version', '13')])
            sock.sendall(conn.data_to_send())

            def recv_frame():
                while True:
                    f = st['reader'].frame()
                    if f is not None:
                        return f
                    assert st['reset'] is None, ('stream reset', st['reset'])
                    pump()
            while st['headers'] is None and st['reset'] is None:
                pump()
            assert st['headers'] and st['headers'].get(b':status') == b'200', st
            assert recv_frame() == (1, b'hello')
            times.append(time.time() - t)
            for i in range(MESSAGES):
                m = b'm%d' % i
                t = time.time()
                conn.send_data(sid, wsp.ws_frame(m))
                sock.sendall(conn.data_to_send())
                assert recv_frame() == (1, m)
                times.append(time.time() - t)
            conn.send_data(sid, wsp.ws_frame(b'\x03\xe8', 8))
            sock.sendall(conn.data_to_send())
            assert recv_frame()[0] == 8
            try:
                conn.end_stream(sid)
            except h2.exceptions.StreamClosedError:
                pass
            sock.sendall(conn.data_to_send())
    finally:
        sock.close()


def run(binary):
    base = wsp.DOMAIN
    root = tempfile.mkdtemp(prefix='kangle-websocket-latency-')
    edge_root, inner_root = root+'/edge', root+'/inner'
    for r in (edge_root, inner_root):
        os.makedirs(r+'/etc')
        wsp.make_cert(r, 'server')
    origin = wsp.start_origin(None)
    origin_url = 'http://127.0.0.2:%d/' % origin.server_address[1]
    edge_port, inner_port = wsp.free_port(), wsp.free_port()
    hosts = [('plain', origin_url),
             ('h2', 'http://127.0.0.1:%d/?ssl=sp&life_time=60' % inner_port)]
    procs = []
    failed = []
    try:
        procs.append(wsp.start_kangle(binary, inner_root, [('h2', origin_url)], inner_port))
        procs.append(wsp.start_kangle(binary, edge_root, hosts, edge_port))
        for name, _ in hosts:
            host = name + '.' + base
            if name == 'h2':
                wsp.warm_h2_pool(edge_port, host)
            for proto in ('h1', 'h2'):
                times = []
                try:
                    if proto == 'h1':
                        for _ in range(CONNECTIONS):
                            h1_session(edge_port, host, times)
                    else:
                        h2_session(edge_port, host, times)
                    times.sort()
                    median = times[len(times) // 2]
                    assert median < LIMIT, ('median %.3fs, max %.3fs over %d round trips' %
                                            (median, times[-1], len(times)))
                    print('PASS: wss %s -> %s origin: median %.1fms, %d round trips' % (
                        proto, name, median * 1000, len(times)), flush=True)
                except Exception as e:
                    failed.append((proto, name))
                    print('FAIL: wss %s -> %s origin: %r' % (proto, name, e), flush=True)
        for proc, _ in procs:
            assert proc.poll() is None, 'kangle exited'
    finally:
        for proc, executable in procs:
            stop_server(proc, executable)
        origin.shutdown()
    if failed:
        print('WebSocket latency regression FAILED: %r (%s)' % (failed, root), flush=True)
        sys.exit(1)
    print('WebSocket latency regression passed', flush=True)


if __name__ == '__main__':
    run(os.path.abspath(sys.argv[1]))
