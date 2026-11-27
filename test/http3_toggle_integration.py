#!/usr/bin/env python3
"""Check HTTP/3 enable/disable on an already running HTTPS listener.

Usage: python3 test/http3_toggle_integration.py /absolute/path/to/kangle
Requires Go 1.23. Uses an isolated prefix and does not change installed config.
"""
import hashlib
import json
import os
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time
from runtime_fixture import copy_runtime


def run(binary):
    root = tempfile.mkdtemp(prefix='kangle-h3-toggle-')
    proc = None
    try:
        for name in ('etc', 'ext', 'bin', 'var', 'tmp', 'log', 'www'):
            os.makedirs(root+'/'+name)
        shutil.copy2(binary, root+'/bin/kangle')
        copy_runtime(binary, root+'/bin')
        payload = b'HTTP3 switch regression\n' * 128
        with open(root+'/www/payload.bin', 'wb') as f:
            f.write(payload)
        subprocess.check_call(['openssl', 'req', '-x509', '-nodes', '-newkey', 'rsa:2048',
            '-keyout', root+'/etc/key.pem', '-out', root+'/etc/cert.pem', '-days', '2',
            '-subj', '/CN=localhost'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        with socket.socket() as sock:
            sock.bind(('127.0.0.1', 0))
            port = sock.getsockname()[1]
        cfg = "<config><worker_thread>2</worker_thread>"
        cfg += "<listen ip='127.0.0.1' port='%d' type='https' certificate='etc/cert.pem' certificate_key='etc/key.pem' http2='1'/>" % port
        cfg += "<cache default='0' memory='0' disk='0'/><request action='vhs'/><response action='allow'/><vhs><mime_type ext='*' type='application/octet-stream'/></vhs>"
        cfg += "<vh name='toggle' doc_root='www' inherit='on' http2='1' http3='%d' certificate='-etc/cert.pem' certificate_key='-etc/key.pem'>"
        cfg += "<host>127.0.0.1</host><host>localhost</host><bind>127.0.0.1:%ds</bind></vh></config>" % port
        def configure(enabled):
            with open(root+'/etc/config.xml', 'w') as f:
                f.write(cfg % enabled)
        configure(0)
        with open(root+'/console.log', 'wb') as log:
            proc = subprocess.Popen([root+'/bin/kangle', '-n', '-g'], stdout=log, stderr=log)
        for _ in range(100):
            try:
                socket.create_connection(('127.0.0.1', port), timeout=.1).close()
                break
            except OSError:
                assert proc.poll() is None, 'server exited: '+root
                time.sleep(.05)
        framework = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'test_framework')
        probe = root+'/probe'
        subprocess.check_call(['go', 'build', '-o', probe, './http3_load'], cwd=framework)
        def check(protocol, enabled=True):
            output = subprocess.run([probe, '-url', 'https://127.0.0.1:%d/payload.bin' % port,
                '-protocol', protocol, '-seconds', '1', '-concurrency', '1', '-size', str(len(payload)),
                '-sha256', hashlib.sha256(payload).hexdigest()], stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False, timeout=25)
            if not enabled:
                assert output.returncode != 0 and b'timeout: no recent network activity' in output.stderr, output.stderr
                print('PASS: disabled HTTP3 rejects new connections', flush=True)
                return
            assert output.returncode == 0, output.stderr
            result = json.loads(output.stdout)
            assert result['success'] > 0 and result['errors'] == 0, result
            print('PASS: '+protocol+' requests with complete matching body', flush=True)
        def udp_closed():
            for _ in range(60):
                with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
                    try:
                        sock.bind(('127.0.0.1', port))
                        return
                    except OSError:
                        time.sleep(.05)
            raise AssertionError('UDP listener remains open after HTTP3 disabled')
        udp_closed()
        check('h2')
        for _ in range(2):
            configure(1)
            proc.send_signal(signal.SIGHUP)
            time.sleep(2)
            check('h3')
            check('h2')
            configure(0)
            proc.send_signal(signal.SIGHUP)
            time.sleep(2)
            check('h3', False)
            check('h2')
            assert proc.poll() is None, 'server exited while toggling HTTP3'
        print('HTTP3 hot toggle regression passed; artifacts: '+root, flush=True)
    finally:
        if proc and proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()


if __name__ == '__main__':
    run(os.path.abspath(sys.argv[1]))
