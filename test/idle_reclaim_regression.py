#!/usr/bin/env python3
"""Linux regression: idle keep-alive connections make room for new clients.

1. File descriptors run out (RLIMIT_NOFILE 128) while every connection
   only waits for its next request: a new client is still served.
2. <connect max='20'> is reached the same way: a new client is served
   instead of refused.
In both cases some of the idle connections are closed by the server.

Usage: python3 test/idle_reclaim_regression.py /path/to/build/kangle
"""
import pathlib
import resource
import shutil
import socket
import subprocess
import sys
import tempfile
import time

REQUEST = b"GET /kangle.status HTTP/1.1\r\nHost: localhost\r\n\r\n"


def read_response(conn):
    """Read one keep-alive response; None if nothing arrives in time."""
    data = b""
    while b"\r\n\r\n" not in data:
        chunk = conn.recv(4096)
        if not chunk:
            return None
        data += chunk
    head, body = data.split(b"\r\n\r\n", 1)
    length = 0
    for line in head.split(b"\r\n")[1:]:
        name, _, value = line.partition(b":")
        if name.strip().lower() == b"content-length":
            length = int(value.strip())
    while len(body) < length:
        chunk = conn.recv(4096)
        if not chunk:
            return None
        body += chunk
    return head.split(b"\r\n", 1)[0]


def keepalive_client(port, timeout):
    conn = socket.create_connection(("127.0.0.1", port), timeout=timeout)
    try:
        conn.sendall(REQUEST)
        status = read_response(conn)
    except OSError:
        status = None
    if status is None or b" 200 " not in status:
        conn.close()
        return None
    return conn


def closed_by_server(conn):
    conn.settimeout(0.2)
    try:
        return conn.recv(1) == b""
    except socket.timeout:
        return False
    except OSError:
        return True


def run_case(executable, name, config, fill, preexec=None):
    with tempfile.TemporaryDirectory(prefix="kangle-idle-reclaim-") as temp:
        base = pathlib.Path(temp)
        for d in ("bin", "etc", "ext", "var", "www"):
            (base / d).mkdir()
        shutil.copy2(str(executable), str(base / "bin/kangle"))
        with socket.socket() as reservation:
            reservation.bind(("127.0.0.1", 0))
            port = reservation.getsockname()[1]
        (base / "etc/config.xml").write_text(config % port)
        idle = []
        with (base / "console.log").open("wb") as console:
            process = subprocess.Popen([str(base / "bin/kangle"), "-n", "-g"],
                                       stdout=console, stderr=console, preexec_fn=preexec)
            try:
                for _ in range(50):
                    try:
                        conn = keepalive_client(port, 1)
                        if conn:
                            conn.close()
                            break
                    except OSError:
                        pass
                    time.sleep(0.1)
                else:
                    raise AssertionError(name + ": server did not start")
                time.sleep(0.3)
                # fill the server with keep-alive connections that wait for a next request
                for _ in range(fill):
                    try:
                        conn = keepalive_client(port, 0.5)
                    except OSError:
                        conn = None
                    if conn is None:
                        break
                    idle.append(conn)
                assert len(idle) >= 10, "%s: only %d keep-alive connections" % (name, len(idle))
                # timeout rw is 30s, so only reclaiming lets this through quickly
                start = time.time()
                served = None
                while served is None and time.time() - start < 5:
                    try:
                        served = keepalive_client(port, 2)
                    except OSError:
                        served = None
                    if served is None:
                        time.sleep(0.2)
                elapsed = time.time() - start
                assert served is not None, "%s: new client not served after %.1fs (%d idle)" % (name, elapsed, len(idle))
                served.close()
                reclaimed = sum(1 for c in idle if closed_by_server(c))
                assert reclaimed > 0, name + ": no idle connection was closed"
                assert process.poll() is None, name + ": server exited"
                print("PASS: %s: new client served in %.2fs; %d of %d idle connections closed"
                      % (name, elapsed, reclaimed, len(idle)))
            finally:
                for c in idle:
                    c.close()
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=8)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()


def main():
    executable = pathlib.Path(sys.argv[1]).resolve()
    base_config = ('<config><worker_thread>1</worker_thread><listen ip="127.0.0.1" port="%d" type="http"/>'
                   '<timeout rw="30" connect="5"/><request action="allow"/>{}</config>')
    run_case(executable, "out of file descriptors", base_config.format(""), 200,
             lambda: resource.setrlimit(resource.RLIMIT_NOFILE, (128, 128)))
    run_case(executable, "connect max", base_config.format('<connect max="20" max_per_ip="0"/>'), 20)


if __name__ == "__main__":
    main()
