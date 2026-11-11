#!/usr/bin/env python3
"""Pin/reversal/seam controls for native port observation; no DOM/runtime acceptance claim."""

import importlib.util
from pathlib import Path
import hashlib
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
spec = importlib.util.spec_from_file_location("_owned_port_composition", HERE / "observer.py")
policy = importlib.util.module_from_spec(spec)
spec.loader.exec_module(policy)
SOURCE = ROOT / "vendor/ladybird/Libraries/LibWeb/Bindings/Rust/src"


def reject(action):
    try:
        action()
    except ValueError:
        return
    raise AssertionError("Unreviewed port composition accepted")


def main():
    name = "html/message_port.rs"
    original = (SOURCE / name).read_text()
    generated = policy.compose(name, original)
    reject(lambda: policy.compose(name, original + "\n"))
    reject(lambda: policy.compose(name, generated))
    for before, _after in policy.replacements():
        for changed in [
            original.replace(before, "// missing owned seam", 1),
            original + "\n" + before,
        ]:
            expected = policy.PINS[name]
            try:
                policy.PINS[name] = hashlib.sha256(changed.encode()).hexdigest()
                reject(lambda: policy.compose(name, changed))
            finally:
                policy.PINS[name] = expected
    # No closure/JS export is involved; normal disentangle remains separate from
    # actual close/finalizer observation, and transfer clears the old owner.
    assert generated.count("fn firefly_message_port_observe(") == 1
    assert generated.count("fn firefly_observer_closing(") == 1
    assert generated.count("fn firefly_observer_finish_peer_close(") == 1
    assert "data_holder.write(&this.firefly_observer_id.replace(0));" in generated
    assert "value.realm.as_ptr().cast()" in generated
    assert "value.firefly_observer_id.set(firefly_id)" in generated
    assert (
        "this.firefly_observer_dequeued();\n                Self::post_message_task_steps"
        in generated
    )
    with tempfile.TemporaryDirectory(prefix="elytra-rust-port-") as directory:
        path = Path(directory) / "message_port.rs"
        path.write_text(generated)
        # Parse the composed actual Rust without modifying upstream/layout bytes.
        subprocess.run(
            ["rustfmt", "--edition", "2024", "--emit", "stdout", str(path)],
            stdout=subprocess.DEVNULL,
            check=True,
        )
    stubs = policy.compose("ffi_test_stubs.rs", (SOURCE / "ffi_test_stubs.rs").read_text())
    assert "fn firefly_port_observer_bind" in stubs and "fn firefly_port_observer_notify" in stubs
    print(
        "PASS native port observer pinned reversal, missing/duplicate seam guards and actual Rust parse"
    )


if __name__ == "__main__":
    main()
