#!/usr/bin/env python3
"""Boot SWEB and verify a fresh help response. Exit 77 means unsupported/skipped."""

import argparse
import json
import os
import select
import shutil
import signal
import subprocess
import time
from pathlib import Path


TARGETS = {
    "x86_64": ("qemu-system-x86_64", None),
    "x86_32": ("qemu-system-i386", None),
    "x86_32_pae": ("qemu-system-i386", None),
    "arm_rpi2": ("qemu-system-arm", "raspi2b"),
    "armv8_rpi3": ("qemu-system-aarch64", "raspi3b"),
}
PROMPT = b"SWEB: />"
HELP = b"List all commands"


class Skipped(Exception):
    pass


class BootFailure(Exception):
    pass


def check_running(process):
    if process.poll() is not None:
        raise BootFailure(f"QEMU exited unexpectedly (status {process.returncode})")


class QMP:
    def __init__(self, process, timeout):
        self.process = process
        self.timeout = timeout
        self.buffer = b""
        greeting = self.read(time.monotonic() + timeout)
        if "QMP" not in greeting:
            raise BootFailure("Missing QMP greeting")
        self.execute("qmp_capabilities")

    def read(self, deadline):
        while b"\n" not in self.buffer:
            check_running(self.process)
            remaining = deadline - time.monotonic()
            if remaining <= 0 or not select.select([self.process.stdout], [], [], remaining)[0]:
                raise BootFailure("Timed out waiting for QMP response")
            data = os.read(self.process.stdout.fileno(), 65536)
            if not data:
                raise BootFailure("QMP connection closed")
            self.buffer += data
        line, self.buffer = self.buffer.split(b"\n", 1)
        return json.loads(line)

    def execute(self, command, arguments=None):
        request = {"execute": command}
        if arguments is not None:
            request["arguments"] = arguments
        self.process.stdin.write(json.dumps(request).encode() + b"\n")
        deadline = time.monotonic() + self.timeout
        # QMP can interleave asynchronous events with a command's response.
        while True:
            response = self.read(deadline)
            if "error" in response:
                raise BootFailure(f"QMP command failed: {response['error']}")
            if "return" in response:
                return response["return"]

    def key(self, key):
        self.execute("human-monitor-command", {"command-line": f"sendkey {key} 50"})


def verify_boot(architecture, log_dir, timeout):
    executable, machine = TARGETS[architecture]
    if shutil.which(executable) is None:
        raise Skipped(f"{executable} is not installed")
    if machine:
        supported = subprocess.run(
            [executable, "-machine", "help"], capture_output=True, timeout=10,
            text=True, check=True,
        )
        if not any(line.split() and line.split()[0] == machine
                   for line in supported.stdout.splitlines()):
            raise Skipped(f"{executable} does not support {machine}")
    for filename in ("SWEB.qcow2", "kernel.x"):
        if not Path(filename).is_file():
            raise BootFailure(f"Missing {filename}; build {architecture} first")

    guest_log = log_dir / "guest.log"
    guest_log.write_bytes(b"")  # Never accept output from a previous run.
    command = [executable, "-accel", "tcg", "-display", "none", "-no-reboot",
               "-snapshot", "-monitor", "none"]
    if machine:
        command += ["-M", machine, "-m", "1024", "-kernel", "kernel.x",
                    "-drive", "file=SWEB.qcow2,if=sd,format=qcow2",
                    "-serial", "stdio"]
    else:
        command += ["-m", "8M", "-cpu", "qemu64", "-nic", "none",
                    "-drive", "file=SWEB.qcow2,index=0,media=disk,format=qcow2",
                    "-debugcon", f"file:{guest_log}", "-serial", "none",
                    "-qmp", "stdio"]
    (log_dir / "command.json").write_text(json.dumps(command, indent=2) + "\n")
    with (log_dir / "qemu.log").open("wb") as diagnostics:
        process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   stderr=diagnostics, bufsize=0)
        qmp = None
        try:
            if not machine:
                qmp = QMP(process, min(timeout, 10))

            def output():
                check_running(process)
                if machine:
                    with guest_log.open("ab") as log:
                        while select.select([process.stdout], [], [], 0)[0]:
                            data = os.read(process.stdout.fileno(), 65536)
                            if not data:
                                raise BootFailure("QEMU serial connection closed")
                            log.write(data)
                return guest_log.read_bytes()

            # GRUB has no autoboot timeout. Press Enter until it boots.
            deadline = time.monotonic() + timeout
            next_enter = time.monotonic() + 1
            while PROMPT not in output():
                if time.monotonic() >= deadline:
                    raise BootFailure("Timed out waiting for the shell prompt")
                if qmp and time.monotonic() >= next_enter:
                    qmp.key("ret")
                    next_enter = time.monotonic() + 1
                time.sleep(0.05)

            # Help is also printed at startup, so examine only NEW output.
            offset = len(output())
            for key in "help\r":
                if qmp:
                    qmp.key("ret" if key == "\r" else key)
                else:
                    process.stdin.write(key.encode())
                time.sleep(0.1)
            deadline = time.monotonic() + timeout
            while True:
                fresh_output = output()[offset:]
                help_at = fresh_output.find(HELP)
                if help_at >= 0 and PROMPT in fresh_output[help_at + len(HELP):]:
                    return "booted, executed help, and returned to the shell prompt"
                if time.monotonic() >= deadline:
                    raise BootFailure("Shell did not return help output and a new prompt")
                time.sleep(0.05)
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
            else:
                process.wait()
            process.stdin.close()
            process.stdout.close()


def positive_timeout(value):
    value = float(value)
    if not 0 < value <= 300:
        raise argparse.ArgumentTypeError("timeout must be greater than 0 and at most 300 seconds")
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("architecture", choices=TARGETS)
    parser.add_argument("--log-dir", type=Path, default=Path("ci-logs"))
    parser.add_argument("--timeout", type=positive_timeout, default=60,
                        help="maximum seconds per boot/command phase (default: 60)")
    args = parser.parse_args()
    args.log_dir.mkdir(parents=True, exist_ok=True)

    def interrupted(signum, _frame):
        raise BootFailure(f"Interrupted by signal {signum}")

    signal.signal(signal.SIGTERM, interrupted)
    signal.signal(signal.SIGINT, interrupted)
    try:
        detail = verify_boot(args.architecture, args.log_dir.resolve(), args.timeout)
        status, code = "PASS", 0
    except Skipped as error:
        status, code, detail = "SKIP", 77, str(error)
    except (BootFailure, OSError, ValueError, subprocess.SubprocessError) as error:
        status, code, detail = "FAIL", 1, str(error)
    result = {"architecture": args.architecture, "status": status, "detail": detail}
    (args.log_dir / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(f"{status}: {args.architecture}: {detail}")
    print(f"Logs: {args.log_dir.resolve()}")
    return code


if __name__ == "__main__":
    raise SystemExit(main())
