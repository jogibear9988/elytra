#!/usr/bin/env python3
"""Ensure the owned lock cannot silently introduce different upstream packages."""
from pathlib import Path
import tempfile
import unittest

from check_mite_rust_lock import check


class LockGuard(unittest.TestCase):
    def test_inventory_and_drift(self):
        dependency = ('[[package]]\nname = "dependency"\nversion = "1.0.0"\n'
                      'source = "registry+https://example.invalid"\nchecksum = "abc"\n')
        own = '[[package]]\nname = "mite_runtime_rust"\nversion = "0.1.0"\n'
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            upstream = root / 'vendor/ladybird/Cargo.lock'
            aggregate = root / 'mite/rust/Cargo.lock'
            upstream.parent.mkdir(parents=True)
            aggregate.parent.mkdir(parents=True)
            upstream.write_text(dependency)
            aggregate.write_text(dependency + own)
            check(root)
            for before, after in [('1.0.0', '1.0.1'), ('abc', 'xyz'),
                                  ('example.invalid', 'different.invalid'),
                                  ('dependency', 'new-dependency')]:
                with self.subTest(after=after):
                    aggregate.write_text(dependency.replace(before, after) + own)
                    with self.assertRaisesRegex(ValueError, 'drift'):
                        check(root)
            aggregate.write_text(dependency)
            with self.assertRaisesRegex(ValueError, 'Missing'):
                check(root)


if __name__ == '__main__':
    unittest.main()
