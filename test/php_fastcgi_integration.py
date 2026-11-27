#!/usr/bin/env python3
"""Real PHP FastCGI regression; optional verified native Go load test.

python test/php_fastcgi_integration.py KANGLE PHP_CGI [--benchmark BENCH_EXE]
Uses an isolated temporary installation and loopback ports, never changes php.ini.
Requires h2>=3.2 for the HTTP/2 request-body regression.
"""
import argparse
import concurrent.futures
import hashlib
import http.client
import json
import os
from pathlib import Path
import shutil
import socket
import ssl
import subprocess
import tempfile
import time
from xml.sax.saxutils import escape
from runtime_fixture import copy_server, stop_server
import h2.config
import h2.connection
import h2.events


def free_port():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


PHP = r'''<?php
$mode = $_GET['mode'] ?? 'info';
if ($mode === 'bench') { header('Content-Type: application/octet-stream'); readfile(__DIR__.'/static.bin'); exit; }
if ($mode === 'echo') { header('Content-Type: application/octet-stream'); echo file_get_contents('php://input'); exit; }
if ($mode === 'empty') { http_response_code((int)($_GET['code'] ?? 204)); exit; }
if ($mode === 'exit') { header('Content-Type: text/plain'); echo 'bye'; flush(); exit; }
header('Content-Type: application/json');
$input = file_get_contents('php://input');
$files = [];
foreach ($_FILES as $key => $file) {
    $files[$key] = ['error'=>$file['error'], 'size'=>$file['size'], 'hash'=>hash_file('sha256', $file['tmp_name'])];
}
echo json_encode(['version'=>PHP_VERSION, 'sapi'=>PHP_SAPI, 'pid'=>getmypid(),
    'method'=>$_SERVER['REQUEST_METHOD'], 'path_info'=>$_SERVER['PATH_INFO'] ?? '',
    'script'=>$_SERVER['SCRIPT_FILENAME'], 'query'=>$_GET, 'post'=>$_POST, 'files'=>$files,
    'size'=>strlen($input), 'hash'=>hash('sha256', $input)], JSON_UNESCAPED_UNICODE);
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('kangle')
    parser.add_argument('php_cgi')
    parser.add_argument('--benchmark')
    parser.add_argument('--benchmark-filter', action='append', help='only matching benchmark labels (repeatable)')
    parser.add_argument('--keep', action='store_true')
    args = parser.parse_args()
    binary, php = Path(args.kangle).resolve(), Path(args.php_cgi).resolve()
    root = Path(tempfile.mkdtemp(prefix='kangle-php-'))
    proc = None
    log = None
    passed = False
    checks = 0
    def check(condition, label):
        nonlocal checks
        assert condition, label
        checks += 1
        print('PASS:', label, flush=True)
    try:
        for folder in ('bin', 'etc', 'ext', 'var', 'tmp', 'www'):
            (root / folder).mkdir()
        executable = copy_server(str(binary), str(root / 'bin'))
        data = bytes(range(256)) * 64
        (root / 'www/static.bin').write_bytes(data)
        (root / 'www/test.php').write_text(PHP, encoding='utf-8')
        (root / 'payload.bin').write_bytes(data)
        (root / 'www/ready').write_text('ready', encoding='ascii')
        ports = [free_port() for _ in range(3)]
        http_port, tls_port, sp_port = ports
        openssl = os.environ.get('KANGLE_TEST_OPENSSL', 'openssl')
        subprocess.run([openssl, 'req', '-x509', '-newkey', 'rsa:2048', '-nodes',
                        '-keyout', str(root / 'etc/server.key'), '-out', str(root / 'etc/server.crt'),
                        '-days', '1', '-subj', '/CN=localhost'], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        command = escape('"' + php.as_posix() + '" -n -d cgi.force_redirect=0 -d upload_max_filesize=8M -d post_max_size=8M')
        # -n makes this independent of supplied PHP's production php.ini/extensions.
        cfg = f"""<config><worker_thread>4</worker_thread><timeout rw='30' connect='5'/>
<listen ip='127.0.0.1' port='{http_port}h' type='http'/>
<listen ip='127.0.0.1' port='{tls_port}' type='https' certificate='etc/server.crt' certificate_key='etc/server.key' alpn='3'/>
<cache default='0' disk='0'/><vhs><mime_type ext='*' type='application/octet-stream'/></vhs>
<request action='allow'/><response action='allow'/>
<cmd name='php_mp' proto='fastcgi' file='{command}' type='mp' idle_time='0' life_time='600'><env PHP_FCGI_MAX_REQUESTS='0'/></cmd>
<cmd name='php_sp' proto='fastcgi' file='{command} -b 127.0.0.1:{sp_port}' port='{sp_port}' type='sp' idle_time='0' life_time='0'><env PHP_FCGI_MAX_REQUESTS='0'/></cmd>
<vh name='php' doc_root='www' inherit='on'><host>*</host>
<map path='/mp' extend='cmd:php_mp' allow_method='*'/>
<map path='/sp' extend='cmd:php_sp' allow_method='*'/>
</vh></config>"""
        # PHP -b has a single accept loop: persistent connections can starve
        # other connections. Disable SP connection pooling, not process reuse.
        # Separate mappings point to real files.
        (root / 'www/mp').mkdir()
        (root / 'www/sp').mkdir()
        for mode in ('mp', 'sp'):
            (root / f'www/{mode}/test.php').write_text(PHP, encoding='utf-8')
            (root / f'www/{mode}/static.bin').write_bytes(data)
        (root / 'etc/config.xml').write_text(cfg, encoding='utf-8')
        log = open(root / 'console.log', 'wb')
        proc = subprocess.Popen([executable, '-n', '-g'], stdout=log, stderr=log)
        def request(path, method='GET', body=None, headers=None, tls=False, connection=None, chunked=False):
            own = connection is None
            if own:
                connection = (http.client.HTTPSConnection('127.0.0.1', tls_port, timeout=40,
                              context=ssl._create_unverified_context()) if tls else
                              http.client.HTTPConnection('127.0.0.1', http_port, timeout=40))
            connection.request(method, path, body=body, headers=headers or {}, encode_chunked=chunked)
            response = connection.getresponse()
            result = response.status, dict(response.getheaders()), response.read()
            if own:
                connection.close()
            return result
        def h2_post_without_length(path, payload):
            context = ssl._create_unverified_context()
            context.set_alpn_protocols(['h2'])
            with socket.create_connection(('127.0.0.1', tls_port), timeout=40) as raw:
                with context.wrap_socket(raw, server_hostname='localhost') as sock:
                    assert sock.selected_alpn_protocol() == 'h2'
                    cn = h2.connection.H2Connection(config=h2.config.H2Configuration(client_side=True, header_encoding='utf-8'))
                    cn.initiate_connection()
                    cn.send_headers(1, [(':method','POST'), (':scheme','https'), (':authority','localhost'),
                                        (':path',path), ('content-type','application/octet-stream')])
                    cn.send_data(1, payload, end_stream=True)
                    sock.sendall(cn.data_to_send())
                    status, body = None, bytearray()
                    while True:
                        packet = sock.recv(65536)
                        assert packet, 'HTTP/2 EOF before PHP response'
                        ended = False
                        for event in cn.receive_data(packet):
                            if isinstance(event, h2.events.ResponseReceived):
                                status = int(dict(event.headers)[':status'])
                            elif isinstance(event, h2.events.DataReceived):
                                body.extend(event.data)
                                cn.acknowledge_received_data(event.flow_controlled_length, event.stream_id)
                            elif isinstance(event, h2.events.StreamEnded):
                                ended = True
                            elif isinstance(event, (h2.events.StreamReset, h2.events.ConnectionTerminated)):
                                raise AssertionError(str(event))
                        pending = cn.data_to_send()
                        if pending:
                            sock.sendall(pending)
                        if ended:
                            return status, json.loads(body)
        for _ in range(100):
            try:
                status, _, body = request('/ready')
                if status == 200 and body == b'ready':
                    break
            except (OSError, http.client.HTTPException):
                time.sleep(.05)
        else:
            raise AssertionError('Kangle startup failed')
        for mode in ('mp', 'sp'):
            path = f'/{mode}/test.php'
            status, _, body = request(path + '?name=%E4%B8%AD%E6%96%87')
            info = json.loads(body)
            check(status == 200 and info['sapi'] == 'cgi-fcgi' and info['query']['name'] == '中文', mode + ' PHP/GET/UTF-8 query')
            print('PHP version:', info['version'], 'mode:', mode, flush=True)
            check(Path(info['script']).resolve() == (root / f'www/{mode}/test.php').resolve(), mode + ' SCRIPT_FILENAME')
            cn = http.client.HTTPConnection('127.0.0.1', http_port, timeout=40)
            try:
                for method, code in [('HEAD', 200), ('GET', 204), ('GET', 304)]:
                    suffix = '' if method == 'HEAD' else f'?mode=empty&code={code}'
                    status, _, body = request(path + suffix, method, connection=cn)
                    check(status == code and not body, mode + ' no body ' + str(code))
                    check(request(path + '?mode=bench', connection=cn)[2] == data, mode + ' reuse after no-body response')
            finally:
                cn.close()
            payload = data * 64
            status, _, body = request(path, 'POST', payload, {'Content-Type':'application/octet-stream', 'Expect':'100-continue'})
            info = json.loads(body)
            check(status == 200 and info['size'] == len(payload) and info['hash'] == hashlib.sha256(payload).hexdigest(), mode + ' 1MiB POST/Expect')
            # http.client sends Expect bodies immediately. Check a client that
            # actually waits for 100 Continue before transmitting its body too.
            with socket.create_connection(('127.0.0.1', http_port), timeout=40) as sock:
                sock.sendall((f'POST {path} HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/octet-stream\r\nContent-Length: {len(data)}\r\nExpect: 100-continue\r\n\r\n').encode())
                stream = sock.makefile('rb')
                try:
                    check(stream.readline().split()[1] == b'100', mode + ' receives 100 before body')
                    for _ in range(100):
                        line = stream.readline()
                        assert line, 'EOF in 100 Continue headers'
                        if line == b'\r\n':
                            break
                    else:
                        raise AssertionError('too many 100 Continue headers')
                    sock.sendall(data)
                    response = http.client.HTTPResponse(sock)
                    response.begin()
                    info = json.loads(response.read())
                    check(response.status == 200 and info['hash'] == hashlib.sha256(data).hexdigest(), mode + ' body after 100 Continue')
                finally:
                    stream.close()
            status, _, body = request(path, 'POST', iter([data[:13], data[13:]]),
                                      {'Content-Type':'application/octet-stream'}, chunked=True)
            info = json.loads(body)
            check(status == 200 and info['size'] == len(data) and info['hash'] == hashlib.sha256(data).hexdigest(), mode + ' chunked POST')
            status, info = h2_post_without_length(path, data)
            check(status == 200 and info['size'] == len(data) and info['hash'] == hashlib.sha256(data).hexdigest(), mode + ' HTTP/2 POST without Content-Length')
            status, _, body = request(path, 'POST', 'name=%E4%B8%AD%E6%96%87&answer=42', {'Content-Type':'application/x-www-form-urlencoded'})
            check(status == 200 and json.loads(body)['post'] == {'name':'中文','answer':'42'}, mode + ' form body')
            boundary = 'KanglePHPUpload'
            upload = (f'--{boundary}\r\nContent-Disposition: form-data; name="file"; filename="test.bin"\r\nContent-Type: application/octet-stream\r\n\r\n'.encode()
                      + data + f'\r\n--{boundary}--\r\n'.encode())
            status, _, body = request(path, 'POST', upload, {'Content-Type':'multipart/form-data; boundary='+boundary})
            file = json.loads(body)['files']['file']
            check(status == 200 and file == {'error':0, 'size':len(data), 'hash':hashlib.sha256(data).hexdigest()}, mode + ' multipart upload')
            check(request(path + '?mode=bench', tls=True)[2] == data, mode + ' HTTPS FastCGI')
            first_pid = json.loads(request(path)[2])['pid']
            time.sleep(1)
            check(json.loads(request(path)[2])['pid'] == first_pid, mode + ' PHP process remains resident')
            with concurrent.futures.ThreadPoolExecutor(max_workers=16) as executor:
                results = list(executor.map(lambda _: request(path + '?mode=echo', 'POST', data)[2], range(160)))
            check(all(result == data for result in results), mode + ' concurrent POST (160)')
        # Kill only a process identified by this isolated server's own PHP reply.
        pid = json.loads(request('/sp/test.php')[2])['pid']
        if os.name == 'nt':
            subprocess.run(['taskkill', '/PID', str(pid), '/F'], check=True, stdout=subprocess.DEVNULL)
        else:
            import signal
            os.kill(pid, signal.SIGKILL)
        recovered = False
        for _ in range(10):
            try:
                status, _, body = request('/sp/test.php')
                if status == 200 and json.loads(body)['pid'] != pid:
                    recovered = True
                    break
            except (ValueError, OSError, http.client.HTTPException):
                pass
            time.sleep(.2)
        check(recovered, 'SP PHP restarts after process termination')
        if args.benchmark:
            digest = hashlib.sha256(data).hexdigest()
            cases = [
                ('static HTTP/1', 0, 32, 1000, f'http://127.0.0.1:{http_port}/static.bin', []),
                ('static HTTP/2', 1, 32, 1000, f'https://127.0.0.1:{tls_port}/static.bin', []),
                ('static h2c', 3, 32, 1000, f'http://127.0.0.1:{http_port}/static.bin', []),
                ('PHP MP GET', 0, 32, 300, f'http://127.0.0.1:{http_port}/mp/test.php?mode=bench', []),
                ('PHP SP GET', 0, 32, 300, f'http://127.0.0.1:{http_port}/sp/test.php?mode=bench', []),
                ('PHP MP HTTP/2 POST', 1, 32, 300, f'https://127.0.0.1:{tls_port}/mp/test.php?mode=echo', ['-post-file', str(root / 'payload.bin')]),
                ('static HTTP/1 128 concurrent', 0, 128, 500, f'http://127.0.0.1:{http_port}/static.bin', []),
                ('static HTTP/2 128 concurrent', 1, 128, 500, f'https://127.0.0.1:{tls_port}/static.bin', []),
            ]
            report = []
            for label, protocol, concurrency, count, url, extra in cases:
                if args.benchmark_filter and not any(value in label for value in args.benchmark_filter):
                    continue
                print('BENCH START:', label, flush=True)
                output = subprocess.run([args.benchmark, '-k', '-a', str(protocol), '-c', str(concurrency),
                                         '-n', str(count), '-sha256', digest] + extra + [url],
                                        capture_output=True, text=True, timeout=180)
                print(output.stdout.strip(), output.stderr.strip(), flush=True)
                check(output.returncode == 0, label + ' load/status/body hash')
                result = json.loads(output.stdout)
                result.update(case=label, concurrency=concurrency)
                report.append(result)
                check(proc.poll() is None and request('/ready')[2] == b'ready', label + ' server remains healthy')
            (root / 'performance.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
        stop_server(proc, executable)
        check(proc.returncode == 0, 'graceful Kangle shutdown')
        passed = True
        print(f'{checks} checks passed; artifacts: {root}', flush=True)
    finally:
        if proc and proc.poll() is None:
            stop_server(proc, executable)
        if log:
            log.close()
        if passed and not args.keep:
            shutil.rmtree(root)
        elif not passed:
            print('FAIL artifacts:', root, flush=True)


if __name__ == '__main__':
    main()
