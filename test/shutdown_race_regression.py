#!/usr/bin/env python3
"""Linux regression: stopping kangle while requests are writing error logs.

Several clients keep requesting a missing file, so every response writes
an error log line, while kangle is stopped with SIGTERM. Shutdown must
end with exit code 0, not a crash: exit() used to run global destructors
(the error logger among them) while worker threads were still logging.

This is a smoke test. On a small test server the race window is too short
to hit reliably, so the old build usually passes too. On busy production
servers, where tearing down large caches makes exit() slow, it crashed in
KLogElement::open. To check the mechanism directly, run kangle under gdb
with a breakpoint on KLogElement::~KLogElement and stop it: the fixed build
never reaches the breakpoint.

Usage: python3 test/shutdown_race_regression.py /path/to/build/kangle [rounds]
"""
import pathlib
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import threading
import time

REQUEST = b"GET /no-such-file-%d HTTP/1.1\r\nHost: localhost\r\n\r\n"


def hammer(port, stop):
    n = 0
    while not stop.is_set():
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=1) as conn:
                while not stop.is_set():
                    conn.sendall(REQUEST % n)
                    n += 1
                    data = conn.recv(65536)
                    if not data:
                        break
        except OSError:
            time.sleep(0.01)


def one_round(executable, base, port):
    with (base / "console.log").open("ab") as console:
        process = subprocess.Popen([str(executable), "-n", "-g"], stdout=console, stderr=console)
    try:
        for _ in range(100):
            try:
                socket.create_connection(("127.0.0.1", port), timeout=0.1).close()
                break
            except OSError:
                if process.poll() is not None:
                    raise AssertionError("server exited at start: %s" % process.returncode)
                time.sleep(0.05)
        stop = threading.Event()
        clients = [threading.Thread(target=hammer, args=(port, stop), daemon=True) for _ in range(16)]
        for c in clients:
            c.start()
        time.sleep(0.4)
        process.send_signal(signal.SIGTERM)
        code = process.wait(timeout=30)
        stop.set()
        for c in clients:
            c.join(timeout=2)
        return code
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()


def main():
    executable = pathlib.Path(sys.argv[1]).resolve()
    rounds = int(sys.argv[2]) if len(sys.argv) > 2 else 30
    with tempfile.TemporaryDirectory(prefix="kangle-shutdown-race-") as temp:
        base = pathlib.Path(temp)
        for name in ("bin", "etc", "ext", "var", "www"):
            (base / name).mkdir()
        shutil.copy2(str(executable), str(base / "bin/kangle"))
        with socket.socket() as reservation:
            reservation.bind(("127.0.0.1", 0))
            port = reservation.getsockname()[1]
        (base / "etc/config.xml").write_text(
            '<config><worker_thread>4</worker_thread><listen ip="127.0.0.1" port="%d" type="http"/>'
            '<timeout rw="10" connect="5"/><request action="allow"/>'
            '<vh name="t" doc_root="www" inherit="on"><host>*</host></vh></config>' % port)
        codes = [one_round(base / "bin/kangle", base, port) for _ in range(rounds)]
        crashes = [c for c in codes if c != 0]
        assert not crashes, "%d of %d shutdowns failed, exit codes %s (negative = signal)" % (
            len(crashes), rounds, sorted(set(crashes)))
        print("PASS: %d shutdowns under error-log load, all exit code 0" % rounds)


if __name__ == "__main__":
    main()
