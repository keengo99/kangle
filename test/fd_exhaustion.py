#!/usr/bin/env python3
"""Linux regression: exhaust an isolated worker's fds, then verify accept recovery.

Usage: python3 test/fd_exhaustion.py /path/to/build/kangle
"""
import os
import pathlib
import resource
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time


def status(port):
    with socket.create_connection(("127.0.0.1", port), timeout=2) as conn:
        conn.sendall(b"GET /kangle.status HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
        response = b""
        while True:
            data = conn.recv(4096)
            if not data:
                break
            response += data
    return b"200 OK" in response and b"\r\n\r\nOK\n" in response


def limit_fds():
    resource.setrlimit(resource.RLIMIT_NOFILE, (128, 128))


def main():
    executable = pathlib.Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="kangle-fd-regression-") as temp:
        base = pathlib.Path(temp)
        for name in ("bin", "etc", "ext", "var", "www"):
            (base / name).mkdir()
        shutil.copy2(str(executable), str(base / "bin/kangle"))
        with socket.socket() as reservation:
            reservation.bind(("127.0.0.1", 0))
            port = reservation.getsockname()[1]
        (base / "etc/config.xml").write_text(
            '<config><worker_thread>1</worker_thread><listen ip="127.0.0.1" port="%d" type="http"/>'
            '<timeout rw="30" connect="5"/><request action="allow"/></config>' % port)
        sockets = []
        with (base / "console.log").open("wb") as console:
            process = subprocess.Popen([str(base / "bin/kangle"), "-n", "-g"],
                                       stdout=console, stderr=console, preexec_fn=limit_fds)
            try:
                ready = False
                for _ in range(50):
                    if process.poll() is not None:
                        break
                    try:
                        ready = status(port)
                    except OSError:
                        pass
                    if ready:
                        break
                    time.sleep(0.1)
                assert ready, "worker did not start"
                for round_number in range(2):
                    for _ in range(180):
                        sockets.append(socket.create_connection(("127.0.0.1", port), timeout=2))
                    time.sleep(0.3)
                    logs = b"".join(path.read_bytes() for path in base.rglob("*.log"))
                    assert b"accept failed errno=24" in logs, "EMFILE path was not exercised"
                    assert process.poll() is None, "worker died under fd exhaustion"
                    if round_number == 1:
                        # Closing the listener with an accept-retry timer pending must be safe too.
                        os.kill(process.pid, signal.SIGTERM)
                        time.sleep(0.2)
                    for conn in sockets:
                        conn.close()
                    sockets.clear()
                    if round_number == 0:
                        time.sleep(0.3)
                        for _ in range(3):
                            assert status(port), "listener did not recover after fd exhaustion"
                    else:
                        assert process.wait(timeout=8) == 0, "worker failed to stop cleanly"
                print("fd exhaustion: EMFILE exercised; 3 recovery requests passed; pending-retry shutdown passed")
            finally:
                for conn in sockets:
                    conn.close()
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=8)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()


if __name__ == "__main__":
    main()
