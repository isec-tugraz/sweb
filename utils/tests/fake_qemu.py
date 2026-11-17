#!/usr/bin/env python3
"""Small QEMU protocol fixture; exercises the harness, never verifies a kernel."""
import json
import os
import signal
import sys
import time
from pathlib import Path

args = sys.argv[1:]
scenario = os.environ.get("FAKE_SCENARIO", "pass")
if args == ["-machine", "help"]:
    if scenario == "probe-error":
        sys.exit(1)
    print("Supported machines are:")
    if scenario != "unsupported":
        print("raspi2b Raspberry Pi 2B")
        print("raspi3b Raspberry Pi 3B")
    sys.exit(0)

Path("qemu.pid").write_text(str(os.getpid()))
def stop(_signal, _frame):
    Path("qemu.stopped").touch()
    sys.exit(0)
signal.signal(signal.SIGTERM, stop)
if scenario == "exit":
    sys.exit(3)
if scenario == "no-output":
    while True:
        time.sleep(0.1)

is_arm = "-M" in args
stream = sys.stdout.buffer
input_stream = sys.stdin.buffer
Path("qemu.ready").touch()

def output(data):
    if is_arm:
        stream.write(data)
        stream.flush()
    else:
        log = args[args.index("-debugcon") + 1][5:]
        with open(log, "ab") as handle:
            handle.write(data)

def boot():
    if scenario != "no-prompt":
        # Startup help must NOT satisfy the subsequent command assertion.
        output(b"List all commands\nSWEB: /> ")

def help_response(command):
    if command != "help":
        sys.exit(4)
    if scenario == "help-fails":
        output(b"Unknown command\nSWEB: /> ")
    elif scenario == "no-reprompt":
        output(b"List all commands\n")
    elif scenario == "wrong-order":
        output(b"SWEB: /> List all commands\n")
    else:
        # Split the marker across writes like real serial/debug output.
        output(b"List all ")
        output(b"commands\nSWEB: /> ")

command = ""
if is_arm:
    boot()
    while True:
        key = input_stream.read(1)
        if not key:
            break
        if key == b"\r":
            help_response(command)
            command = ""
        else:
            command += key.decode()
else:
    stream.write(b'{"QMP": {}}\n')
    stream.flush()
    booted = False
    while line := input_stream.readline():
        request = json.loads(line)
        stream.write(b'{"event":"TEST"}\n')
        if scenario == "qmp-error":
            stream.write(b'{"error":{"desc":"injected failure"}}\n')
            stream.flush()
            continue
        stream.write(b'{"return":{}}\n')
        stream.flush()
        if request["execute"] == "human-monitor-command":
            key = request["arguments"]["command-line"].split()[1]
            if key == "ret":
                if not booted:
                    boot()
                    booted = True
                elif command:
                    help_response(command)
                    command = ""
            else:
                command += key
