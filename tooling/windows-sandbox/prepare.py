#!/usr/bin/env python3
"""Compose private AppContainer helper authority into exact Windows process-member seams."""

import argparse
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PINS = {
    "ProcessWindows.cpp": (
        "Libraries/LibCore/ProcessWindows.cpp",
        "86e3a91367ccdc5c2dfeeb4ac4b56faf6715f33488e2c4b0586ada2a0a57a824",
    ),
    "Process.cpp": (
        "Libraries/LibWebView/Process.cpp",
        "b9b4d6662e8ff2c2440beb3f91ec3bc6af8e6fa1dbfcc3ff61fb2102190b7545",
    ),
    "SystemProbeClient.cpp": (
        "Libraries/LibWebView/SystemProbeClient.cpp",
        "d539b4d8b86771d98681c824e081e50c8669ed8f921751f51a0f214ff4af2d9a",
    ),
    "SystemProbeClient.h": (
        "Libraries/LibWebView/SystemProbeClient.h",
        "8ddda6efffe6e7d1bec25b21f3ba24ddd5dc2bcf9adc11a94cc3991eca78ec5e",
    ),
    "SystemProbeMain.cpp": (
        "Services/SystemProbe/main.cpp",
        "914e604deddbe25682426e968a4b065f789709397eb9d05fa60b23476ba3dcc9",
    ),
}
PREFIX = '#include "native/sandbox/windows/helper/SDK.h"\n'
HEADER_PREFIX = "// Firefly retains the native SystemProbe lease until the client retires.\n"


def prefix_for(name):
    return HEADER_PREFIX if name == "SystemProbeClient.h" else PREFIX


def changes(name):
    if name == "ProcessWindows.cpp":
        return [
            (
                "ErrorOr<Process> Process::spawn(ProcessSpawnOptions const& options)\n{\n",
                "ErrorOr<Process> Process::spawn(ProcessSpawnOptions const& options)\n{\n"
                "    if (Firefly::WindowsHelpers::has_launch_scope())\n"
                "        return Process { TRY(Firefly::WindowsHelpers::spawn(options)) };\n",
            ),
            (
                "Process::~Process()\n{\n    if (m_handle)\n",
                "Process::~Process()\n{\n"
                "    Firefly::WindowsHelpers::release(m_handle);\n"
                "    if (m_handle)\n",
            ),
            (
                "Process& Process::operator=(Process&& other)\n{\n",
                "Process& Process::operator=(Process&& other)\n{\n"
                "    if (this != &other && m_handle && Firefly::WindowsHelpers::release(m_handle))\n"
                "        CloseHandle(m_handle);\n",
            ),
        ]
    if name == "Process.cpp":
        return [
            (
                "    Array<int, 2> stdout_pipe {};\n    Array<int, 2> stderr_pipe {};",
                "    Array<int, 2> stdout_pipe { -1, -1 };\n    Array<int, 2> stderr_pipe { -1, -1 };\n"
                "    ArmedScopeGuard firefly_output_cleanup { [&] {\n"
                "        for (auto fd : { stdout_pipe[0], stdout_pipe[1], stderr_pipe[0], stderr_pipe[1] })\n"
                "            if (fd != -1)\n                (void)Core::System::close(fd);\n"
                "    } };",
            ),
            (
                "        MUST(Core::System::close(stdout_pipe[1]));\n        MUST(Core::System::close(stderr_pipe[1]));",
                "        MUST(Core::System::close(stdout_pipe[1]));\n        stdout_pipe[1] = -1;\n"
                "        MUST(Core::System::close(stderr_pipe[1]));\n        stderr_pipe[1] = -1;",
            ),
            (
                "        output_capture.stdout_file = TRY(Core::File::adopt_fd(stdout_pipe[0], Core::File::OpenMode::Read));\n"
                "        output_capture.stderr_file = TRY(Core::File::adopt_fd(stderr_pipe[0], Core::File::OpenMode::Read));",
                "        output_capture.stdout_file = TRY(Core::File::adopt_fd(stdout_pipe[0], Core::File::OpenMode::Read));\n"
                "        stdout_pipe[0] = -1;\n"
                "        output_capture.stderr_file = TRY(Core::File::adopt_fd(stderr_pipe[0], Core::File::OpenMode::Read));\n"
                "        stderr_pipe[0] = -1;",
            ),
            (
                "#if defined(AK_OS_MACOS) || defined(AK_OS_LINUX)\n    spawn_options.environment = Process::helper_process_environment(type);",
                "#if defined(AK_OS_MACOS) || defined(AK_OS_LINUX) || defined(AK_OS_WINDOWS)\n    spawn_options.environment = Process::helper_process_environment(type);",
            ),
            (
                '    TRY(Core::Environment::set("SOCKET_TAKEOVER"sv, ByteString::formatted("ipc:{};handle-broker:{}", channel[1], broker_channel[1]), Core::Environment::Overwrite::Yes));',
                '    spawn_options.environment->append(ByteString::formatted("SOCKET_TAKEOVER=ipc:{};handle-broker:{}", channel[1], broker_channel[1]));',
            ),
            (
                "    process = TRY(Core::Process::spawn(spawn_options));\n\n    auto transport = TRY(IPC::Transport::adopt_fd(channel[0]));",
                "    using SandboxRole = Firefly::WindowsHelpers::Role;\n"
                "    auto role = SandboxRole::Unsupported;\n"
                "    switch (type) {\n"
                "    case ProcessType::WebContent: role = SandboxRole::WebContent; break;\n"
                "    case ProcessType::ImageDecoder: role = SandboxRole::ImageDecoder; break;\n"
                "    case ProcessType::Compositor: role = SandboxRole::Compositor; break;\n"
                "    case ProcessType::RequestServer: role = SandboxRole::RequestServer; break;\n"
                "    case ProcessType::AudioServer: role = SandboxRole::AudioServer; break;\n"
                "    case ProcessType::MediaServer: role = SandboxRole::MediaServer; break;\n"
                "    case ProcessType::VideoDecoderServer: role = SandboxRole::VideoDecoderServer; break;\n"
                "    default: break;\n"
                "    }\n"
                "    Firefly::WindowsHelpers::Scope firefly_launch_scope { role, spawn_options };\n"
                "    process = TRY(Core::Process::spawn(spawn_options));\n\n    auto transport = TRY(IPC::Transport::adopt_fd(channel[0]));",
            ),
            (
                "    auto process = TRY(Core::Process::spawn(spawn_options));\n#if defined(AK_OS_MACOS)",
                "#if defined(AK_OS_WINDOWS)\n"
                "    // No transport-free helper role has a reviewed Windows sandbox policy.\n"
                "    Firefly::WindowsHelpers::Scope firefly_launch_scope { Firefly::WindowsHelpers::Role::Unsupported, spawn_options };\n"
                "#endif\n"
                "    auto process = TRY(Core::Process::spawn(spawn_options));\n#if defined(AK_OS_MACOS)",
            ),
        ]
    if name == "SystemProbeClient.h":
        return [("    pid_t m_pid { -1 };", "    OwnPtr<Core::Process> m_process;")]
    if name == "SystemProbeClient.cpp":
        return [
            (
                "        // The probe loads the driver as the Compositor will, so it sees the same environment.\n"
                "        return Process::spawn_without_transport(ProcessType::Compositor, options);",
                "        options.die_with_parent = true;\n"
                "        options.environment = Vector<ByteString> {};\n"
                "        Firefly::WindowsHelpers::Scope scope { Firefly::WindowsHelpers::Role::SystemProbe, options };\n"
                "        return Core::Process::spawn(options);",
            ),
            (
                "    client.m_pid = process.value().pid();",
                "    client.m_process = make<Core::Process>(process.release_value());",
            ),
            (
                "    if (m_pid > 0)\n"
                "        (void)Core::Process::terminate_process(m_pid, Core::Process::TerminationMode::Forceful);",
                "    // Closing this exact owner ends only the probe's own Job.\n"
                "    m_process = nullptr;",
            ),
            (
                "    // The ProcessMonitor reaps the probe along with every other child.\n"
                "    m_pid = -1;\n",
                "    // The probe closes its output before it unloads the driver.\n"
                "    // Keep the native owner until client destruction instead of terminating here.\n",
            ),
        ]
    if name == "SystemProbeMain.cpp":
        return [
            (
                "    StringView gpu_device;\n",
                "    TRY(Firefly::WindowsHelpers::verify_current());\n\n"
                "    StringView gpu_device;\n",
            ),
        ]
    raise ValueError("Unknown Windows helper source")


