#!/usr/bin/env python3
"""Hung mp command process regression (Linux, PHP-CGI).

Usage: python3 test/cmd_hung_process_regression.py KANGLE PHP_CGI
Uses isolated loopback instances; does not change the live service or php.ini.

An mp-mode php-cgi serves one request at a time. When a script blocks forever
(for example curl without a timeout), Kangle times out reading the response
header and returns 504. The hung process must then be killed: if it goes back
to the head of the free list, every following request is sent to it and also
ends in 504 until Kangle is restarted.
"""
import argparse
import http.client
import json
from pathlib import Path
import socket
import subprocess
import tempfile
import time
from xml.sax.saxutils import quoteattr
from runtime_fixture import copy_server, stop_server


PROBE = '''<?php
header('Content-Type: application/json');
echo json_encode(array('pid'=>getmypid()));
'''

# sleep() blocks without using CPU time, like a socket read with no timeout,
# so max_execution_time never fires.
HANG = '''<?php
file_put_contents(__DIR__ . '/hang.pid', (string)getmypid());
sleep(3600);
'''

RW_TIMEOUT = 2


def alive(pid):
    try:
        stat = Path('/proc/%d/stat' % pid).read_text()
    except FileNotFoundError:
        return False
    # The state field follows the parenthesised command name.
    return stat.rsplit(')', 1)[1].split()[0] != 'Z'


def run(binary, php, unix):
    errors = []
    with tempfile.TemporaryDirectory(prefix='kangle-cmd-hung-') as directory:
        root = Path(directory)
        for folder in ('bin', 'etc', 'ext', 'var', 'tmp', 'www'):
            (root / folder).mkdir()
        executable = copy_server(binary, str(root / 'bin'))
        with socket.socket() as sock:
            sock.bind(('127.0.0.1', 0))
            port = sock.getsockname()[1]
        (root / 'www/ready').write_text('ready')
        (root / 'www/probe.php').write_text(PROBE)
        (root / 'www/hang.php').write_text(HANG)
        command = quoteattr('"' + php + '" -n -d cgi.force_redirect=0')
        (root / 'etc/config.xml').write_text('''<config>
<worker_thread>2</worker_thread><unix_socket>{unix}</unix_socket>
<timeout rw='{rw}' connect='5'/><listen ip='127.0.0.1' port='{port}' type='http'/>
<cache default='0' disk='0'/><request action='allow'/><response action='allow'/>
<vhs><mime_type ext='*' type='application/octet-stream'/></vhs>
<cmd name='php' proto='fastcgi' file={command} type='mp' idle_time='600' life_time='60'>
<env PHP_FCGI_MAX_REQUESTS='0'/></cmd>
<vh name='probe' doc_root='www' inherit='on'><host>*</host>
<map file_ext='php' extend='cmd:php' allow_method='*'/></vh></config>'''.format(
            unix=unix, rw=RW_TIMEOUT, port=port, command=command))

        def request(path):
            # With rw=2 the 504 arrives after about 10 s; keep the client
            # timeout well above that.
            cn = http.client.HTTPConnection('127.0.0.1', port, timeout=30)
            try:
                cn.request('GET', path)
                response = cn.getresponse()
                return response.status, response.read()
            finally:
                cn.close()

        hung_pid = None
        with open(str(root / 'console.log'), 'wb') as log:
            proc = subprocess.Popen([executable, '-n', '-g'], stdout=log, stderr=log)
            try:
                for _ in range(100):
                    try:
                        if request('/ready') == (200, b'ready'):
                            break
                    except OSError:
                        pass
                    if proc.poll() is not None:
                        raise RuntimeError('Kangle exited during startup')
                    time.sleep(.05)
                else:
                    raise RuntimeError('Kangle did not start')

                status, body = request('/probe.php')
                assert status == 200, ('warm-up', status, body[:300])
                warm_pid = json.loads(body.decode())['pid']

                started = time.time()
                status, body = request('/hang.php')
                elapsed = time.time() - started
                if status != 504:
                    errors.append('hung script returned %d, expected 504' % status)
                hung_pid = int((root / 'www/hang.pid').read_text())
                if hung_pid != warm_pid:
                    errors.append('hung request did not reuse the idle process')

                deadline = time.time() + 3
                while alive(hung_pid) and time.time() < deadline:
                    time.sleep(.05)
                if alive(hung_pid):
                    errors.append('hung php-cgi %d still running after 504' % hung_pid)

                for i in range(3):
                    status, body = request('/probe.php')
                    if status != 200:
                        errors.append('request %d after the hang returned %d' % (i + 1, status))
                        continue
                    if json.loads(body.decode())['pid'] == hung_pid:
                        errors.append('request %d after the hang went to the hung process' % (i + 1))
                print('{} unix={}: {}'.format(
                    'FAIL' if errors else 'PASS', unix,
                    '; '.join(errors) if errors else
                    '504 after %.1fs, hung process killed, next 3 requests 200' % elapsed),
                    flush=True)
            finally:
                stop_server(proc, executable)
                if hung_pid and alive(hung_pid):
                    subprocess.run(['kill', '-9', str(hung_pid)])
    return not errors


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('kangle')
    parser.add_argument('php_cgi')
    args = parser.parse_args()
    if not Path('/proc/self/stat').is_file():
        parser.error('this regression requires Linux /proc')
    binary = str(Path(args.kangle).resolve())
    php = str(Path(args.php_cgi).resolve())
    passed = True
    for unix in (1, 0):
        passed = run(binary, php, unix) and passed
    raise SystemExit(0 if passed else 1)


if __name__ == '__main__':
    main()
