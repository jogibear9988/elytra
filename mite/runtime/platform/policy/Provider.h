/** Native policy snapshots; workers own plain metadata, never VM objects or callbacks. */
#pragma once
#include <AK/ByteString.h>
#include <AK/Error.h>
#include <AK/HashMap.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/Optional.h>
#include <AK/Variant.h>
#include <AK/Vector.h>

namespace Mite::Policy {
enum class Type { String, Number, Boolean };

struct Definition {
    ByteString name;
    Vector<Type> types;
    bool union_type {};
};

using Value = Variant<ByteString, double, bool>;
using Snapshot = HashMap<ByteString, Value>;
inline constexpr size_t definition_limit = 256;
inline constexpr size_t name_limit = 4096;
inline constexpr size_t value_limit = 64 * 1024;
inline constexpr size_t snapshot_limit = 1024 * 1024;

class Provider {
    AK_ALLOC_WITH_KMALLOC;

public:
    static ErrorOr<NonnullOwnPtr<Provider>> create(ByteString product, Vector<Definition>);

    virtual ~Provider() = default;

    // No value means no new completed snapshot, not an observed empty policy set.
    virtual ErrorOr<Optional<Snapshot>> take_snapshot() = 0;

    virtual void stop() = 0;
};
}
