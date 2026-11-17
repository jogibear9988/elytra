#!/usr/bin/env python3
"""Compose exact SDK broker input, remote-admission and disconnect boundaries.

The generated full header is selected before every SDK consumer. Added methods
are nonvirtual and add no fields; no vendor file or standalone Mite API changes.
"""

import argparse
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PINS = {
    "LibIPC/HandleBroker.h": "6fcf295551aff8acef7137b46ec11355b7beb3d511227d2f1159933ef0b69128",
    "HandleBroker.cpp": "1f7671d38bb3fa46f71c9ae5e3cc383785eff9c840f2138aa4d7de08b1a2b8e1",
    "HandleBrokerServer.cpp": "d9534bdbd4b09050b54aea46ca71140778b28e65d3e414b1c8fd8f68a9198051",
}
PREFIX = '#include "native/sandbox/windows/helper/BrokerPolicy.h"\n'

METHODS = """bool LocalHandleBroker::can_park_on_behalf_of(pid_t owner, size_t count)
{
    MutexLocker locker(m_mutex);
    return Firefly::WindowsBroker::can_park(m_parked_attachments, Core::System::getpid(), owner, count);
}

void LocalHandleBroker::forget_owner(pid_t owner)
{
    MutexLocker locker(m_mutex);
    m_parked_attachments.remove_all_matching([&](auto const&, auto const& record) {
        return record.owner == owner;
    });
}

"""
COPY_IN = """    ErrorOr<Vector<u64>> copy_handles_into_child(Vector<Attachment> const& attachments)
    {
        // Validate the entire native source batch before making any remote copy.
        for (auto const& attachment : attachments) {
            if (attachment.fd() <= 0)
                return Error::from_errno(EBADF);
        }
        Vector<u64> handle_values;
        Vector<HANDLE> copies;
        TRY(handle_values.try_ensure_capacity(attachments.size()));
        TRY(copies.try_ensure_capacity(attachments.size()));
        ArmedScopeGuard rollback { [&] {
            for (auto handle : copies) {
                // Keep the actual remote HANDLE, never its narrowed descriptor.
                // DUPLICATE_CLOSE_SOURCE closes it even when duplication fails.
                (void)DuplicateHandle(m_process_handle, handle, nullptr, nullptr, 0, FALSE, DUPLICATE_CLOSE_SOURCE);
            }
        } };
        for (auto const& attachment : attachments) {
            HANDLE copied_handle = nullptr;
            if (!DuplicateHandle(GetCurrentProcess(), to_handle(attachment.fd()), m_process_handle, &copied_handle, 0, FALSE, DUPLICATE_SAME_ACCESS))
                return Error::from_windows_error();
            copies.unchecked_append(copied_handle);
            if (!Firefly::WindowsBroker::valid_handle_value(reinterpret_cast<uintptr_t>(copied_handle)))
                return Error::from_errno(EBADF);
            handle_values.unchecked_append(static_cast<u64>(to_fd(copied_handle)));
        }
        rollback.disarm();
        return handle_values;
    }

"""


def changes(name, source):
    if name == "LibIPC/HandleBroker.h":
        seam = "    void forget_channel_of(pid_t owner, HandleBrokerChannel);\n"
        return [
            (
                seam,
                seam
                + "\n    bool can_park_on_behalf_of(pid_t owner, size_t count);\n    void forget_owner(pid_t owner);\n",
            )
        ]
    if name == "HandleBroker.cpp":
        seam = "    MutexLocker locker(m_mutex);\n\n    // Whoever holds a token may redeem it, so it must not be guessable. Zero stands for no token."
        return [
            (
                seam,
                "    MutexLocker locker(m_mutex);\n    if (!Firefly::WindowsBroker::can_park(m_parked_attachments, Core::System::getpid(), owner, attachments.size()))\n        return Error::from_errno(ENOBUFS);\n\n    // Whoever holds a token may redeem it, so it must not be guessable. Zero stands for no token.",
            ),
            (
                "void LocalHandleBroker::forget_channel(HandleBrokerChannel channel)\n",
                METHODS + "void LocalHandleBroker::forget_channel(HandleBrokerChannel channel)\n",
            ),
        ]
    if name == "HandleBrokerServer.cpp":
        begin = source.index("    ErrorOr<Vector<u64>> copy_handles_into_child(")
        end = source.index("    ErrorOr<void> respond(", begin)
        return [
            (
                "    ~HandleBrokerChildConnection()\n    {\n        CloseHandle(m_process_handle);",
                "    ~HandleBrokerChildConnection()\n    {\n        m_server.m_broker.forget_owner(m_pid);\n        CloseHandle(m_process_handle);",
            ),
            (
                "                auto attachments = TRY(copy_handles_out_of_child(request.handle_values));",
                "                if (!m_server.m_broker.can_park_on_behalf_of(m_pid, request.handle_values.size()))\n                    return Error::from_errno(ENOBUFS);\n                auto attachments = TRY(copy_handles_out_of_child(request.handle_values));",
            ),
            (
                "    ErrorOr<Vector<Attachment>> copy_handles_out_of_child(Vector<u64> const& handle_values)\n    {\n",
                "    ErrorOr<Vector<Attachment>> copy_handles_out_of_child(Vector<u64> const& handle_values)\n    {\n        for (auto value : handle_values) {\n            if (!Firefly::WindowsBroker::valid_handle_value(value))\n                return Error::from_errno(EBADF);\n        }\n",
            ),
            (
                "            attachments.unchecked_append(Attachment::from_fd(to_fd(copied_handle)));",
                "            if (!Firefly::WindowsBroker::valid_handle_value(reinterpret_cast<uintptr_t>(copied_handle))) {\n                CloseHandle(copied_handle);\n                return Error::from_errno(EBADF);\n            }\n            attachments.unchecked_append(Attachment::from_fd(to_fd(copied_handle)));",
            ),
            (source[begin:end], COPY_IN),
        ]
    raise ValueError("Unknown broker source")


def compose(name, source):
    if hashlib.sha256(source.encode()).hexdigest() != PINS[name]:
        raise ValueError("Review pinned Windows broker source: " + name)
    replacements = changes(name, source)
    output = source
    for before, after in replacements:
        if output.count(before) != 1 or after in output:
            raise ValueError("Missing/repeated Windows broker seam: " + name)
        output = output.replace(before, after, 1)
    restored = output
    for before, after in reversed(replacements):
        if restored.count(after) != 1:
            raise ValueError("Ambiguous Windows broker reversal: " + name)
        restored = restored.replace(after, before, 1)
    if restored != source:
        raise ValueError("Windows broker changed unrelated upstream bytes")
    return (
        ""
        if name.endswith(".h")
        else PREFIX + ("#include <AK/ScopeGuard.h>\n" if name == "HandleBrokerServer.cpp" else "")
    ) + output


def prepare(output):
    planned = {}
    for name in PINS:
        source = ROOT / "vendor/ladybird/Libraries/LibIPC" / Path(name).name
        planned[name] = compose(name, source.read_text()).encode()
    output.mkdir(parents=True, exist_ok=True)
    for name, data in planned.items():
        path = output / name
        path.parent.mkdir(parents=True, exist_ok=True)
        if not path.is_file() or path.read_bytes() != data:
            path.write_bytes(data)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    prepare(parser.parse_args().output)
    print("PASS exact Windows broker input/admission/cleanup source composition")
