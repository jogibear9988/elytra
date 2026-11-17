#!/usr/bin/env python3
"""Exact Windows SDK broker compositions; no native confinement claim from these controls."""

import _support

import hashlib
import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "broker_composer", ROOT / "tooling/windows-sandbox/broker.py"
)
broker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(broker)


class BrokerSources(unittest.TestCase):
    def test_pins_exact_reversal_and_added_methods(self):
        for name, expected in broker.PINS.items():
            original = (ROOT / "vendor/ladybird/Libraries/LibIPC" / Path(name).name).read_text()
            self.assertEqual(hashlib.sha256(original.encode()).hexdigest(), expected)
            result = broker.compose(name, original)
            restored = result if name.endswith(".h") else result.removeprefix(broker.PREFIX)
            if name == "HandleBrokerServer.cpp":
                restored = restored.removeprefix("#include <AK/ScopeGuard.h>\n")
            for before, after in reversed(broker.changes(name, original)):
                self.assertEqual(restored.count(after), 1)
                restored = restored.replace(after, before, 1)
            self.assertEqual(restored, original)
            if name.endswith(".h"):
                # Add callable cleanup/admission only; no private fields, vtable,
                # existing signatures or source license are replaced.
                self.assertEqual(result.count("can_park_on_behalf_of"), 1)
                self.assertEqual(result.count("void forget_owner(pid_t owner);"), 1)
                self.assertEqual(result.count("virtual "), original.count("virtual "))

    def test_missing_duplicate_double_and_unrelated_drift_refused(self):
        for name in broker.PINS:
            original = (ROOT / "vendor/ladybird/Libraries/LibIPC" / Path(name).name).read_text()
            invalids = [original + "\n", broker.compose(name, original)]
            for before, _ in broker.changes(name, original):
                invalids.extend([original.replace(before, "", 1), original + before])
            for invalid in invalids:
                with self.assertRaises(ValueError):
                    broker.compose(name, invalid)

    def test_failure_preserves_all_previous_outputs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in broker.PINS:
                target = root / "vendor/ladybird/Libraries/LibIPC" / Path(name).name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(
                    (ROOT / "vendor/ladybird/Libraries/LibIPC" / Path(name).name).read_bytes()
                )
            previous = broker.ROOT
            try:
                broker.ROOT = root
                output = root / "output"
                broker.prepare(output)
                accepted = {
                    p.relative_to(output): p.read_bytes() for p in output.rglob("*") if p.is_file()
                }
                (root / "vendor/ladybird/Libraries/LibIPC/HandleBrokerServer.cpp").write_text(
                    "drift"
                )
                with self.assertRaises(ValueError):
                    broker.prepare(output)
                self.assertEqual(
                    {
                        p.relative_to(output): p.read_bytes()
                        for p in output.rglob("*")
                        if p.is_file()
                    },
                    accepted,
                )
                with self.assertRaises(ValueError):
                    broker.prepare(root / "absent")
                self.assertFalse((root / "absent").exists())
            finally:
                broker.ROOT = previous

    def test_preflight_duplication_rollback_and_actual_map_refunds(self):
        original = (ROOT / "vendor/ladybird/Libraries/LibIPC/HandleBrokerServer.cpp").read_text()
        server = broker.compose("HandleBrokerServer.cpp", original)
        self.assertLess(
            server.index("can_park_on_behalf_of(m_pid"),
            server.index("TRY(copy_handles_out_of_child"),
        )
        method = server[server.index("    ErrorOr<Vector<Attachment>> copy_handles_out_of_child") :]
        self.assertLess(method.index("valid_handle_value(value)"), method.index("DuplicateHandle("))
        self.assertIn(
            "m_server.m_broker.forget_owner(m_pid);\n        CloseHandle(m_process_handle)", server
        )
        self.assertIn("copies.unchecked_append(copied_handle);", server)
        self.assertIn("DUPLICATE_CLOSE_SOURCE", server)
        self.assertLess(
            server.index("ArmedScopeGuard rollback"), server.index("copies.unchecked_append")
        )
        self.assertLess(server.index("copies.unchecked_append"), server.index("rollback.disarm()"))
        original = (ROOT / "vendor/ladybird/Libraries/LibIPC/HandleBroker.cpp").read_text()
        local = broker.compose("HandleBroker.cpp", original)
        self.assertIn(
            "MutexLocker locker(m_mutex);\n    if (!Firefly::WindowsBroker::can_park", local
        )
        for original_step in (
            "m_parked_attachments.take(token)",
            "parked_attachments.owner == owner && parked_attachments.channel == channel",
        ):
            self.assertIn(original_step, local)
        self.assertIn("return record.owner == owner;", local)
        # No parallel quota ledger can miss Browser/other-child token redemption.
        self.assertNotIn("token_ledger", local)


if __name__ == "__main__":
    unittest.main()
