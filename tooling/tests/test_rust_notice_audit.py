#!/usr/bin/env python3
"""Ensure notice audits keep renamed, build/dev, cyclic and feature dependencies."""

import _support  # Shared build-helper import path for standalone test CLIs.

import unittest

from audit_rust_notices import external_packages


class NoticeAudit(unittest.TestCase):
    def fixture(self):
        return {
            "packages": [
                {
                    "id": "opaque root",
                    "name": "root",
                    "version": "1",
                    "source": None,
                    "features": {"optional": []},
                },
                {
                    "id": "opaque build",
                    "name": "build",
                    "version": "2",
                    "source": "registry+example",
                    "features": {},
                },
                {
                    "id": "opaque dev",
                    "name": "renamed",
                    "version": "3",
                    "source": "registry+example",
                    "features": {},
                },
                {
                    "id": "unused",
                    "name": "unused",
                    "version": "4",
                    "source": "registry+example",
                    "features": {},
                },
            ],
            "workspace_members": ["opaque root"],
            "resolve": {
                "nodes": [
                    {
                        "id": "opaque root",
                        "features": ["optional"],
                        "dependencies": ["opaque build", "opaque dev"],
                        "deps": [
                            {
                                "name": "build",
                                "pkg": "opaque build",
                                "dep_kinds": [{"kind": "build"}],
                            },
                            {"name": "alias", "pkg": "opaque dev", "dep_kinds": [{"kind": "dev"}]},
                        ],
                    },
                    {"id": "opaque build", "features": [], "dependencies": ["opaque root"]},
                    {"id": "opaque dev", "features": [], "dependencies": []},
                ]
            },
        }

    def test_keeps_all_kinds_without_parsing_ids(self):
        selected = external_packages(self.fixture(), True)
        self.assertEqual({key[0] for key in selected}, {"build", "renamed"})

    def test_missing_node_rejects_incomplete_audit(self):
        metadata = self.fixture()
        metadata["resolve"]["nodes"].pop()
        with self.assertRaisesRegex(ValueError, "Incomplete"):
            external_packages(metadata, False)

    def test_incomplete_feature_expansion_rejects_audit(self):
        metadata = self.fixture()
        metadata["resolve"]["nodes"][0]["features"] = []
        with self.assertRaisesRegex(ValueError, "all workspace features"):
            external_packages(metadata, True)


if __name__ == "__main__":
    unittest.main()
