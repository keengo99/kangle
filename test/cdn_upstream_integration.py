#!/usr/bin/env python3
"""Check legacy CDN port inheritance and Host rewriting with a local origin.

Usage: python3 test/cdn_upstream_integration.py /absolute/path/to/kangle
Uses an isolated prefix and does not change the installed configuration.
"""
import http.client
import http.server
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time
from runtime_fixture import copy_server, stop_server


class Origin(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        self.server.requests += 1
        body = (self.headers['Host']+' '+self.path).encode()
        self.send_response(200)
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args):
        pass


def run(binary):
    origin = http.server.HTTPServer(('127.0.0.2', 0), Origin)
    port = origin.server_port
    origin.requests = 0
    threading.Thread(target=origin.serve_forever, daemon=True).start()
    domain = 'cdn.example.test'
    rule = '<mark module="host_rewrite" reg_host="(.*)" host="$1.aliyuncs.com" life_time="0" port="0" rewrite="1"/>'
    try:
        for where, inherited in (('none', True), ('global', True), ('vhost', True), ('global', False)):
            root = tempfile.mkdtemp(prefix='kangle-cdn-upstream-')
            for name in ('etc', 'ext', 'bin', 'var', 'tmp', 'log', 'www'):
                os.makedirs(root+'/'+name)
            executable = copy_server(binary, root+'/bin')
            chain = "<table name='BEGIN'><chain action='continue'>"+rule+'</chain></table>'
            bound_host = domain+'.aliyuncs.com' if where == 'global' else domain
            cfg = "<config><worker_thread>2</worker_thread><timeout rw='3' connect='2'/>"
            cfg += "<listen ip='127.0.0.1' port='%d' type='http'/>" % port
            cfg += "<request action='vhs'>%s</request>" % (chain if where == 'global' else '')
            cfg += "<response action='allow'/><cache default='0' memory='0' disk='0'/>"
            cfg += "<vh name='cdn' doc_root='www' inherit='on'>"
            cfg += "<host dir='http://127.0.0.2:%d/'>%s</host>" % (0 if inherited else port, bound_host)
            if where == 'vhost':
                cfg += "<request action='allow'>"+chain+'</request>'
            cfg += '</vh></config>'
            with open(root+'/etc/config.xml', 'w') as f:
                f.write(cfg)
            with open(root+'/console.log', 'wb') as log:
                proc = subprocess.Popen([executable, '-n', '-g'], stdout=log, stderr=log)
            try:
                for _ in range(100):
                    try:
                        socket.create_connection(('127.0.0.1', port), timeout=.1).close()
                        break
                    except OSError:
                        assert proc.poll() is None, 'server exited: '+root
                        time.sleep(.05)
                else:
                    raise AssertionError('server did not start: '+root)
                conn = http.client.HTTPConnection('127.0.0.1', port, timeout=5)
                conn.request('GET', '/object?version=1', headers={'Host': domain+':'+str(port)})
                res = conn.getresponse()
                body = res.read()
                conn.close()
                rewritten = domain if where == 'none' else domain+'.aliyuncs.com'
                expected = (rewritten+':'+str(port)+' /object?version=1').encode()
                assert res.status == 200 and body == expected, (res.status, body, root)
                requests = origin.requests
                conn = http.client.HTTPConnection('127.0.0.1', port, timeout=5)
                try:
                    conn.request('GET', '/', headers={'Host': domain+':0'})
                    res = conn.getresponse()
                    res.read()
                    assert res.status == 400, ('client port 0 was accepted', res.status, root)
                except (http.client.RemoteDisconnected, ConnectionResetError):
                    pass  # Invalid authority can close the connection before routing.
                finally:
                    conn.close()
                assert origin.requests == requests, 'invalid client authority reached the origin'
                print('PASS: %s Host rewrite, %s upstream port, strict client validation' %
                    (where, 'inherited' if inherited else 'explicit'), flush=True)
            finally:
                stop_server(proc, executable)
        print('CDN upstream regression passed', flush=True)
    finally:
        origin.shutdown()
        origin.server_close()


if __name__ == '__main__':
    run(os.path.abspath(sys.argv[1]))
