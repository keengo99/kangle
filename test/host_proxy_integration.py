#!/usr/bin/env python3
"""Check host/self_ports reverse proxy rules against a distinct local origin."""
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
        body = ('origin '+self.headers['Host']+' '+self.path).encode()
        self.send_response(200)
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args):
        pass


def run(binary):
    origin = http.server.HTTPServer(('127.0.0.2', 0), Origin)
    origin.requests = 0
    port = origin.server_port
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        other_port = sock.getsockname()[1]
    threading.Thread(target=origin.serve_forever, daemon=True).start()
    domain = 'cn.kinqin.com'
    try:
        for rewrite in (False, True):
            root = tempfile.mkdtemp(prefix='kangle-host-proxy-')
            proc = None
            try:
                for name in ('etc', 'ext', 'bin', 'var', 'tmp', 'log', 'www'):
                    os.makedirs(root+'/'+name)
                executable = copy_server(binary, root+'/bin')
                with open(root+'/www/probe.txt', 'w') as f:
                    f.write('local')
                cfg = "<config><worker_thread>2</worker_thread><timeout rw='3' connect='2'/>"
                cfg += "<listen ip='127.0.0.1' port='%d' type='http'/>" % port
                cfg += "<listen ip='127.0.0.1' port='%d' type='http'/>" % other_port
                cfg += "<request action='vhs'><table name='BEGIN'><chain action='continue'>"
                cfg += "<acl module='host' split='|' v='%s'/>" % domain
                cfg += "<acl module='self_ports' split='|' v='%d|3313'/>" % port
                cfg += "<mark host='127.0.0.2' life_time='0' module='host' port='0' proxy='1'%s/>" % (" rewrite='1'" if rewrite else '')
                cfg += "</chain></table></request><response action='allow'/>"
                cfg += "<cache default='0' memory='0' disk='0'/><vhs><mime_type ext='*' type='text/plain'/></vhs>"
                cfg += "<vh name='local' doc_root='www' inherit='on'><host>%s</host><host>other.test</host></vh></config>" % domain
                with open(root+'/etc/config.xml', 'w') as f:
                    f.write(cfg)
                with open(root+'/console.log', 'wb') as log:
                    proc = subprocess.Popen([executable, '-n', '-g'], stdout=log, stderr=log)
                for _ in range(100):
                    try:
                        socket.create_connection(('127.0.0.1', port), timeout=.1).close()
                        break
                    except OSError:
                        assert proc.poll() is None, 'server exited: '+root
                        time.sleep(.05)
                else:
                    raise AssertionError('server did not start: '+root)
                def request(host, target_port=port):
                    conn = http.client.HTTPConnection('127.0.0.1', target_port, timeout=5)
                    conn.request('GET', '/probe.txt?source=1', headers={'Host': host+':'+str(port)})
                    res = conn.getresponse()
                    body = res.read()
                    conn.close()
                    return res.status, body
                expected_host = '127.0.0.2' if rewrite else domain
                expected = ('origin '+expected_host+':'+str(port)+' /probe.txt?source=1').encode()
                result = request(domain)
                assert result == (200, expected), (result, root)
                requests = origin.requests
                assert request('other.test') == (200, b'local'), root
                assert request(domain, other_port) == (200, b'local'), root
                assert origin.requests == requests, 'nonmatching host reached origin'
                print('PASS: host proxy, rewrite=%s, inherited port, ACL fallback' % rewrite, flush=True)
            finally:
                if proc is not None:
                    stop_server(proc, executable)
    finally:
        origin.shutdown()
        origin.server_close()


if __name__ == '__main__':
    run(os.path.abspath(sys.argv[1]))
