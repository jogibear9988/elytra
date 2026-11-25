#!/usr/bin/env python3
"""Bind dedicated-worker fetch scope at pinned native admission and agent lifetime seams."""

import argparse
import hashlib
from pathlib import Path

# Reviewed f9acbe91: new media and compositor recovery callbacks leave worker
# admission, publication and owner teardown unchanged.
ROOT = Path(__file__).resolve().parents[1]
PINS = {
    "WorkerPageClient.cpp": (
        "Services/WebContent/PageClient.cpp",
        "f7d500b91d78549aaec774b60b8d11bd327775dad6b4d209f2f1e775ac63c469",
    ),
    "WorkerPageHost.cpp": (
        "Services/WebContent/Worker/PageHost.cpp",
        "18fddeb911cab152e613e7c0d1dd4353b6ae1b562701c738f24a82920efe17aa",
    ),
    "WorkerThread.cpp": (
        "Services/WebContent/Worker/WorkerThread.cpp",
        "3e87668c7cfa20ab4a00e761334339ab2cc4f778f09aa16d1c4d9667c86a07a3",
    ),
    "WorkerHost.cpp": (
        "Services/WebContent/Worker/WorkerHost.cpp",
        "f38c661efa46a0396fee91fbc4543025f8be77ebcbba107f9bd08d8995d89bd9",
    ),
}
PREFIX = '#include "loading/workers/Scope.h"\n'


def edits(name):
    if name in ("WorkerPageClient.cpp", "WorkerPageHost.cpp"):
        receiver = "page()" if name == "WorkerPageClient.cpp" else "page()"
        start = (
            "    auto started_thread = "
            + ("WebWorker::" if name == "WorkerPageClient.cpp" else "")
            + "start_dedicated_worker_thread(request.owner_token);\n"
        )
        publish = (
            "    client().async_register_dedicated_worker(m_id, move(request), move(started_thread.value().browser_end_of_worker_connection));"
            if name == "WorkerPageClient.cpp"
            else "    m_client.async_register_dedicated_worker(move(request), move(started_thread.value().browser_end_of_worker_connection));"
        )
        return PREFIX, [
            (
                start,
                f"    auto firefly_admission = Firefly::Workers::admit({receiver}, request);\n"
                "    if (firefly_admission.is_error())\n        return {};\n" + start,
            ),
            (publish, "    firefly_admission.value().commit();\n" + publish),
        ]
    if name == "WorkerThread.cpp":
        return PREFIX + '#include "loading/workers/Fetch.h"\n', [
            (
                "    Core::EventLoop event_loop;\n    Web::Bindings::initialize_agent(agent_type);",
                "    Core::EventLoop event_loop;\n"
                "    Firefly::Workers::AgentScope firefly_scope(dedicated_worker_thread_link ? dedicated_worker_thread_link->owner_token() : 0);\n"
                "    Web::Bindings::initialize_agent(agent_type);\n"
                "    Firefly::WorkersFetch::enter();",
            ),
            (
                "    client->shut_down_agent();",
                "    Firefly::WorkersFetch::leave();\n    client->shut_down_agent();",
            ),
        ]
    if name == "WorkerHost.cpp":
        return PREFIX, [
            (
                "void WorkerHost::terminate()\n{\n",
                "void WorkerHost::terminate()\n{\n    Firefly::Workers::retire_current();\n",
            ),
            (
                "void WorkerHost::run_closing_steps()\n{\n",
                "void WorkerHost::run_closing_steps()\n{\n    Firefly::Workers::retire_current();\n",
            ),
            (
                "    // 12. Obtain script by switching on the value of options's type member:\n",
                "    if (!Firefly::Workers::bind_environment(inside_settings->id)) {\n"
                "        on_complete->function()(nullptr);\n        return;\n    }\n\n"
                "    // 12. Obtain script by switching on the value of options's type member:\n",
            ),
        ]
    raise ValueError("Unknown worker source")


def adapt(name, original):
    prefix, changes = edits(name)
    result = original
    for before, after in changes:
        if result.count(before) != 1 or after in result:
            raise ValueError(f"Missing, repeated or previously composed worker seam: {name}")
        result = result.replace(before, after, 1)
    return prefix + result


def reverse(name, composed):
    prefix, changes = edits(name)
    if not composed.startswith(prefix):
        raise ValueError("Missing worker composition prefix")
    original = composed.removeprefix(prefix)
    for before, after in reversed(changes):
        if original.count(after) != 1:
            raise ValueError("Missing or repeated worker journal entry")
        original = original.replace(after, before, 1)
    return original


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    planned = {}
    for name, (relative, digest) in PINS.items():
        raw = (ROOT / "vendor/ladybird" / relative).read_bytes()
        if hashlib.sha256(raw).hexdigest() != digest:
            raise SystemExit(f"Review pinned worker lifecycle source: {relative}")
        original = raw.decode()
        composed = adapt(name, original)
        assert reverse(name, composed) == original
        prefix, changes = edits(name)
        for before, _after in changes:
            for bad in (original.replace(before, "", 1), original + before, composed):
                try:
                    adapt(name, bad)
                except ValueError:
                    pass
                else:
                    raise AssertionError(f"Worker drift/duplicate/recurrence accepted: {name}")
        planned[name] = composed
    args.output.mkdir(parents=True, exist_ok=True)
    for name, composed in planned.items():
        path = args.output / name
        if not path.exists() or path.read_text() != composed:
            path.write_text(composed)
    print("PASS 4 pinned worker lifecycle sources: reversal, missing/duplicate/recurrence refusal")


if __name__ == "__main__":
    main()
