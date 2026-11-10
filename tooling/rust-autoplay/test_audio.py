#!/usr/bin/env python3
"""Exercise the actual pinned composer and its byte-preserving edit journal."""

import hashlib
from pathlib import Path
import unittest
from unittest.mock import patch

import audio

SOURCE = (
    Path(__file__).resolve().parents[2]
    / "vendor/ladybird/Libraries/LibWeb/Bindings/Rust/src/web_audio"
)


class JournalText(str):
    """Observe actual str.replace calls without changing their string semantics."""

    def __new__(cls, value, journal):
        instance = super().__new__(cls, value)
        instance.journal = journal
        return instance

    def replace(self, before, after, count=-1):
        self.journal.append((before, after, count))
        return JournalText(super().replace(before, after, count), self.journal)


class AudioComposition(unittest.TestCase):
    def test_exact_edits_and_independent_reversal(self):
        for name in audio.PINS:
            with self.subTest(name=name):
                original = (SOURCE / name).read_text()
                journal = []
                generated = audio.compose(name, JournalText(original, journal))
                self.assertNotEqual(generated, original)
                forward = [entry for entry in journal if entry[2] == -1]
                backward = [entry for entry in journal if entry[2] == 1]
                self.assertTrue(forward)
                self.assertEqual(
                    backward,
                    [(after, before, 1) for before, after, _ in reversed(forward)],
                )
                # Independently replay the exact edits and their inverse on
                # ordinary strings. Every byte outside those spans must survive.
                replay = original
                for before, after, _ in forward:
                    self.assertEqual(replay.count(before), 1)
                    replay = replay.replace(before, after)
                self.assertEqual(replay, generated)
                for before, after, _ in backward:
                    self.assertEqual(replay.count(before), 1)
                    replay = replay.replace(before, after, 1)
                self.assertEqual(replay.encode(), original.encode())
                self.assertTrue(generated.startswith(original[: original.index("use ")]))

    def test_pins_unknown_inputs_and_recurrence(self):
        with self.assertRaises(ValueError):
            audio.compose("unreviewed.rs", "")
        for name in audio.PINS:
            with self.subTest(name=name):
                original = (SOURCE / name).read_text()
                generated = audio.compose(name, original)
                for invalid in (original + "\n// drift", original[:-1], generated):
                    with self.assertRaisesRegex(ValueError, "Review pinned WebAudio"):
                        audio.compose(name, invalid)

    def test_missing_and_duplicate_anchors_refused_after_pin_review(self):
        # A reviewed new source pin alone must not silently admit a disappeared
        # or duplicated seam. Each mutation passes the hash guard in isolation,
        # then the real production anchor validator must refuse it.
        anchors = {
            "audio_context.rs": "    allowed_to_start: bool,",
            "audio_scheduled_source_node.rs": "        // FIXME: 5. Send a control message to the associated AudioContext to start running its rendering thread only when all the following conditions are met:",
            "audio_buffer_source_node.rs": "        // FIXME: 6. Send a control message to the associated AudioContext to start running its rendering thread only when all the following conditions are met:",
        }
        for name, anchor in anchors.items():
            original = (SOURCE / name).read_text()
            self.assertEqual(original.count(anchor), 1)
            for invalid in (
                original.replace(anchor, ""),
                original.replace(anchor, anchor + "\n" + anchor),
            ):
                with self.subTest(name=name, duplicate=invalid.count(anchor) == 2):
                    reviewed = audio.PINS | {name: hashlib.sha256(invalid.encode()).hexdigest()}
                    with patch.object(audio, "PINS", reviewed):
                        with self.assertRaisesRegex(
                            ValueError, "Missing/duplicate WebAudio anchor"
                        ):
                            audio.compose(name, invalid)


if __name__ == "__main__":
    unittest.main()
