#!/usr/bin/env python3
"""Linux regression: stopping kangle must stop its php-cgi processes.

Usage: python3 test/shutdown_child_regression.py KANGLE PHP_CGI
(vhs_sqlite.so must sit next to KANGLE, as it does in a build directory.)

Lays out sites like EasyPanel does: the <cmd> and the template that maps
.php to it live in ext/php.xml, and sites come either from a config file or
from vhs.db. Each case makes a request so a php-cgi is running, stops kangle
with SIGTERM and checks that the php-cgi exits too, once idle and once with
a request still running in it.

Clearing the config at shutdown removes each <cmd> and only released it.
Its processes were killed only if that release destroyed it. A request
still running when kangle stops holds the command, so it was not destroyed,
and the later kill of all processes no longer found it: every php-cgi of
that command was left running under init. Before the fix the "db site,
request running" case fails; a traced restart on an EasyPanel server showed
the same path.
"""
import argparse
import http.client
import os
from pathlib import Path
import shutil
import signal
import socket
import sqlite3
import subprocess
import tempfile
import threading
import time
from xml.sax.saxutils import quoteattr
from runtime_fixture import copy_server, module_name

PHP = '<?php echo getmypid();'
SLOW = '''<?php
file_put_contents(__DIR__ . '/slow.pid', getmypid());
sleep(5);
echo 'done';
'''


def alive(pid):
    try:
        with open('/proc/%d/stat' % pid) as f:
            # A zombie is already dead; it only waits to be reaped.
            return f.read().rsplit(')', 1)[1].split()[0] != 'Z'
    except OSError:
        return False


def run(binary, php, site, busy):
    with tempfile.TemporaryDirectory(prefix='kangle-shutdown-child-') as directory:
        root = Path(directory)
        for folder in ('bin', 'etc', 'ext', 'var', 'tmp', 'www'):
            (root / folder).mkdir()
        executable = copy_server(binary, str(root / 'bin'))
        driver = module_name('vhs_sqlite')
        shutil.copy2(str(Path(binary).with_name(driver)), str(root / 'bin' / driver))
        with socket.socket() as sock:
            sock.bind(('127.0.0.1', 0))
            port = sock.getsockname()[1]
        (root / 'www/ready').write_text('ready')
        (root / 'www/pid.php').write_text(PHP)
        (root / 'www/slow.php').write_text(SLOW)
        command = quoteattr('"' + php + '" -n -d cgi.force_redirect=0')
        (root / 'etc/config.xml').write_text('''<config>
<worker_thread>2</worker_thread><unix_socket>1</unix_socket>
<timeout rw='10' connect='5'/><listen ip='127.0.0.1' port='{port}' type='http'/>
<cache default='0' disk='0'/><request action='vhs'/><response action='allow'/>
<vhs><mime_type ext='*' type='application/octet-stream'/></vhs>
<vh_database driver='bin/{driver}' dbname='etc/vhs.db'/>
</config>'''.format(port=port, driver=driver))
        (root / 'ext/php.xml').write_text('''<!--#start 61--><config>
<cmd name='php' proto='fastcgi' file={command} type='mp' idle_time='600'>
<env PHP_FCGI_MAX_REQUESTS='0'/></cmd>
<vhs><vh_templete name='php' inherit='on'>
<map file_ext='php' extend='cmd:php' allow_method='*'/></vh_templete></vhs>
</config>'''.format(command=command))
        (root / 'ext/site.xml').write_text('''<!--#start 62--><config>
<vh name='file' templete='php' doc_root='www' inherit='on'><host>file.test</host></vh>
</config>''')
        db = sqlite3.connect(str(root / 'etc/vhs.db'))
        db.executescript('''CREATE TABLE vhost(name, uid, gid, doc_root, templete, subtemplete, status, inherit);
CREATE TABLE vhost_info(vhost, type, name, value, id);
INSERT INTO vhost VALUES ('db', NULL, NULL, 'www', 'php', NULL, 0, 1);
INSERT INTO vhost_info VALUES ('db', 0, 'db.test', '', 1);''')
        db.commit()
        db.close()
        host = site + '.test'

        def request(path):
            cn = http.client.HTTPConnection('127.0.0.1', port, timeout=15)
            try:
                cn.request('GET', path, headers={'Host': host})
                response = cn.getresponse()
                data = response.read()
                assert response.status == 200, (response.status, data[:500])
                return data
            finally:
                cn.close()

        def request_quietly(path):
            try:
                request(path)
            except Exception:
                pass

        php_pid = None
        with open(str(root / 'console.log'), 'wb') as log:
            proc = subprocess.Popen([executable, '-n', '-g'], stdout=log, stderr=log)
            try:
                for _ in range(100):
                    try:
                        assert request('/ready') == b'ready'
                        break
                    except OSError:
                        if proc.poll() is not None:
                            raise RuntimeError('kangle exited during startup')
                        time.sleep(.05)
                else:
                    raise RuntimeError('kangle did not start')
                if busy:
                    # The running request holds the command while kangle stops.
                    threading.Thread(target=lambda: request_quietly('/slow.php'),
                                     daemon=True).start()
                    pid_file = root / 'www/slow.pid'
                    for _ in range(100):
                        if pid_file.exists() and pid_file.read_text():
                            break
                        time.sleep(.05)
                    php_pid = int(pid_file.read_text())
                else:
                    php_pid = int(request('/pid.php'))
                assert alive(php_pid), 'php-cgi is not running'
                proc.send_signal(signal.SIGTERM)
                code = proc.wait(timeout=30)
            finally:
                if proc.poll() is None:
                    proc.kill()
                    proc.wait()
        # A busy php-cgi finishes its 5 s request before it exits.
        deadline = time.time() + 15
        while alive(php_pid) and time.time() < deadline:
            time.sleep(.1)
        left = alive(php_pid)
        if left:
            os.kill(php_pid, signal.SIGKILL)
        ok = code == 0 and not left
        print('{} {} site, {}: kangle exit {}, php-cgi {}'.format(
            'PASS' if ok else 'FAIL', site, 'request running' if busy else 'idle', code,
            'still running after shutdown' if left else 'stopped'), flush=True)
        return ok


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('kangle')
    parser.add_argument('php_cgi')
    args = parser.parse_args()
    binary = str(Path(args.kangle).resolve())
    php = str(Path(args.php_cgi).resolve())
    passed = True
    for site in ('file', 'db'):
        for busy in (False, True):
            passed = run(binary, php, site, busy) and passed
    raise SystemExit(0 if passed else 1)


if __name__ == '__main__':
    main()
