#!/usr/bin/env python3
"""Early Hints forwarding regressions; requires h2>=3.2,<4.

Run: python3 test/early_hints_integration.py /absolute/path/to/kangle
Uses isolated loopback listeners. Keeps logs and configuration for inspection.
"""
import collections
import os
import select
import shutil
import signal
import socket
import socketserver
import ssl
import subprocess
import sys
import tempfile
import threading
import time
from runtime_fixture import copy_runtime

import h2.config
import h2.connection
import h2.events
from http_protocol_integration import Server, chunks, exact, fields, port


gates = {}
hits = collections.Counter()
LINKS = [('link', '</style.css>; rel=preload; as=style'),
         ('link', '</app.js>; rel=preload; as=script')]


def hint_blocks(path):
    if path == '/none':
        return []
    if path == '/empty':
        return [[]]
    if path == '/oversize':
        return [[('x-large-%d' % i, 'a' * 2000) for i in range(40)]]
    if path.startswith('/many/'):
        return [[('x-sequence', str(i))] for i in range(int(path.rsplit('/', 1)[1]))]
    return [LINKS + [('x-early', 'one')], [('link', '</font.woff2>; rel=preload; as=font; crossorigin'), ('x-early', 'two')]]


def wait_gate(path):
    gate = gates.get(path)
    if gate:
        gate[0].set()
        if not gate[1].wait(8):
            raise AssertionError('client did not release origin gate')


class HttpOrigin(socketserver.StreamRequestHandler):
    def handle(self):
        self.request.settimeout(10)
        while True:
            first = self.rfile.readline()
            if not first:
                return
            method, path, _ = first.decode().split(' ', 2)
            request_headers = fields(self.rfile)
            hits[('h1', path)] += 1
            packet = b''
            for values in hint_blocks(path):
                packet += b'HTTP/1.1 103 Early Hints\r\n'
                if path == '/hop':
                    values = values + [('connection', ' X-Transient , x-another'), ('x-transient', 'discard'),
                                       ('x-another', 'discard'), ('x-transient-extra', 'retain'),
                                       ('content-length', '999'), ('transfer-encoding', 'chunked'),
                                       ('keep-alive', 'timeout=5'), ('trailer', 'x-final'), ('te', 'trailers'),
                                       ('upgrade', 'websocket'), ('proxy-connection', 'keep-alive')]
                if path == '/bad-name':
                    values = [('Bad Name', 'invalid')]
                packet += b''.join((name+': '+value+'\r\n').encode() for name, value in values) + b'\r\n'
            if path in gates or path == '/expect':
                self.request.sendall(packet)
                packet = b''
                wait_gate(path)
            premature = False
            if path == '/expect':
                premature = bool(select.select([self.request], [], [], .15)[0])
                self.request.sendall(b'HTTP/1.1 100 Continue\r\n\r\n')
            if path == '/disconnect':
                self.request.sendall(packet)
                return
            body = exact(self.rfile, int(request_headers.get(b'content-length', b'0')))
            if request_headers.get(b'transfer-encoding') == b'chunked':
                body, _ = chunks(self.rfile)
            payload = body if method == 'POST' else b'ok'
            if premature:
                payload = b'premature-upload'
            if path == '/trailers':
                packet += b'HTTP/1.1 200 OK\r\nCache-Control: no-store\r\nTransfer-Encoding: chunked\r\nTrailer: X-Final\r\n\r\n2\r\nok\r\n0\r\nX-Final: done\r\n\r\n'
            else:
                control = 'max-age=60' if path == '/cache' else 'no-store'
                packet += ('HTTP/1.1 200 OK\r\nCache-Control: %s\r\nContent-Length: %d\r\nX-Origin: final\r\n\r\n' % (control, len(payload))).encode()
                if method != 'HEAD':
                    packet += payload
            self.request.sendall(packet)


