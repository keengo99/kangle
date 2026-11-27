#!/usr/bin/env python3
"""Invalid upstream dates must never become ETag pointers.

Run on Linux: python3 test/cache_date_regression.py /path/to/build/kangle
Uses isolated HTTP/FastCGI origins, loopback ports and a temporary installation.
"""
import http.client
import os
import socketserver
import struct
import subprocess
import sys
import tempfile
import time
from email.utils import formatdate

from runtime_fixture import copy_server, stop_server
from upstream_functional_regression import exact, fcgi_params, fcgi_record, free_port, start_server


VALID = 'Sun, 06 Nov 1994 08:49:37 GMT'
CASES = {
    'invalid': ([('Last-Modified', 'invalid')], None, None),
    'minus-one': ([('Last-Modified', '-1')], None, None),
    'zero': ([('Last-Modified', '0')], None, None),
    'empty': ([('Last-Modified', '')], None, None),
    'bad-day': ([('Last-Modified', 'Thu, 32 Jan 2026 00:00:00 GMT')], None, None),
    'bad-hour': ([('Last-Modified', 'Thu, 01 Jan 2026 25:00:00 GMT')], None, None),
    'epoch': ([('Last-Modified', 'Thu, 01 Jan 1970 00:00:00 GMT')], None, None),
    'valid': ([('Last-Modified', VALID)], VALID, None),
    'invalid-then-valid': ([('Last-Modified', 'invalid'), ('Last-Modified', VALID)], VALID, None),
    'valid-then-invalid': ([('Last-Modified', VALID), ('Last-Modified', 'invalid')], VALID, None),
    'etag-first': ([('ETag', '"good"'), ('Last-Modified', 'invalid')], None, '"good"'),
    'etag-last': ([('Last-Modified', 'invalid'), ('ETag', '"good"')], None, '"good"'),
    'etag-before-date': ([('ETag', '"good"'), ('Last-Modified', VALID)], VALID, '"good"'),
    'date-before-etag': ([('Last-Modified', VALID), ('ETag', '"good"')], VALID, '"good"'),
    'invalid-date': ([('Date', 'invalid'), ('Last-Modified', VALID)], VALID, None),
    'invalid-expires': ([('Expires', '0'), ('Last-Modified', VALID)], VALID, None),
    'valid-expires': ([('Expires', 'Thu, 01 Jan 2037 00:00:00 GMT'), ('Last-Modified', VALID)], VALID, None),
}


def response_headers(path):
    name = path.split('?', 1)[0].rstrip('/').rsplit('/', 1)[-1]
    fields = CASES[name][0][:]
    if not any(key == 'Date' for key, _ in fields):
        fields.append(('Date', formatdate(usegmt=True)))
    if name not in ('invalid-expires', 'valid-expires'):
        fields.append(('Cache-Control', 'public, max-age=600'))
    body = ('date-regression:' + name).encode()
    fields.append(('Content-Length', str(len(body))))
    head = ''.join('%s: %s\r\n' % field for field in fields).encode() + b'\r\n'
    return head, body


class HttpOrigin(socketserver.StreamRequestHandler):
    def handle(self):
        self.request.settimeout(10)
        while True:
            line = self.rfile.readline()
            if not line:
                return
            method, path, _ = line.decode().strip().split(' ', 2)
            while True:
                line = self.rfile.readline()
                if not line:
                    return
                if line == b'\r\n':
                    break
            head, body = response_headers(path)
            self.request.sendall(b'HTTP/1.1 200 OK\r\n' + head + (b'' if method == 'HEAD' else body))


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
                if kind == 5 and not data:
                    break
            values = fcgi_params(params)
            head, body = response_headers(values['REQUEST_URI'])
            stdout = b'Status: 200 OK\r\n' + head + (b'' if values['REQUEST_METHOD'] == 'HEAD' else body)
            self.request.sendall(fcgi_record(6, stdout) + fcgi_record(6, b'') + fcgi_record(3, b'\0' * 8))


