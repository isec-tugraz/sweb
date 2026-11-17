"""Regression tests of CI failure detection using fake tools, not guest builds."""
import json
import os
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "utils"))
import qemu_boot  # noqa: E402


class BootTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.build = Path(self.temporary.name)
        self.bin = self.build / "bin"
        self.bin.mkdir()
        fixture = Path(__file__).with_name("fake_qemu.py").read_text()
        for executable, _ in qemu_boot.TARGETS.values():
            path = self.bin / executable
            path.write_text(f"#!{sys.executable}\n" + fixture.split("\n", 1)[1])
            path.chmod(0o755)
        for filename in ("SWEB.qcow2", "kernel.x"):
            (self.build / filename).touch()
        self.env = {**os.environ, "PATH": f"{self.bin}:{os.environ['PATH']}"}

    def run_boot(self, architecture="x86_64", scenario="pass", timeout=2):
        result = subprocess.run(
            [sys.executable, str(ROOT / "utils/qemu_boot.py"), architecture,
             "--timeout", str(timeout)], cwd=self.build,
            env={**self.env, "FAKE_SCENARIO": scenario},
            capture_output=True, text=True, timeout=15,
        )
        report = json.loads((self.build / "ci-logs/result.json").read_text())
        return result, report

    def assert_stopped(self):
        pid = int((self.build / "qemu.pid").read_text())
        with self.assertRaises(ProcessLookupError):
            os.kill(pid, 0)

    def test_all_architectures_and_input_transports(self):
        for architecture in qemu_boot.TARGETS:
            with self.subTest(architecture=architecture):
                result, report = self.run_boot(architecture)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual(report["status"], "PASS")
                self.assert_stopped()

    def test_boot_and_shell_failures_are_nonzero(self):
        for scenario in ("no-prompt", "help-fails", "no-reprompt", "wrong-order",
                         "exit", "no-output", "qmp-error"):
            with self.subTest(scenario=scenario):
                result, report = self.run_boot(scenario=scenario)
                self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                self.assertEqual(report["status"], "FAIL")
                self.assert_stopped()

    def test_arm_help_failure(self):
        result, report = self.run_boot("arm_rpi2", "help-fails")
        self.assertEqual(result.returncode, 1)
        self.assertEqual(report["status"], "FAIL")
        self.assert_stopped()

    def test_unsupported_machines_are_skipped(self):
        for architecture in ("arm_rpi2", "armv8_rpi3"):
            with self.subTest(architecture=architecture):
                result, report = self.run_boot(architecture, "unsupported")
                self.assertEqual(result.returncode, 77)
                self.assertEqual(report["status"], "SKIP")
                self.assertFalse((self.build / "qemu.pid").exists())

    def test_failed_machine_probe_is_failure(self):
        result, report = self.run_boot("arm_rpi2", "probe-error")
        self.assertEqual(result.returncode, 1)
        self.assertEqual(report["status"], "FAIL")

    def test_missing_emulator_is_skipped(self):
        with patch.object(qemu_boot.shutil, "which", return_value=None):
            with self.assertRaises(qemu_boot.Skipped):
                qemu_boot.verify_boot("x86_64", self.build, 1)

    def test_stale_log_cannot_pass(self):
        (self.build / "ci-logs").mkdir()
        (self.build / "ci-logs/guest.log").write_bytes(
            b"SWEB: /> List all commands\nSWEB: />")
        result, _ = self.run_boot(scenario="no-prompt")
        self.assertEqual(result.returncode, 1)
        self.assert_stopped()

    def test_missing_image_is_failure(self):
        (self.build / "SWEB.qcow2").unlink()
        result, report = self.run_boot()
        self.assertEqual(result.returncode, 1)
        self.assertIn("Missing SWEB.qcow2", report["detail"])

    def test_termination_cleans_up_qemu(self):
        process = subprocess.Popen(
            [sys.executable, str(ROOT / "utils/qemu_boot.py"), "x86_64"],
            cwd=self.build, env={**self.env, "FAKE_SCENARIO": "no-prompt"},
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        )
        self.addCleanup(lambda: process.poll() is None and process.kill())
        deadline = time.monotonic() + 5
        while not (self.build / "qemu.ready").exists():
            if time.monotonic() > deadline:
                self.fail("QEMU fixture did not start")
            time.sleep(0.05)
        process.send_signal(signal.SIGTERM)
        process.communicate(timeout=10)
        self.assertEqual(process.returncode, 1)
        self.assert_stopped()

    def test_build_failure_stops_before_qemu(self):
        cmake = self.bin / "cmake"
        for fail_at in ("configure", "build"):
            with self.subTest(fail_at=fail_at):
                cmake.write_text(
                    "#!/bin/bash\n"
                    'echo "cmake $*" >> calls\n'
                    + ('exit 42\n' if fail_at == "configure" else
                       'if [[ $1 == --build ]]; then exit 42; fi\n')
                )
                cmake.chmod(0o755)
                result = subprocess.run(
                    ["bash", str(ROOT / "utils/travis_test.sh"), "x86_64"],
                    cwd=self.build, env=self.env, capture_output=True, text=True,
                    timeout=10,
                )
                self.assertEqual(result.returncode, 42, result.stderr)
                self.assertFalse((self.build / "qemu.pid").exists())
                calls = (self.build / "calls").read_text()
                self.assertEqual("--build" in calls, fail_at == "build")
                (self.build / "calls").unlink()

    def test_wrong_architecture_cache_is_rejected(self):
        (self.build / "CMakeCache.txt").write_text("ARCH:UNINITIALIZED=arm/rpi2\n")
        result = subprocess.run(
            ["bash", str(ROOT / "utils/travis_test.sh"), "x86_64"],
            cwd=self.build, env=self.env, capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 2)
        self.assertIn("fresh build directory", result.stderr)


if __name__ == "__main__":
    unittest.main()