class H2Origin(socketserver.BaseRequestHandler):
    def handle(self):
        self.request.settimeout(10)
        cn = h2.connection.H2Connection(config=h2.config.H2Configuration(client_side=False, header_encoding='utf-8'))
        cn.initiate_connection()
        self.request.sendall(cn.data_to_send())
        requests, bodies = {}, {}
        while True:
            data = self.request.recv(65536)
            if not data:
                return
            for event in cn.receive_data(data):
                sid = getattr(event, 'stream_id', 0)
                if isinstance(event, h2.events.RequestReceived):
                    requests[sid] = dict(event.headers)
                    path = requests[sid][':path']
                    bodies[sid] = b''
                    hits[('h2', path)] += 1
                    if path == '/bad-end':
                        block = cn.encoder.encode([(':status', '103'), ('link', LINKS[0][1])])
                        self.request.sendall(cn.data_to_send()+len(block).to_bytes(3, 'big')+b'\x01\x05'+sid.to_bytes(4, 'big')+block)
                        return
                    for values in hint_blocks(path):
                        cn.send_headers(sid, [(':status', '103')]+values)
                    if path in gates or path == '/disconnect':
                        self.request.sendall(cn.data_to_send())
                        wait_gate(path)
                    if path == '/disconnect':
                        return
                elif isinstance(event, h2.events.DataReceived):
                    bodies[sid] += event.data
                    cn.acknowledge_received_data(event.flow_controlled_length, sid)
                elif isinstance(event, h2.events.StreamEnded):
                    path = requests[sid][':path']
                    method = requests[sid][':method']
                    payload = bodies[sid] if method == 'POST' else b'ok'
                    control = 'max-age=60' if path == '/cache' else 'no-store'
                    headers = [(':status', '200'), ('cache-control', control), ('content-length', str(len(payload))), ('x-origin', 'final')]
                    if path == '/trailers':
                        headers += [('trailer', 'x-final')]
                    cn.send_headers(sid, headers)
                    if method != 'HEAD' and payload:
                        cn.send_data(sid, payload)
                    if path == '/trailers':
                        cn.send_headers(sid, [('x-final', 'done')], end_stream=True)
                    else:
                        cn.end_stream(sid)
            outgoing = cn.data_to_send()
            if outgoing:
                self.request.sendall(outgoing)


class H1Client:
    def __init__(self, frontend, tls=False):
        self.sock = socket.create_connection(('127.0.0.1', frontend), timeout=5)
        if tls:
            ctx = ssl._create_unverified_context()
            ctx.set_alpn_protocols(['http/1.1'])
            self.sock = ctx.wrap_socket(self.sock)
        self.file = self.sock.makefile('rb')

    def request(self, host, path, method='GET', version='1.1', extra=''):
        self.sock.sendall(('%s %s HTTP/%s\r\nHost: %s\r\n%s\r\n' % (method, path, version, host, extra)).encode())

    def head(self):
        first = self.file.readline()
        if not first:
            raise EOFError('HTTP/1 peer closed')
        values = []
        while True:
            line = self.file.readline()
            if line == b'\r\n':
                break
            if not line:
                raise EOFError()
            key, value = line.rstrip(b'\r\n').split(b':', 1)
            values.append((key.decode().lower(), value.strip().decode()))
        return int(first.split()[1]), values

    def finish(self, method='GET'):
        info = []
        while True:
            status, values = self.head()
            if status >= 200:
                break
            info.append((status, values))
        trailer = {}
        headers = dict(values)
        if method == 'HEAD' or status in (204, 304):
            body = b''
        elif headers.get('transfer-encoding') == 'chunked':
            body, trailer = chunks(self.file)
        elif 'content-length' in headers:
            body = exact(self.file, int(headers['content-length']))
        else:
            body = self.file.read()
        return status, values, body, trailer, info

    def close(self):
        self.file.close()
        self.sock.close()


class H2Client:
    def __init__(self, frontend, tls=False):
        self.sock = socket.create_connection(('127.0.0.1', frontend), timeout=5)
        if tls:
            ctx = ssl._create_unverified_context()
            ctx.set_alpn_protocols(['h2'])
            self.sock = ctx.wrap_socket(self.sock)
            assert self.sock.selected_alpn_protocol() == 'h2'
        self.cn = h2.connection.H2Connection(config=h2.config.H2Configuration(client_side=True, header_encoding='utf-8'))
        self.cn.initiate_connection()
        self.flush()
        self.info, self.headers, self.bodies, self.trailers = {}, {}, {}, {}
        self.ended, self.reset = set(), set()

    def flush(self):
        outgoing = self.cn.data_to_send()
        if outgoing:
            self.sock.sendall(outgoing)

    def request(self, host, path, method='GET'):
        sid = self.cn.get_next_available_stream_id()
        self.cn.send_headers(sid, [(':method', method), (':scheme', 'https' if isinstance(self.sock, ssl.SSLSocket) else 'http'), (':authority', host), (':path', path), ('te', 'trailers')], end_stream=True)
        self.flush()
        return sid

    def receive(self, predicate):
        deadline = time.monotonic()+6
        while not predicate():
            self.sock.settimeout(max(.1, deadline-time.monotonic()))
            data = self.sock.recv(65536)
            if not data:
                raise EOFError('HTTP/2 peer closed')
            for event in self.cn.receive_data(data):
                sid = getattr(event, 'stream_id', 0)
                if isinstance(event, h2.events.InformationalResponseReceived):
                    self.info.setdefault(sid, []).append(event.headers)
                elif isinstance(event, h2.events.ResponseReceived):
                    self.headers[sid] = event.headers
                elif isinstance(event, h2.events.TrailersReceived):
                    self.trailers[sid] = dict(event.headers)
                elif isinstance(event, h2.events.DataReceived):
                    self.bodies[sid] = self.bodies.get(sid, b'')+event.data
                    self.cn.acknowledge_received_data(event.flow_controlled_length, sid)
                elif isinstance(event, h2.events.StreamEnded):
                    self.ended.add(sid)
                elif isinstance(event, h2.events.StreamReset):
                    self.reset.add(sid)
            self.flush()

    def done(self, sid):
        self.receive(lambda: sid in self.ended or sid in self.reset)

    def close(self):
        self.sock.close()


