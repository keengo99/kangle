#!/usr/bin/env python3
"""Linux PHP-CGI inherited-listener regression (PHP 5.6+).

Usage: python3 test/php_listener_regression.py KANGLE PHP_CGI
Uses isolated loopback instances; does not change the live service or php.ini.
Checks listener flags, process residency across connection closure/expiry,
and POST payload delivery/execution count with Unix and TCP listeners.
"""
import argparse
import http.client
import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import time
from xml.sax.saxutils import quoteattr
from runtime_fixture import copy_server, stop_server


PHP = '''<?php
$file = __DIR__ . '/counter';
$count = file_exists($file) ? (int)file_get_contents($file) : 0;
file_put_contents($file, (string)++$count);
header('Content-Type: application/json');
echo json_encode(array('pid'=>getmypid(), 'count'=>$count,
    'body'=>file_get_contents('php://input'), 'method'=>$_SERVER['REQUEST_METHOD']));
'''


def run(binary, php, unix, lifetime):
    errors = []
    with tempfile.TemporaryDirectory(prefix='kangle-php-listener-') as directory:
        root = Path(directory)
        for folder in ('bin', 'etc', 'ext', 'var', 'tmp', 'www'):
            (root / folder).mkdir()
        executable = copy_server(binary, str(root / 'bin'))
        with socket.socket() as sock:
            sock.bind(('127.0.0.1', 0))
            port = sock.getsockname()[1]
        (root / 'www/ready').write_text('ready')
        (root / 'www/probe.php').write_text(PHP)
        command = quoteattr('"' + php + '" -n -d cgi.force_redirect=0')
        (root / 'etc/config.xml').write_text('''<config>
<worker_thread>2</worker_thread><unix_socket>{unix}</unix_socket>
<timeout rw='10' connect='5'/><listen ip='127.0.0.1' port='{port}' type='http'/>
<cache default='0' disk='0'/><request action='allow'/><response action='allow'/>
<vhs><mime_type ext='*' type='application/octet-stream'/></vhs>
<cmd name='php' proto='fastcgi' file={command} type='mp' idle_time='600' life_time='{lifetime}'>
<env PHP_FCGI_MAX_REQUESTS='0'/></cmd>
<vh name='probe' doc_root='www' inherit='on'><host>*</host>
<map file_ext='php' extend='cmd:php' allow_method='*'/></vh></config>'''.format(
            unix=unix, port=port, command=command, lifetime=lifetime))

        def request(path, body=None):
            cn = http.client.HTTPConnection('127.0.0.1', port, timeout=15)
            try:
                cn.request('GET' if body is None else 'POST', path, body=body,
                           headers={'Content-Type': 'application/octet-stream'})
                response = cn.getresponse()
                data = response.read()
                assert response.status == 200, (response.status, data[:500])
                return data
            finally:
                cn.close()

        with open(str(root / 'console.log'), 'wb') as log:
            proc = subprocess.Popen([executable, '-n', '-g'], stdout=log, stderr=log)
            try:
                for _ in range(100):
                    try:
                        assert request('/ready') == b'ready'
                        break
                    except OSError:
                        if proc.poll() is not None:
                            raise RuntimeError('Kangle exited during startup')
                        time.sleep(.05)
                else:
                    raise RuntimeError('Kangle did not start')
                initial_pid = None
                for i in range(4):
                    # Expire the connection while keeping the PHP process alive.
                    if i:
                        time.sleep(2.2 if lifetime else .2)
                    payload = None if i % 2 == 0 else ('payload-%d-' % i + 'x' * 8192)
                    info = json.loads(request('/probe.php', payload).decode())
                    if initial_pid is None:
                        initial_pid = info['pid']
                    if info['pid'] != initial_pid:
                        errors.append('PHP PID changed after connection closure')
                    assert info['count'] == i + 1, 'request executed more than once'
                    assert info['body'] == (payload or ''), 'POST payload changed'
                    assert info['method'] == ('GET' if payload is None else 'POST')
                    fdinfo = Path('/proc/%d/fdinfo/0' % info['pid'])
                    try:
                        flags = next(line.split()[1] for line in fdinfo.read_text().splitlines()
                                     if line.startswith('flags:'))
                        if int(flags, 8) & os.O_NONBLOCK:
                            errors.append('inherited listener has O_NONBLOCK')
                    except FileNotFoundError:
                        errors.append('PHP exited after returning its response')
                print('{} unix={} life_time={} requests=4: {}'.format(
                    'FAIL' if errors else 'PASS', unix, lifetime,
                    '; '.join(sorted(set(errors))) if errors else
                    'blocking listener, same PHP PID, GET/POST exactly once'), flush=True)
            finally:
                stop_server(proc, executable)
    return not errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('kangle')
    parser.add_argument('php_cgi')
    args = parser.parse_args()
    if not Path('/proc/self/fdinfo').is_dir():
        parser.error('this regression requires Linux /proc fdinfo')
    binary = str(Path(args.kangle).resolve())
    php = str(Path(args.php_cgi).resolve())
    passed = True
    for unix in (1, 0):
        for lifetime in (0, 1):
            passed = run(binary, php, unix, lifetime) and passed
    raise SystemExit(0 if passed else 1)


if __name__ == '__main__':
    main()
