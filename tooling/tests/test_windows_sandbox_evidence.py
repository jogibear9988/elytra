#!/usr/bin/env python3
"""Platform-independent negative controls for the native sandbox report verifier."""

import unittest

from test_windows_sandbox import network_evidence


class NetworkEvidenceTests(unittest.TestCase):
    def report(self, operation="WSAPoll timeout", error=10060, status=0, capability=0):
        return {
            "networkOperation": operation,
            "networkError": error,
            "networkPolicyStatus": status,
            "networkPolicyError": capability,
        }

    def test_exact_policy_diagnoses(self):
        for capability in (1, 2, 3):
            self.assertIn(
                "OS capability-policy", network_evidence(self.report(capability=capability))
            )

    def test_observed_access_denial(self):
        for operation in ("WSASocket", "connect", "SO_ERROR"):
            self.assertIn("observed", network_evidence(self.report(operation, 10013)))

    def test_timeout_without_valid_diagnosis_fails(self):
        for capability in (0, 4, 255):
            with self.assertRaises(AssertionError):
                network_evidence(self.report(capability=capability))
        for status in (5, 126, 127):  # Denied, missing library, missing procedure.
            for capability in (1, 2, 3):
                with self.assertRaises(AssertionError):
                    network_evidence(self.report(status=status, capability=capability))

    def test_socket_success_or_other_failure_never_passes(self):
        for operation, error in (
            ("connect", 0),
            ("SO_ERROR", 0),
            ("SO_ERROR", 10061),
            ("WSAPoll failed", 10060),
        ):
            for capability in (1, 2, 3):
                with self.assertRaises(AssertionError):
                    network_evidence(self.report(operation, error, capability=capability))


if __name__ == "__main__":
    unittest.main()