def run(binary):
    root = tempfile.mkdtemp(prefix='kangle-early-hints-')
    servers, clients, proc = [], [], None
    count = 0
    def check(ok, label):
        nonlocal count
        if not ok:
            raise AssertionError(label)
        count += 1
        print('PASS: '+label, flush=True)
    def client(kind, tls=False):
        result = (H1Client if kind == 'h1' else H2Client)(tlsport if tls else frontend, tls)
        clients.append(result)
        return result
    try:
        for name in ['etc', 'ext', 'bin', 'var', 'tmp', 'log', 'www']:
            os.makedirs(root+'/'+name)
        shutil.copy2(binary, root+'/bin/kangle')
        copy_runtime(binary, root+'/bin')
        for handler in [HttpOrigin, H2Origin]:
            server = Server(('127.0.0.1', 0), handler)
            servers.append(server)
            threading.Thread(target=server.serve_forever, daemon=True).start()
        frontend, tlsport = port(), port()
        subprocess.check_call(['openssl', 'req', '-x509', '-nodes', '-newkey', 'rsa:2048', '-keyout', root+'/etc/key.pem', '-out', root+'/etc/cert.pem', '-days', '2', '-subj', '/CN=localhost'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        cfg = "<config><worker_thread>2</worker_thread><timeout rw='8' connect='2'/>"
        cfg += "<listen ip='127.0.0.1' port='%dh' type='http'/><listen ip='127.0.0.1' port='%d' type='https' certificate='etc/cert.pem' certificate_key='etc/key.pem' http2='1'/>" % (frontend, tlsport)
        cfg += "<cache default='60' memory='16M' disk='0'/><request action='allow'><table name='BEGIN'>"
        for value in ('on', 'off', 'h2'):
            cfg += "<chain action='continue'><acl module='path' path='/rule-%s'/><mark module='early_hints' value='%s'/></chain>" % (value, value)
        cfg += "<chain action='continue'><acl module='path' path='/pending'/><mark module='add_response_header' attr='X-Pending' val='final-only'/></chain></table></request><response action='allow'/>"
        for origin, server in zip(('h1', 'h2'), servers):
            cfg += "<server name='%s' host='127.0.0.1' port='%d%s' proto='http' life_time='30'/>" % (origin, server.server_address[1], 'h' if origin == 'h2' else '')
            for mode in ('off', 'on', 'h2'):
                cfg += "<vh name='%s-%s' doc_root='www' inherit='on'%s><host>%s.%s.test</host><map path='/' extend='server:%s' confirm_file='0' allow_method='*'/></vh>" % (origin, mode, '' if mode == 'off' else " early_hints='%s'" % mode, origin, mode, origin)
        with open(root+'/etc/config.xml', 'w') as output:
            output.write(cfg+'</config>')
        log = open(root+'/console.log', 'wb')
        proc = subprocess.Popen([root+'/bin/kangle', '-n', '-g'], stdout=log, stderr=log)
        for _ in range(100):
            try:
                socket.create_connection(('127.0.0.1', frontend), timeout=.1).close()
                break
            except OSError:
                time.sleep(.05)
        else:
            raise AssertionError('startup failed')

        for origin in ('h1', 'h2'):
            for kind in ('h1', 'h2'):
                for mode in ('off', 'on', 'h2'):
                    host = '%s.%s.test' % (origin, mode)
                    label = '%s origin -> %s client, %s' % (origin, kind, mode)
                    enabled = mode == 'on' or (mode == 'h2' and kind == 'h2')
                    path = '/gate/'+str(count)
                    gate = gates[path] = (threading.Event(), threading.Event())
                    cn = client(kind)
                    sid = cn.request(host, path)
                    if enabled:
                        if kind == 'h1':
                            info = [cn.head(), cn.head()]
                            check(all(status == 103 for status, _ in info) and [v for k, v in info[0][1] if k == 'link'] == [v for k, v in LINKS], label+' forwards both hints and duplicate Link fields before final response')
                        else:
                            cn.receive(lambda: len(cn.info.get(sid, [])) == 2)
                            info = cn.info[sid]
                            check(sid not in cn.headers and sid not in cn.ended and [v for k, v in info[0] if k == 'link'] == [v for k, v in LINKS], label+' sends informational HEADERS without END_STREAM before final response')
                    else:
                        check(gate[0].wait(3), label+' origin emits hints')
                    gate[1].set()
                    if kind == 'h1':
                        status, headers, body, _, tail = cn.finish()
                        check(status == 200 and body == b'ok' and not tail and 'x-early' not in dict(headers), label+' final response remains intact and hints stay isolated')
                    else:
                        cn.done(sid)
                        check(sid not in cn.reset and cn.bodies.get(sid) == b'ok' and dict(cn.headers[sid])[':status'] == '200' and 'x-early' not in dict(cn.headers[sid]) and (enabled or sid not in cn.info), label+' final response remains intact and hints stay isolated')
                    cn.close()

                cn = client(kind, tls=True)
                for path, expected in [('/rule-on', 2), ('/empty', 1), ('/none', 0), ('/pending', 2), ('/oversize', 0), ('/many/8', 8), ('/rule-off', 0), ('/rule-h2', 2 if kind == 'h2' else 0), ('/trailers', 2)]:
                    host = origin+'.off.test' if path == '/rule-on' else origin+'.on.test'
                    sid = cn.request(host, path, extra='TE: trailers\r\n') if kind == 'h1' else cn.request(host, path)
                    if kind == 'h1':
                        status, values, body, trailer, info = cn.finish()
                        check(status == 200 and body == b'ok' and len(info) == expected and all(s == 103 for s, _ in info), origin+' -> TLS '+kind+' '+path+' on reused connection')
                        if path == '/pending':
                            check(dict(values).get('x-pending') == 'final-only' and all('x-pending' not in dict(v) for _, v in info), 'pending final header survives HTTP/1 hints')
                        if path == '/trailers':
                            check(trailer.get(b'x-final') == b'done', 'trailers survive HTTP/1 hints')
                    else:
                        cn.done(sid)
                        check(sid not in cn.reset and dict(cn.headers[sid])[':status'] == '200' and cn.bodies.get(sid) == b'ok' and len(cn.info.get(sid, [])) == expected, origin+' -> TLS '+kind+' '+path+' on reused connection')
                        if path == '/pending':
                            check(dict(cn.headers[sid]).get('x-pending') == 'final-only' and all('x-pending' not in dict(v) for v in cn.info[sid]), 'pending final header survives HTTP/2 hints')
                        if path == '/trailers':
                            check(cn.trailers[sid].get('x-final') == 'done', 'trailers survive HTTP/2 hints')
                cn.close()

                cn = client(kind)
                sid = cn.request(origin+'.on.test', '/head', method='HEAD')
                if kind == 'h1':
                    status, values, body, _, info = cn.finish(method='HEAD')
                    length = dict(values).get('content-length')
                    check(status == 200 and not body and len(info) == 2, origin+' -> '+kind+' HEAD receives hints and a final response without a body')
                    cn.request(origin+'.off.test', '/head', method='HEAD')
                    status, values, body, _, info = cn.finish(method='HEAD')
                    check(status == 200 and not body and not info and dict(values).get('content-length') == length, origin+' -> '+kind+' HEAD framing matches disabled forwarding')
                else:
                    cn.done(sid)
                    length = dict(cn.headers[sid]).get('content-length')
                    check(sid not in cn.reset and not cn.bodies.get(sid) and len(cn.info[sid]) == 2, origin+' -> '+kind+' HEAD receives hints and a final response without a body')
                    sid = cn.request(origin+'.off.test', '/head', method='HEAD')
                    cn.done(sid)
                    check(sid not in cn.reset and not cn.bodies.get(sid) and not cn.info.get(sid) and dict(cn.headers[sid]).get('content-length') == length, origin+' -> '+kind+' HEAD framing matches disabled forwarding')
                cn.close()

            cn = client('h2')
            streams = [cn.request(origin+'.on.test', '/parallel/'+str(i)) for i in range(24)]
            for sid in streams:
                cn.done(sid)
            check(all(sid not in cn.reset and len(cn.info[sid]) == 2 and cn.bodies[sid] == b'ok' for sid in streams), origin+' origin: 24 concurrent streams keep hints isolated')
            cn.close()

            cn = client('h1')
            cn.request(origin+'.on.test', '/http10', version='1.0')
            status, _, body, _, info = cn.finish()
            check(status == 200 and body == b'ok' and not info, origin+' origin: HTTP/1.0 skips hints and preserves final response')
            cn.close()

            for kind in ('h1', 'h2'):
                cn = client(kind)
                sid = cn.request(origin+'.on.test', '/many/9')
                if kind == 'h1':
                    status, _, _, _, info = cn.finish()
                    check(status == 504 and len(info) == 8, 'excess hints fail safely for '+origin+' -> '+kind)
                else:
                    cn.done(sid)
                    check(dict(cn.headers.get(sid, [] )).get(':status') == '504' and len(cn.info.get(sid, [])) == 8, 'excess hints fail safely for '+origin+' -> '+kind)
                cn.close()

            for kind in ('h1', 'h2'):
                cn = client(kind)
                sid = cn.request(origin+'.on.test', '/disconnect')
                if kind == 'h1':
                    status, _, _, _, info = cn.finish()
                    check(status == 504 and len(info) >= 2, origin+' -> '+kind+' upstream failure after hints still produces an error response')
                else:
                    cn.done(sid)
                    check(dict(cn.headers.get(sid, [])).get(':status') == '504' and len(cn.info.get(sid, [])) >= 2, origin+' -> '+kind+' upstream failure after hints still produces an error response')
                cn.close()

        for kind in ('h1', 'h2'):
            cn = client(kind)
            sid = cn.request('h1.on.test', '/hop')
            if kind == 'h1':
                _, _, _, _, info = cn.finish()
                first = dict(info[0][1])
            else:
                cn.done(sid)
                first = dict(cn.info[sid][0])
            check(first.get('x-transient-extra') == 'retain' and not any(name in first for name in ['connection', 'x-transient', 'x-another', 'content-length', 'transfer-encoding', 'keep-alive', 'trailer', 'te', 'upgrade', 'proxy-connection']), kind+' hints remove hop-by-hop and framing fields')
            cn.close()

        cn = client('h1')
        cn.request('h1.on.test', '/expect', method='POST', extra='Expect: 100-continue\r\nContent-Length: 4\r\n')
        check(cn.head()[0] == 103 and cn.head()[0] == 103 and cn.head()[0] == 100, '103 and 100 Continue remain distinct')
        cn.sock.sendall(b'body')
        check(cn.finish()[2] == b'body', '103 does not authorize premature Expect upload')
        cn.close()

        for origin in ('h1', 'h2'):
            cn = client('h2')
            first = cn.request(origin+'.on.test', '/cache')
            cn.done(first)
            second = cn.request(origin+'.on.test', '/cache')
            cn.done(second)
            check(len(cn.info.get(first, [])) == 2 and not cn.info.get(second) and cn.bodies[second] == b'ok' and hits[(origin, '/cache')] == 1, origin+' origin: final response caches normally without replaying hints')
            cn.close()

        cn = client('h2')
        sid = cn.request('h2.on.test', '/bad-end')
        cn.done(sid)
        check(sid in cn.reset or dict(cn.headers.get(sid, [])).get(':status') == '504', 'invalid 103 END_STREAM rejected')
        cn.close()
        cn = client('h1')
        cn.request('h1.on.test', '/bad-name')
        status, _, _, _, info = cn.finish()
        check(status == 504 and not info, 'invalid hint field rejected before forwarding')
        cn.close()
        print('%d Early Hints checks passed; logs: %s' % (count, root), flush=True)
    finally:
        for _, gate in gates.values():
            gate.set()
        for cn in clients:
            cn.close()
        if proc:
            proc.send_signal(signal.SIGTERM)
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
        for server in servers:
            server.shutdown()
            server.server_close()
        print('Test directory: '+root, flush=True)


if __name__ == '__main__':
    run(os.path.abspath(sys.argv[1]))
