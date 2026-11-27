"""Platform helpers for isolated integration-test installations."""
import os
import shutil
import subprocess


def exe_name(name):
    return name + ('.exe' if os.name == 'nt' else '')


def module_name(name):
    return name + ('.dll' if os.name == 'nt' else '.so')


def copy_server(binary, bindir):
    target = os.path.join(bindir, exe_name('kangle'))
    shutil.copy2(binary, target)
    copy_runtime(binary, bindir)
    return target


def stop_server(proc, binary, timeout=10):
    if proc.poll() is None:
        if os.name == 'nt':
            # SIGTERM means TerminateProcess on Windows; use Kangle's signal
            # pipe instead so configuration and child-process cleanup run.
            try:
                subprocess.run([binary, '-q'], stdout=subprocess.DEVNULL,
                               stderr=subprocess.DEVNULL, timeout=timeout)
            except subprocess.TimeoutExpired:
                pass
        else:
            proc.terminate()
    try:
        proc.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()


def copy_runtime(binary, bindir):
    libraries = ('sqlite3.dll',) if os.name == 'nt' else ('libstdc++.so.6', 'libgcc_s.so.1')
    for name in libraries:
        source = os.path.join(os.path.dirname(binary), name)
        if os.path.isfile(source):
            shutil.copy2(source, os.path.join(bindir, name))
