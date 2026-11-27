#!/usr/bin/env python3
"""Check template map inheritance on file- and database-backed vhosts."""

import argparse
import base64
from contextlib import closing
import http.client
import http.server
from pathlib import Path
import shutil
import socket
import sqlite3
import subprocess
import tempfile
import threading
import time

from runtime_fixture import copy_server, module_name, stop_server


class Origin(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        payload = b'upstream'
        self.send_response(200)
        self.send_header('Content-Length', str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def log_message(self, *args):
        pass


def request(port, path, host, auth=None):
    headers = {'Host': host}
    if auth:
        headers['Authorization'] = 'Basic ' + base64.b64encode(auth).decode('ascii')
    with closing(http.client.HTTPConnection('127.0.0.1', port, timeout=5)) as connection:
        connection.request('GET', path, headers=headers)
        response = connection.getresponse()
        return response.status, response.read()


def run(binary):
    with http.server.ThreadingHTTPServer(('127.0.0.1', 0), Origin) as origin, tempfile.TemporaryDirectory() as directory:
        threading.Thread(target=origin.serve_forever, daemon=True).start()
        try:
            check_sites(binary, Path(directory), origin.server_port)
        finally:
            origin.shutdown()


def check_sites(binary, root, origin_port):
    for name in ('bin', 'etc', 'ext', 'webadmin', 'var', 'tmp', 'www/routed', 'www/override'):
        (root / name).mkdir(parents=True, exist_ok=True)
    exe = copy_server(str(binary), str(root / 'bin'))
    driver = module_name('vhs_sqlite')
    shutil.copy2(binary.with_name(driver), root / 'bin' / driver)
    (root / 'webadmin/core.whm').write_text('<whm version="1.0"><extend name="core" type="core"/><call name="reload" extend="core"/></whm>', encoding='utf-8')
    for name in ('routed/page.txt', 'override/page.txt', 'page.aspx'):
        (root / 'www' / name).write_bytes(b'static')

    with closing(sqlite3.connect(root / 'etc/vhs.db')) as db:
        db.executescript('''CREATE TABLE vhost(name, uid, gid, doc_root, templete, subtemplete, status, inherit);
CREATE TABLE vhost_info(vhost, type, name, value, id);
INSERT INTO vhost VALUES ('dbsite', NULL, NULL, 'www', 'child', NULL, 0, 1);
INSERT INTO vhost_info VALUES ('dbsite', 0, 'db.test', '', 1);
INSERT INTO vhost_info VALUES ('dbsite', 3, '0,/override/', '0,default,*', 2);''')

    with socket.socket() as http_socket, socket.socket() as admin_socket:
        http_socket.bind(('127.0.0.1', 0))
        admin_socket.bind(('127.0.0.1', 0))
        port, admin = http_socket.getsockname()[1], admin_socket.getsockname()[1]
    template = root / 'ext/template.xml'
    template.write_text(f'''<!--#start 10--><config>
<server name="origin" host="127.0.0.1" port="{origin_port}" proto="http"/>
<vhs>
<vh_templete name="parent">
 <map path="/routed/" extend="server:origin" allow_method="*"/>
 <map_path path="/override/" extend="server:origin" allow_method="*"/>
</vh_templete>
<vh_templete name="child" templete="parent">
 <map_file ext="aspx" extend="server:origin" allow_method="*"/>
</vh_templete>
</vhs></config>''', encoding='utf-8')
    (root / 'etc/config.xml').write_text(f'''<config><worker_thread>2</worker_thread>
<listen ip="127.0.0.1" port="{port}" type="http"/>
<listen ip="127.0.0.1" port="{admin}" type="manage"/>
<admin user="probe" password="probe" admin_ips="127.0.0.1"/>
<api name="whm" file="buildin:whm" type="mt"/>
<cache default="0" memory="0" disk="0"/><request action="vhs"/><response action="allow"/>
<vhs><mime_type ext="txt" type="text/plain"/></vhs>
<vh_database driver="bin/{driver}" dbname="etc/vhs.db"/>
</config>''', encoding='utf-8')
    (root / 'ext/site.xml').write_text('''<!--#start 20--><config>
<vh name="file" templete="child" doc_root="www" inherit="1"><host>file.test</host>
 <map path="/override/" extend="default" allow_method="*"/>
</vh></config>''', encoding='utf-8')

    log = root / 'console.log'
    with log.open('wb') as output:
        process = subprocess.Popen([exe, '-n', '-g'], stdout=output, stderr=output)
        try:
            for _ in range(100):
                if process.poll() is not None:
                    raise RuntimeError('Kangle exited before startup')
                try:
                    with socket.create_connection(('127.0.0.1', port), timeout=.1):
                        break
                except OSError:
                    time.sleep(.1)
            else:
                raise RuntimeError('Kangle startup timed out')
            for host in ('file.test', 'db.test'):
                for path, expected in (('/routed/page.txt', b'upstream'), ('/page.aspx', b'upstream'), ('/override/page.txt', b'static')):
                    result = request(port, path, host)
                    assert result == (200, expected), (host, path, result)
                print(f'PASS {host}: parent path, child extension and site override')
            xml = template.read_text(encoding='utf-8')
            template.write_text(xml.replace('path="/routed/"', 'path="/moved/"'), encoding='utf-8')
            result = request(admin, '/core.whm?whm_call=reload', '127.0.0.1', b'probe:probe')
            assert result[0] == 200 and b'status="200"' in result[1], result
            assert request(port, '/routed/page.txt', 'db.test') == (200, b'static')
            assert request(port, '/moved/page.txt', 'db.test') == (200, b'upstream')
            print('PASS db.test: template reload removes the old path mapping')
        except Exception:
            print(log.read_text(encoding='utf-8', errors='replace')[-6000:])
            raise
        finally:
            stop_server(process, exe)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    run(parser.parse_args().binary.resolve())