def run(binary):
    root = tempfile.mkdtemp(prefix='kangle-cache-date-')
    proc = None
    servers = []
    checks = 0

    def check(condition, message):
        nonlocal checks
        if not condition:
            raise AssertionError(message)
        checks += 1
        print('PASS: ' + message, flush=True)

    try:
        for folder in ('bin', 'etc', 'ext', 'var', 'tmp', 'www'):
            os.makedirs(os.path.join(root, folder))
        installed = copy_server(binary, root + '/bin')
        http_origin = start_server(HttpOrigin)
        fcgi = start_server(FastcgiOrigin)
        servers.extend((http_origin, fcgi))
        port = free_port()
        with open(root + '/etc/config.xml', 'w') as f:
            f.write('''<config><worker_thread>2</worker_thread><timeout rw='5' connect='2'/>
<listen ip='127.0.0.1' port='{port}' type='http'/>
<cache default='1' memory='16M' disk='0'/>
<server name='http' host='127.0.0.1' port='{http}' proto='http' life_time='30'/>
<server name='fcgi' host='127.0.0.1' port='{fcgi}' proto='fastcgi' life_time='30'/>
<request action='allow'><table name='BEGIN'><chain action='continue'>
<mark module='flag' x_cache='1'/></chain></table></request><response action='allow'/>
<vh name='date-test' doc_root='www' inherit='on'><host>localhost</host>
<map path='/fcgi' extend='server:fcgi' confirm_file='0' allow_method='*'/>
<map path='/' extend='server:http' confirm_file='0' allow_method='*'/></vh>
</config>'''.format(port=port, http=http_origin.server_address[1], fcgi=fcgi.server_address[1]))
        with open(root + '/console.log', 'wb') as log:
            proc = subprocess.Popen([installed, '-n', '-g'], stdout=log, stderr=log)

            def request(path, method='GET', headers=None):
                cn = http.client.HTTPConnection('127.0.0.1', port, timeout=10)
                try:
                    cn.request(method, path, headers=dict({'Host': 'localhost'}, **(headers or {})))
                    resp = cn.getresponse()
                    return resp.status, dict((k.lower(), v) for k, v in resp.getheaders()), resp.read()
                finally:
                    cn.close()

            for _ in range(100):
                try:
                    if request('/http/valid')[0] == 200:
                        break
                except (OSError, http.client.HTTPException):
                    if proc.poll() is not None:
                        raise AssertionError('kangle exited before startup')
                    time.sleep(0.05)
            else:
                raise AssertionError('kangle did not start')

            for protocol in ('http', 'fcgi'):
                for name, (_, modified, etag) in CASES.items():
                    path = '/%s/%s?test=1' % (protocol, name)
                    for repeat in range(3):
                        status, headers, body = request(path)
                        check(status == 200 and body == ('date-regression:' + name).encode(),
                              '%s %s response %d' % (protocol, name, repeat))
                        check(headers.get('last-modified') == modified and headers.get('etag') == etag,
                              '%s %s validator %d' % (protocol, name, repeat))
                        if repeat:
                            hit = headers.get('x-cache', '').startswith('HIT ')
                            check(hit == (name != 'invalid-expires'), '%s %s cache %d' % (protocol, name, repeat))
                    status, _, body = request(path, 'HEAD')
                    check(status == 200 and not body, '%s %s HEAD' % (protocol, name))
                    if etag:
                        status, _, body = request(path, headers={'If-None-Match': etag})
                        check(status == 304 and not body, '%s %s ETag conditional' % (protocol, name))
                    if modified and name != 'invalid-expires':
                        status, _, body = request(path, headers={'If-Modified-Since': modified})
                        check(status == 304 and not body, '%s %s time conditional' % (protocol, name))
                        if etag:
                            status, _, body = request(path, headers={
                                'If-None-Match': '"different"', 'If-Modified-Since': modified})
                            check(status == 200 and body, '%s %s mismatched ETag takes precedence' % (protocol, name))
                            status, _, body = request(path, headers={
                                'If-None-Match': etag, 'If-Modified-Since': 'Sat, 01 Jan 1994 00:00:00 GMT'})
                            check(status == 304 and not body, '%s %s matching ETag takes precedence' % (protocol, name))
                    elif not modified:
                        status, _, body = request(path, headers={'If-Modified-Since': VALID})
                        check(status == 200 and body, '%s %s invalid time is not a validator' % (protocol, name))
            check(proc.poll() is None, 'kangle survives malformed dates and cache hits')
        print('SUCCESS: %d checks; artifacts: %s' % (checks, root), flush=True)
    except Exception:
        print('FAILED; artifacts: ' + root, file=sys.stderr)
        raise
    finally:
        if proc is not None:
            stop_server(proc, installed)
        for server in servers:
            server.shutdown()
            server.server_close()


if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    run(os.path.abspath(sys.argv[1]))