def reverse(name, result):
    prefix = prefix_for(name)
    if not result.startswith(prefix) or result.count(prefix) != 1:
        raise ValueError("Missing/repeated helper authority include")
    original = result.removeprefix(prefix)
    for before, after in reversed(changes(name)):
        if original.count(after) != 1:
            raise ValueError("Missing/repeated helper authority reversal")
        original = original.replace(after, before, 1)
    return original


def compose(name, source):
    prefix = prefix_for(name)
    if prefix in source:
        raise ValueError("Repeated helper authority composition")
    result = source
    for before, after in changes(name):
        if result.count(before) != 1 or after in result:
            raise ValueError("Missing/repeated Windows helper source seam")
        result = result.replace(before, after, 1)
    result = prefix + result
    if reverse(name, result) != source:
        raise ValueError("Windows helper composition changed unrelated upstream bytes")
    return result


def prepare(output):
    planned = {}
    for name, (relative, expected) in PINS.items():
        raw = (ROOT / "vendor/ladybird" / relative).read_bytes()
        if hashlib.sha256(raw).hexdigest() != expected:
            raise ValueError("Review Windows helper source after upstream update: " + relative)
        source = raw.decode()
        result = compose(name, source)
        for before, _ in changes(name):
            for invalid in (source.replace(before, "", 1), source + before, result):
                try:
                    compose(name, invalid)
                except ValueError:
                    pass
                else:
                    raise AssertionError("Malformed helper authority source accepted")
        planned[name] = result.encode()
    output.mkdir(parents=True, exist_ok=True)
    for name, data in planned.items():
        path = output / (
            "LibWebView/SystemProbeClient.h" if name == "SystemProbeClient.h" else name
        )
        path.parent.mkdir(parents=True, exist_ok=True)
        if not path.exists() or path.read_bytes() != data:
            path.write_bytes(data)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    prepare(args.output)
    print("PASS Windows helper member/source pins, exact reversal and seam refusal")
