from __future__ import annotations

import os
import signal
import subprocess
import tempfile
import time
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
LAUNCHER = ROOT / "build" / "tests" / "mosh"
FAKE_DBCLIENT = ROOT / "tests" / "fake-bin" / "dbclient"
FAKE_CLIENT = ROOT / "tests" / "fake-bin" / "mosh-client"


class LauncherTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        (ROOT / "build" / "tests").mkdir(parents=True, exist_ok=True)
        subprocess.check_call([
            "cc",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-std=c11",
            str(ROOT / "launcher" / "mosh.c"),
            "-o",
            str(LAUNCHER),
        ])
        FAKE_DBCLIENT.chmod(0o755)
        FAKE_CLIENT.chmod(0o755)

    def run_launcher(self, args: list[str], extra_env: dict[str, str] | None = None, timeout: float = 5):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        env = os.environ.copy()
        env.update({
            "FAKE_LOG_DIR": tmp.name,
            "FAKE_SSH_CONNECTION": "192.0.2.10 12345 198.51.100.20 22",
            "KO_MOSH_DISABLE_DNS": "1",
        })
        env.pop("MOSH_KEY", None)
        if extra_env:
            env.update(extra_env)
        cmd = [
            str(LAUNCHER),
            "--client", str(FAKE_CLIENT),
            "--ssh", str(FAKE_DBCLIENT),
            *args,
        ]
        proc = subprocess.run(cmd, text=True, capture_output=True, env=env, timeout=timeout)
        return proc, Path(tmp.name)

    def read_log(self, log_dir: Path, name: str) -> str:
        return (log_dir / name).read_text()

    def test_successful_startup_streams_output_and_execs_client(self):
        proc, logs = self.run_launcher(["user@example.com"])
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertIn("remote banner", proc.stdout)
        client_log = self.read_log(logs, "mosh-client.argv")
        self.assertIn("[198.51.100.20]", client_log)
        self.assertIn("[60001]", client_log)
        self.assertIn("MOSH_KEY_PRESENT=yes", client_log)
        self.assertNotIn("abcdefghijklmnopqrstuv", proc.stdout + proc.stderr + client_log)

    def test_client_process_remains_top_level_after_handoff(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        env = os.environ.copy()
        env.update({
            "FAKE_LOG_DIR": tmp.name,
            "FAKE_CLIENT_SLEEP": "1",
            "FAKE_SSH_CONNECTION": "192.0.2.10 12345 198.51.100.20 22",
            "KO_MOSH_DISABLE_DNS": "1",
        })
        proc = subprocess.Popen([
            str(LAUNCHER),
            "--client", str(FAKE_CLIENT),
            "--ssh", str(FAKE_DBCLIENT),
            "user@example.com",
        ], text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
        log = Path(tmp.name) / "mosh-client.argv"
        deadline = time.time() + 5
        while time.time() < deadline and not log.exists():
            time.sleep(0.05)
        self.assertTrue(log.exists(), "mosh-client did not start")
        self.assertIsNone(proc.poll(), "launcher must hand off to a live mosh-client process")
        proc.terminate()
        proc.communicate(timeout=5)
        self.assertEqual(proc.returncode, 143)

    def test_password_prompt_on_stderr_is_not_captured_as_protocol(self):
        proc, _ = self.run_launcher(["user@example.com"], {"FAKE_STDERR": "Password:"})
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertIn("Password:", proc.stderr)

    def test_ipv4_ssh_connection(self):
        proc, logs = self.run_launcher(["user@example.com"], {
            "FAKE_SSH_CONNECTION": "10.0.0.1 50000 10.0.0.2 22",
        })
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertIn("[10.0.0.2]", self.read_log(logs, "mosh-client.argv"))

    def test_ipv6_ssh_connection(self):
        proc, logs = self.run_launcher(["-6", "user@[2001:db8::20]"], {
            "FAKE_SSH_CONNECTION": "2001:db8::1 50000 2001:db8::20 22",
        })
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertIn("[2001:db8::20]", self.read_log(logs, "mosh-client.argv"))
        self.assertIn("[-6]", self.read_log(logs, "dbclient.argv"))

    def test_numeric_hostname_fallback(self):
        proc, logs = self.run_launcher(["127.0.0.1"], {
            "FAKE_SCENARIO": "missing-remote-ip",
            "KO_MOSH_DISABLE_DNS": "",
        })
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertIn("[127.0.0.1]", self.read_log(logs, "mosh-client.argv"))

    def test_requested_udp_port(self):
        proc, logs = self.run_launcher(["-p", "60000", "user@example.com"])
        self.assertEqual(proc.returncode, 0, proc.stderr)
        dbclient_log = self.read_log(logs, "dbclient.argv")
        self.assertIn("-p", dbclient_log)
        self.assertIn("60000", dbclient_log)

    def test_requested_udp_port_range(self):
        proc, logs = self.run_launcher(["--port", "60000:60010", "user@example.com"])
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertIn("'60000:60010'", self.read_log(logs, "dbclient.argv"))

    def test_custom_ssh_port_and_identity(self):
        proc, logs = self.run_launcher([
            "--ssh-port", "2222",
            "--identity", "/mnt/onboard/ssh/id_dropbear",
            "user@example.com",
        ])
        self.assertEqual(proc.returncode, 0, proc.stderr)
        dbclient_log = self.read_log(logs, "dbclient.argv")
        self.assertIn("[-p]\n[2222]", dbclient_log)
        self.assertIn("[-i]\n[/mnt/onboard/ssh/id_dropbear]", dbclient_log)

    def test_prediction_modes_and_no_init(self):
        proc, logs = self.run_launcher(["--predict", "never", "--no-init", "user@example.com"])
        self.assertEqual(proc.returncode, 0, proc.stderr)
        client_log = self.read_log(logs, "mosh-client.argv")
        self.assertIn("MOSH_PREDICTION_DISPLAY=never", client_log)
        self.assertIn("MOSH_NO_TERM_INIT=1", client_log)

    def test_packaged_terminfo_overrides_terminal_environment(self):
        terminfo = ROOT / "tests" / "share" / "terminfo"
        terminfo.mkdir(parents=True, exist_ok=True)
        proc, logs = self.run_launcher(["user@example.com"], {
            "TERM": "xterm",
            "TERMINFO": "/wrong/terminfo",
        })
        self.assertEqual(proc.returncode, 0, proc.stderr)
        client_log = self.read_log(logs, "mosh-client.argv")
        self.assertIn("TERM=vt52", client_log)
        self.assertIn(f"TERMINFO={terminfo}", client_log)

    def test_missing_mosh_connect_is_rejected(self):
        proc, _ = self.run_launcher(["user@example.com"], {"FAKE_SCENARIO": "no-connect"})
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn("Did not find mosh server startup message", proc.stderr)

    def test_malformed_port_is_rejected(self):
        proc, _ = self.run_launcher(["user@example.com"], {"FAKE_SCENARIO": "malformed-port"})
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn("Bad MOSH CONNECT", proc.stderr)

    def test_malformed_key_is_rejected(self):
        proc, _ = self.run_launcher(["user@example.com"], {"FAKE_SCENARIO": "malformed-key"})
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn("Bad MOSH CONNECT", proc.stderr)

    def test_duplicate_connect_is_rejected(self):
        proc, _ = self.run_launcher(["user@example.com"], {"FAKE_SCENARIO": "duplicate-connect"})
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn("Duplicate MOSH CONNECT", proc.stderr)

    def test_missing_remote_ip_is_rejected(self):
        proc, _ = self.run_launcher(["example.invalid"], {"FAKE_SCENARIO": "missing-remote-ip"})
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn("Did not find remote IP address", proc.stderr)

    def test_remote_server_failure_is_reported(self):
        proc, _ = self.run_launcher(["user@example.com"], {"FAKE_SCENARIO": "server-failure"})
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn("mosh-server: command not found", proc.stdout)

    def test_ssh_child_failure_preserves_status(self):
        proc, _ = self.run_launcher(["user@example.com"], {"FAKE_SCENARIO": "ssh-failure"})
        self.assertEqual(proc.returncode, 42)
        self.assertIn("SSH startup failed", proc.stderr)

    def test_client_exec_failure(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        env = {
            "FAKE_LOG_DIR": tmp.name,
            "FAKE_SSH_CONNECTION": "192.0.2.10 12345 198.51.100.20 22",
        }
        proc = subprocess.run([
            str(LAUNCHER),
            "--client", "/no/such/mosh-client",
            "--ssh", str(FAKE_DBCLIENT),
            "user@example.com",
        ], text=True, capture_output=True, env={**os.environ, **env}, timeout=5)
        self.assertEqual(proc.returncode, 1)
        self.assertIn("Cannot determine terminal color count", proc.stderr)

    def test_signal_interruption_forwards_to_dbclient(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        env = os.environ.copy()
        env.update({
            "FAKE_LOG_DIR": tmp.name,
            "FAKE_SCENARIO": "sleep",
            "KO_MOSH_DISABLE_DNS": "1",
        })
        proc = subprocess.Popen([
            str(LAUNCHER),
            "--client", str(FAKE_CLIENT),
            "--ssh", str(FAKE_DBCLIENT),
            "user@example.com",
        ], text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
        time.sleep(0.3)
        proc.send_signal(signal.SIGTERM)
        stdout, stderr = proc.communicate(timeout=5)
        self.assertEqual(proc.returncode, 143, (stdout, stderr))

    def test_arguments_containing_spaces_are_remote_shell_quoted(self):
        proc, logs = self.run_launcher(["user@example.com", "tmux", "new-session", "-A", "-s", "kobo session"])
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertIn("'kobo session'", self.read_log(logs, "dbclient.argv"))

    def test_attempted_userhost_shell_injection_is_rejected(self):
        proc, _ = self.run_launcher(["user@example.com;touch /tmp/pwn"])
        self.assertEqual(proc.returncode, 2)
        self.assertIn("Invalid [user@]host", proc.stderr)

    def test_attempted_remote_command_injection_is_quoted(self):
        proc, logs = self.run_launcher(["user@example.com", "echo", "x; touch /tmp/pwn"])
        self.assertEqual(proc.returncode, 0, proc.stderr)
        dbclient_log = self.read_log(logs, "dbclient.argv")
        self.assertIn("'x; touch /tmp/pwn'", dbclient_log)
        self.assertNotIn("abcdefghijklmnopqrstuv", dbclient_log)


if __name__ == "__main__":
    unittest.main()
