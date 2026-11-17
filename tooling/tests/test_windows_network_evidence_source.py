#!/usr/bin/env python3
"""Source-only guards for the native WFP probe; never claim Windows execution."""

import ast
import copy
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


class NetworkEvidenceSource(unittest.TestCase):
    def setUp(self):
        self.header = (ROOT / "firefly/native/sandbox/windows/NetworkEvidence.h").read_text()
        self.probe = (ROOT / "firefly/native/sandbox/windows/Probe.cpp").read_text()
        self.runner = (ROOT / "tooling/tests/test_windows_sandbox.py").read_text()

    def test_read_only_bounded_lifetime(self):
        for forbidden in (
            "FwpmEngineSetOption",
            "FwpmFilterAdd",
            "FwpmNetEventsSetSecurityInfo",
            "NetworkIsolationSetAppContainerConfig",
            "netsh",
            "AdjustTokenPrivileges",
        ):
            self.assertNotIn(forbidden, self.header)
        for required in (
            "FwpmEngineGetOption0",
            "FWPM_ENGINE_COLLECT_NET_EVENTS",
            "FwpmNetEventSubscribe2",
            "FwpmNetEventUnsubscribe0",
            "FwpmEngineClose0",
            "FwpmGetAppIdFromFileName0",
            "std::array<Diagnostic, 8>",
            "std::chrono::seconds(2)",
            "timestamp(birth) == timestamp(m_birth)",
            "GetProcessId(m_child) == m_pid",
            "std::terminate()",
        ):
            self.assertIn(required, self.header)
        self.assertLess(
            self.probe.index("network_observer->arm()"), self.probe.index("child->resume(error)")
        )
        self.assertLess(
            self.probe.index("network_observer->finish(exit)"),
            self.probe.index("network_observer.reset()"),
        )
        self.assertLess(
            self.probe.index("network_observer.reset()"), self.probe.index("child.reset()")
        )

    def test_exact_native_predicate_and_negative_controls(self):
        for required in (
            "event.type != FWPM_NET_EVENT_TYPE_CAPABILITY_DROP",
            "!event.capabilityDrop",
            "(header.flags & required) != required",
            "header.ipProtocol != IPPROTO_TCP",
            "header.localAddrV4 != INADDR_LOOPBACK",
            "header.remoteAddrV4 != INADDR_LOOPBACK",
            "header.localPort == 0",
            "header.remotePort != identity.remote_port",
            "EqualSid(header.packageSid, identity.package)",
            "std::memcmp(header.appId.data, identity.application.data, header.appId.size)",
            "timestamp(header.timeStamp) < identity.begin",
            "timestamp(header.timeStamp) > identity.end",
            "drop.isLoopback == TRUE",
            "drop.filterId != 0",
        ):
            self.assertIn(required, self.header)
        for required in (
            "FWPM_NET_EVENT_TYPE_CLASSIFY_DROP",
            "IPPROTO_UDP",
            "FWP_IP_VERSION_V6",
            "value.header.packageSid = nullptr",
            "value.header.timeStamp.dwLowDateTime = 99",
            "value.header.timeStamp.dwLowDateTime = 201",
            "unknown missing capability rejected",
            "wfp_evidence_controls();",
        ):
            self.assertIn(required, self.probe)
        self.assertIn("Exit 2 preserves an unresolved raw timeout", self.probe)
        self.assertNotIn("network.denied = true", self.probe)

    def test_diagnostic_only_scope(self):
        wait = self.header.split("m_received.wait_for(", 1)[1].split("});", 1)[0]
        self.assertIn("std::chrono::seconds(2)", wait)
        self.assertIn("m_records[i].accepted && m_records[i].record.time <= m_end", wait)
        self.assertIn("return false;", wait)
        self.assertNotIn("return m_count != 0", wait)
        for required in (
            "diagnostic missing SID never grants acceptance",
            "diagnostic classify drop never grants acceptance",
        ):
            self.assertIn(required, self.probe)
        for required in (
            "FwpmFilterGetById0",
            "FwpmProviderGetByKey0",
            "FwpmSubLayerGetByKey0",
            "filter->numFilterConditions && i < 16",
            "m_records[i].accepted",
            "WFP metadata read only after callback drain",
            "header.remoteAddrV4 != INADDR_LOOPBACK",
            "header.remotePort != identity.remote_port",
        ):
            self.assertIn(required, self.header)
        for forbidden in (
            "displayData.name",
            "displayData.description",
            "providerData.data",
            "conditionValue.byteBlob->data",
        ):
            self.assertNotIn(forbidden, self.header)
        self.assertLess(
            self.runner.index('f"{mode}-report.json"'),
            self.runner.index("envelope = json.loads(stdout)"),
        )
        self.assertNotIn(
            "diagnostics",
            self.runner[
                self.runner.index("def network_evidence(") : self.runner.index("def line(")
            ],
        )

    def test_read_only_options_and_bounded_stage_counters(self):
        self.assertIn(
            'event->header.localAddrV4 == INADDR_ANY ? "any"', " ".join(self.header.split())
        )
        for required in (
            "m_keywords = read_option(FWPM_ENGINE_NET_EVENT_MATCH_ANY_KEYWORDS)",
            "m_collection = read_option(FWPM_ENGINE_COLLECT_NET_EVENTS)",
            "FWPM_NET_EVENT_KEYWORD_CAPABILITY_DROP",
            "if (value < 65535)",
            "DiagnosticStage::Application",
            "DiagnosticStage::Time",
            "DiagnosticStage::Target",
            "result.local_port_present ? event->header.localPort : 0",
            "WFP counters read only after callback drain",
        ):
            self.assertIn(required, self.header)
        for required in (
            "diagnostic local ANY never grants acceptance",
            "diagnostic missing local fields never grants acceptance",
        ):
            self.assertIn(required, self.probe)
        diagnostic = self.header.split("inline DiagnosticStage diagnostic_stage", 1)[1].split(
            "struct Record", 1
        )[0]
        self.assertNotIn("FWPM_NET_EVENT_FLAG_LOCAL_ADDR_SET", diagnostic)
        self.assertNotIn("FWPM_NET_EVENT_FLAG_LOCAL_PORT_SET", diagnostic)
        self.assertIn("std::memcmp(header.appId.data", diagnostic)
        self.assertNotIn(
            "observation",
            self.runner[
                self.runner.index("def network_evidence(") : self.runner.index("def line(")
            ],
        )

    def test_runner_evidence_refusal(self):
        tree = ast.parse(self.runner)
        function = next(
            item
            for item in tree.body
            if isinstance(item, ast.FunctionDef) and item.name == "network_evidence"
        )
        namespace = {}
        exec(
            compile(ast.Module(body=[function], type_ignores=[]), "network-evidence", "exec"),
            namespace,
        )
        verify = namespace["network_evidence"]
        report = dict(
            networkOperation="WSAPoll timeout",
            networkError=10060,
            networkPolicyStatus=0,
            networkPolicyError=0,
            networkLocalPort=45670,
            networkDenied=False,
        )
        child = dict(pid=1234, creationTicks=100)
        kernel = dict(
            retainedIdentity=True,
            pid=1234,
            birth="100",
            events=[
                dict(
                    type="capability-drop",
                    localPort=45670,
                    remotePort=45671,
                    capability=0,
                    filter="123",
                )
            ],
        )
        self.assertEqual(
            verify(copy.deepcopy(report), copy.deepcopy(kernel), child, 45671, 2),
            "exact parent-observed WFP AppContainer capability drop",
        )
        for field, value in [
            ("networkOperation", "connect"),
            ("networkError", 0),
            ("networkDenied", True),
            ("networkLocalPort", 45669),
        ]:
            bad = {**report, field: value}
            with self.subTest(field=field), self.assertRaises(AssertionError):
                verify(bad, copy.deepcopy(kernel), child, 45671, 2)
        for field, value in [
            ("events", []),
            ("birth", "101"),
            ("pid", 1235),
            ("retainedIdentity", False),
        ]:
            with self.subTest(field=field), self.assertRaises(AssertionError):
                verify(copy.deepcopy(report), {**kernel, field: value}, child, 45671, 2)
        for field, value in [
            ("type", "classify-drop"),
            ("filter", "0"),
            ("capability", 3),
            ("remotePort", 45672),
            ("localPort", 45669),
        ]:
            bad = copy.deepcopy(kernel)
            bad["events"][0][field] = value
            with self.subTest(field=field), self.assertRaises(AssertionError):
                verify(copy.deepcopy(report), bad, child, 45671, 2)
        # NONE without an exact kernel event is still failure; valid native
        # capability diagnosis and direct EACCES retain their original exit 0.
        for denied in (
            {**report, "networkOperation": "connect", "networkError": 10013, "networkDenied": True},
            {**report, "networkPolicyError": 1, "networkDenied": True},
        ):
            self.assertTrue(verify(denied, {**kernel, "events": []}, child, 45671, 0))
        with self.assertRaises(AssertionError):
            verify(copy.deepcopy(report), copy.deepcopy(kernel), child, 45671, 0)


if __name__ == "__main__":
    unittest.main()
