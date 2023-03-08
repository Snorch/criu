#!/usr/bin/env python

import multiprocessing
import os
import pty
import shutil
import subprocess
import sys
import time


CRIU_BIN = "../../../criu/criu"
CRIU_NS = "../../../scripts/criu-ns"
IMG_DIR = "dumpdir"


def check_dumpdir():
    if os.path.isdir(IMG_DIR):
        shutil.rmtree(IMG_DIR)
    os.mkdir(IMG_DIR, 0o755)


def create_pty():
    fd_m, fd_s = pty.openpty()
    return (os.fdopen(fd_m, "wb"), os.fdopen(fd_s, "wb"))


def criu_ns_dump(pid, shell_job=False):
    if shell_job:
        cmd = [CRIU_NS, "dump", "-D", "dumpdir", "-v4",
               "--shell-job", "-t", str(pid), "--criu-binary",
               CRIU_BIN]
    else:
        cmd = [CRIU_NS, "dump", "-D", "dumpdir", "-v4",
               "-t", str(pid), "--criu-binary", CRIU_BIN]
    ret = subprocess.Popen(cmd).wait()
    return ret


def criu_ns_restore(shell_job=False, restore_detached=False):
    if shell_job:
        cmd = [CRIU_NS, "restore", "-D", "dumpdir", "-v4",
               "--shell-job", "--criu-binary", CRIU_BIN]
    else:
        cmd = [CRIU_NS, "restore", "-D", "dumpdir", "-v4",
               "--criu-binary", CRIU_BIN]
    if restore_detached:
        cmd = [CRIU_NS, "restore", "-D", "dumpdir", "-v4",
               "--restore-detached", "--criu-binary",
                CRIU_BIN]
    ret = subprocess.Popen(cmd).wait()
    return ret


def test_dump_and_restore_with_shell_job():
    check_dumpdir()
    os.setsid()

    with open("running", "w") as file:
        pass
    pid = os.fork()
    if pid == 0:
        while True:
            if not os.access("running", os.F_OK):
                sys.exit(0)
            time.sleep(1)

    ret = criu_ns_dump(pid, shell_job=True)
    if ret != 0:
        sys.exit(ret)

    os.unlink("running")
    fd_m, fd_s = create_pty()
    pid = os.fork()
    if pid == 0:
        fd_m.close()
        # since criu-ns takes control of the tty stdin
        os.dup2(fd_s.fileno(), 0)
        ret = criu_ns_restore(shell_job=True)
        if ret != 0:
            sys.exit(ret)
        os._exit(0)

    fd_s.close()
    os.waitpid(pid, 0)


def test_dump_and_restore_without_shell_job(restore_detached=False):
    check_dumpdir()

    with open("running", "w") as file:
        pass
    fd_m, fd_s = create_pty()
    pid = os.fork()
    fd_m.close()
    if pid == 0:
        os.setsid()
        os.dup2(fd_s.fileno(), 0)
        os.dup2(fd_s.fileno(), 1)
        os.dup2(fd_s.fileno(), 2)
        while True:
            if not os.access("running", os.F_OK):
                sys.exit(0)
            time.sleep(1)

    ret = criu_ns_dump(pid)
    if ret != 0:
        sys.exit(ret)

    os.unlink("running")
    fd_m, fd_s = create_pty()
    pid = os.fork()
    if pid == 0:
        os.setsid()
        ret = 0
        if restore_detached:
            ret = criu_ns_restore(restore_detached=True)
        else:
            ret = criu_ns_restore()

        if ret != 0:
            sys.exit(ret)
        os._exit(0)

    fd_m.close()
    fd_s.close()
    os.waitpid(pid, 0)


def test_dump_and_restore_pidns():
    SLEEP_SEC = 5
    check_dumpdir()

    def _dumpee():
        time.sleep(SLEEP_SEC)

    d_process = multiprocessing.Process(target=_dumpee, daemon=True)
    d_process.start()
    dumpee_pid = d_process.pid
    ret = criu_ns_dump(dumpee_pid, shell_job=True)
    if ret != 0:
        sys.exit(ret)

    def _restore():
        fd_m, fd_s = create_pty()
        os.dup2(fd_s.fileno(), 0)
        ret = criu_ns_restore(shell_job=True)
        if ret != 0:
            sys.exit(ret)
        fd_s.close()
        fd_m.close()

    def _redump(pid):
        ret = criu_ns_dump(pid, shell_job=True)
        if ret != 0:
            sys.exit(ret)

    def _re_restore():
        fd_m, fd_s = create_pty()
        os.dup2(fd_s.fileno(), 0)
        ret = criu_ns_restore(shell_job=True)
        if ret != 0:
            sys.exit(ret)
        fd_s.close()
        fd_m.close()

    r_process = multiprocessing.Process(target=_restore, daemon=True)
    r_process.start()
    restored_pid = r_process.pid

    rd_process = multiprocessing.Process(target=_redump,
                                         args=(restored_pid,))
    if r_process.is_alive():
        rd_process.start()
        rd_process.join()

    rr_process = multiprocessing.Process(target=_re_restore)
    if rd_process.exitcode == 0:
        rr_process.start()
        rr_process.join()


if __name__ == "__main__":
    test_dump_and_restore_with_shell_job()
    test_dump_and_restore_without_shell_job()
    test_dump_and_restore_without_shell_job(restore_detached=True)
    test_dump_and_restore_pidns()
    sys.exit(0)
