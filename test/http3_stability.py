#!/usr/bin/env python3
"""Isolated H2/H3 benchmarks and sustained load; installed configuration is untouched.

Usage: python3 test/http3_stability.py /path/to/kangle --soak-seconds 600
Requires Go 1.23. Results include integrity checks, process resources and CPU time.
"""
import argparse
import hashlib
import heapq
import json
import os
import random
import selectors
import shutil
import signal
import socket
import socketserver
import subprocess
import tempfile
import threading
import time

from runtime_fixture import copy_runtime


SIZES = {'1k': 1024, '64k': 65536, '1m': 1048576, '16m': 16*1048576}
DATA = bytes(range(256))*4096


class Origin(socketserver.StreamRequestHandler):
    def handle(self):
        try:
            self.serve_requests()
        except (OSError, ValueError):
            # Idle keep-alive and canceled downloads are expected in this fixture.
            return

    def serve_requests(self):
        self.request.settimeout(15)
        while True:
            line = self.rfile.readline()
            if not line:
                return
            fields = line.split()
            if len(fields) != 3:
                return
            headers = {}
            while True:
                h = self.rfile.readline()
                if not h:
                    return
                if h == b'\r\n':
                    break
                k, v = h.split(b':', 1)
                headers[k.lower()] = v.strip()
            if headers.get(b'transfer-encoding') == b'chunked':
                while True:
                    n = int(self.rfile.readline().split(b';')[0], 16)
                    if n == 0:
                        while True:
                            trailer = self.rfile.readline()
                            if not trailer:
                                return
                            if trailer == b'\r\n':
                                break
                        break
                    self.rfile.read(n+2)
            else:
                self.rfile.read(int(headers.get(b'content-length', b'0')))
            name = fields[1].split(b'/')[-1].split(b'.')[0].decode()
            size = SIZES.get(name, 1024)
            try:
                self.request.sendall(b'HTTP/1.1 200 OK\r\nCache-Control: no-store\r\nContent-Length: '+str(size).encode()+b'\r\n\r\n')
                for pos in range(0, size, len(DATA)):
                    self.request.sendall(DATA[:min(len(DATA), size-pos)])
            except OSError:
                return


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


class ImpairedUDP:
    """Per-client UDP forwarding, confined to loopback; no system qdisc changes.

    Each direction has 10 +/- 3 ms delay and independent 1% packet loss. Jitter
    also permits reordering. Fixed seed makes the packet decisions reproducible.
    """
    def __init__(self, target):
        self.target = target
        self.front = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.front.bind(('127.0.0.1', 0))
        self.port = self.front.getsockname()[1]
        self.clients = {}
        self.pending = []
        self.sequence = self.dropped = self.forwarded = 0
        self.random = random.Random(20261008)
        self.stop = threading.Event()
        self.selector = selectors.DefaultSelector()
        self.selector.register(self.front, selectors.EVENT_READ, None)
        self.thread = threading.Thread(target=self.run, daemon=True)
        self.thread.start()

    def schedule(self, sock, target, data):
        if self.random.random() < .01:
            self.dropped += 1
            return
        self.sequence += 1
        heapq.heappush(self.pending, (time.monotonic()+self.random.uniform(.007, .013),
            self.sequence, sock, target, data))

    def run(self):
        while not self.stop.is_set():
            now = time.monotonic()
            while self.pending and self.pending[0][0] <= now:
                _, _, sock, target, data = heapq.heappop(self.pending)
                try:
                    sock.sendto(data, target)
                    self.forwarded += 1
                except OSError:
                    pass
            timeout = min(.01, max(0, self.pending[0][0]-now)) if self.pending else .01
            for key, _ in self.selector.select(timeout):
                data, peer = key.fileobj.recvfrom(65535)
                if key.fileobj is self.front:
                    if peer not in self.clients:
                        upstream = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                        upstream.bind(('127.0.0.1', 0))
                        self.clients[peer] = upstream
                        self.selector.register(upstream, selectors.EVENT_READ, peer)
                    self.schedule(self.clients[peer], self.target, data)
                else:
                    self.schedule(self.front, key.data, data)

    def close(self):
        self.stop.set()
        self.thread.join(timeout=2)
        self.selector.close()
        self.front.close()
        for sock in self.clients.values():
            sock.close()


