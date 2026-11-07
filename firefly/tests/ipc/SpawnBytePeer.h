/** Dedicated test-child bootstrap; never part of the product process-launch policy. */
#pragma once
#include <AK/NonnullOwnPtr.h>
#include <AK/OwnPtr.h>
#include <LibCore/Process.h>
#include <LibIPC/Transport.h>

struct BytePeerResources {
    AK_ALLOC_WITH_KMALLOC;
    virtual ~BytePeerResources() = default;
};

struct SpawnedBytePeer {
    AK_ALLOC_WITH_KMALLOC;
    Core::Process process;
    NonnullOwnPtr<IPC::Transport> transport;
    OwnPtr<BytePeerResources> resources;
    bool reaped { false };

    SpawnedBytePeer(
        Core::Process child, NonnullOwnPtr<IPC::Transport> channel, OwnPtr<BytePeerResources> owner)
        : process(move(child))
        , transport(move(channel))
        , resources(move(owner))
    {
    }

    ~SpawnedBytePeer();

    ErrorOr<int> wait();
};

ErrorOr<NonnullOwnPtr<SpawnedBytePeer>> spawn_byte_peer(
    ByteString const& executable, ReadonlySpan<ByteString> extra_arguments = {});