def resources(pid):
    status = {}
    with open('/proc/%d/status' % pid) as f:
        for line in f:
            if ':' in line:
                k, v = line.split(':', 1)
                if k in ('VmRSS', 'VmSize', 'Threads'):
                    status[k] = int(v.strip().split()[0])
    with open('/proc/%d/stat' % pid) as f:
        fields = f.read().rsplit(')', 1)[1].split()
    status['cpu_seconds'] = (int(fields[11])+int(fields[12]))/os.sysconf('SC_CLK_TCK')
    status['fds'] = len(os.listdir('/proc/%d/fd' % pid))
    return status


def run(args):
    root = tempfile.mkdtemp(prefix='kangle-h3-load-')
    server, proc, relay = None, None, None
    samples, results = [], []
    done = threading.Event()
    try:
        for name in ('etc', 'ext', 'bin', 'var', 'tmp', 'log', 'www'):
            os.makedirs(root+'/'+name)
        shutil.copy2(args.binary, root+'/bin/kangle')
        copy_runtime(args.binary, root+'/bin')
        hashes = {}
        for name, size in SIZES.items():
            h = hashlib.sha256()
            with open(root+'/www/'+name+'.bin', 'wb') as f:
                for pos in range(0, size, len(DATA)):
                    block = DATA[:min(len(DATA), size-pos)]
                    f.write(block)
                    h.update(block)
            hashes[name] = h.hexdigest()
        server = Server(('127.0.0.1', 0), Origin)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        subprocess.check_call(['openssl', 'req', '-x509', '-nodes', '-newkey', 'rsa:2048',
            '-keyout', root+'/etc/key.pem', '-out', root+'/etc/cert.pem', '-days', '2', '-subj', '/CN=localhost'],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        s = socket.socket()
        s.bind(('127.0.0.1', 0))
        port = s.getsockname()[1]
        s.close()
        cfg = "<config><worker_thread>2</worker_thread><timeout rw='5' connect='3'/>"
        cfg += "<listen ip='127.0.0.1' port='%d' type='https' certificate='etc/cert.pem' certificate_key='etc/key.pem' http2='1' http3='1'/>" % port
        cfg += "<cache default='0' memory='16M' disk='0'/><request action='vhs'/><response action='allow'/>"
        cfg += "<vhs><mime_type ext='*' type='application/octet-stream'/></vhs>"
        cfg += "<server name='origin' host='127.0.0.1' port='%d' proto='http' life_time='30'/>" % server.server_address[1]
        cfg += "<vh name='load' doc_root='www' inherit='on'><host>127.0.0.1</host><host>localhost</host><map path='/proxy/' extend='server:origin' confirm_file='0' allow_method='*'/></vh></config>"
        with open(root+'/etc/config.xml', 'w') as f:
            f.write(cfg)
        console = open(root+'/console.log', 'wb')
        allowed = sorted(os.sched_getaffinity(0))
        server_cpus = set(allowed[:2]) if len(allowed) >= 4 else set(allowed)
        client_cpus = set(allowed[2:4]) if len(allowed) >= 4 else set(allowed)
        server_env = os.environ.copy()
        if os.path.isfile(root+'/bin/libstdc++.so.6'):
            # Match the deployed private runtime, even when the harness itself
            # runs inside the compiler's environment with newer system libraries.
            server_env['LD_LIBRARY_PATH'] = root+'/bin'
        proc = subprocess.Popen([root+'/bin/kangle', '-n', '-g'], stdout=console, stderr=console,
            env=server_env, preexec_fn=lambda: os.sched_setaffinity(0, server_cpus))
        for _ in range(100):
            try:
                socket.create_connection(('127.0.0.1', port), timeout=.1).close()
                break
            except OSError:
                assert proc.poll() is None, 'load server exited during startup'
                time.sleep(.05)
        framework = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'test_framework')
        probe = root+'/load-probe'
        subprocess.check_call(['go', 'build', '-o', probe, './http3_load'], cwd=framework)
        def monitor():
            while not done.wait(2):
                try:
                    item = resources(proc.pid)
                    item['seconds'] = time.monotonic()-start
                    samples.append(item)
                except (OSError, ValueError):
                    return
        start = time.monotonic()
        threading.Thread(target=monitor, daemon=True).start()
        print('Load prefix: '+root, flush=True)
        print('Server CPUs=%s, client CPUs=%s' % (sorted(server_cpus), sorted(client_cpus)), flush=True)
        def load(protocol, mode, size, concurrency, connections, seconds, proxy=False, impaired=False):
            assert proc.poll() is None, 'load server exited'
            command = [probe, '-url', 'https://127.0.0.1:%d/%s%s.bin' % (relay.port if impaired else port, 'proxy/' if proxy else '', size),
                '-protocol', protocol, '-mode', mode, '-seconds', str(seconds), '-concurrency', str(concurrency),
                '-connections', str(connections), '-size', str(SIZES[size]), '-sha256', hashes[size]]
            before = resources(proc.pid)
            result = subprocess.run(command, stdout=subprocess.PIPE, timeout=seconds+40,
                preexec_fn=lambda: os.sched_setaffinity(0, client_cpus))
            value = json.loads(result.stdout)
            value.update(size=size, proxy=proxy, impaired=impaired, server_cpu_seconds=resources(proc.pid)['cpu_seconds']-before['cpu_seconds'])
            results.append(value)
            with open(root+'/results.json', 'w') as f:
                json.dump(results, f, indent=2)
            with open(root+'/resources.json', 'w') as f:
                json.dump(samples, f, indent=2)
            print(json.dumps(value), flush=True)
            if mode != 'bench':
                assert result.returncode == 0, 'load errors: '+str(value)
            assert proc.poll() is None, 'load server exited after requests'
        if args.h2_large_only:
            for _ in range(3):
                load('h2', 'bench', '1m', 64, 1, args.bench_seconds)
        elif args.proxy_only:
            for _ in range(3):
                for protocol in ('h2', 'h3'):
                    load(protocol, 'bench', '64k', 32, 4, args.bench_seconds, True)
        elif not args.skip_benchmark:
            for size in ('1k', '64k', '1m'):
                for concurrency in (1, 16, 64, 256):
                    for protocol in ('h2', 'h3'):
                        load(protocol, 'bench', size, concurrency, 1, args.bench_seconds)
            for protocol in ('h2', 'h3'):
                load(protocol, 'bench', '1k', 128, 8, args.bench_seconds)
                load(protocol, 'bench', '64k', 32, 4, args.bench_seconds, True)
        # Small checks after stress assert existing listeners and streams still work.
        if args.small_seconds:
            for concurrency in (16, 64):
                load('h3', 'bench', '1k', concurrency, 1, args.small_seconds)
        if args.soak_seconds:
            load('h3', 'mixed', '1m', 128, 4, args.soak_seconds)
            load('h3', 'churn', '1k', 16, 1, 30)
            load('h3', 'blocked', '1m', 16, 4, 30)
            # One stalled stream per connection avoids other streams consuming
            # its connection window or timing out before response headers arrive.
            load('h3', 'timeout', '16m', 4, 4, 14)
            load('h3', 'bench', '1k', 16, 1, 5)
        if args.network_seconds:
            relay = ImpairedUDP(('127.0.0.1', port))
            load('h3', 'bench', '64k', 16, 4, args.network_seconds, impaired=True)
            load('h3', 'mixed', '1m', 16, 4, args.network_seconds, impaired=True)
            print('UDP impairment: forwarded=%d dropped=%d' % (relay.forwarded, relay.dropped), flush=True)
            load('h3', 'bench', '1k', 16, 1, 5)
        time.sleep(8)
        final = resources(proc.pid)
        print('After idle: '+json.dumps(final), flush=True)
        assert final['fds'] < 100, 'request or connection descriptors did not drain'
        assert all(r['errors'] == 0 for r in results), 'load cases had errors; see results.json'
        print('Load/stability passed: '+root, flush=True)
    finally:
        done.set()
        with open(root+'/resources.json', 'w') as f:
            json.dump(samples, f, indent=2)
        if proc and proc.poll() is None:
            proc.send_signal(signal.SIGTERM)
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
        if relay:
            relay.close()
        if server:
            server.shutdown()
        print('Artifacts: '+root, flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=os.path.abspath)
    parser.add_argument('--soak-seconds', type=int, default=600)
    parser.add_argument('--bench-seconds', type=int, default=5)
    parser.add_argument('--skip-benchmark', action='store_true')
    parser.add_argument('--proxy-only', action='store_true', help='repeat the HTTP/1 upstream comparison three times')
    parser.add_argument('--h2-large-only', action='store_true', help='repeat the large-response H2 control three times')
    parser.add_argument('--network-seconds', type=int, default=30)
    parser.add_argument('--small-seconds', type=int, default=0, help='sustained high-frequency small response checks')
    run(parser.parse_args())
