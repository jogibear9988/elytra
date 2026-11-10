/** Shared lossless native stat projection consumed by fs.Stats and polling watchers. */
#pragma once
#include "platform/UV.h"
#include <LibGC/Root.h>
#include <LibJS/Runtime/Object.h>
#include <LibJS/Runtime/PrimitiveString.h>
#include <LibJS/Runtime/Realm.h>
#include <string>

namespace Mite {
inline GC::Ref<JS::Object> make_stat_snapshot(JS::Realm& realm, uv_stat_t const& stat)
{
    auto& vm = realm.vm();

    auto integer = [&](auto value) {
        auto bytes = std::to_string(value);

        return JS::PrimitiveString::create(
            vm, Utf16String::from_utf8(StringView(bytes.data(), bytes.size())));
    };

    GC::Root object { JS::Object::create(realm, nullptr) };
#define FIELD(name)                                                                                \
    object->define_direct_property(#name##_utf16, integer(stat.st_##name), JS::default_attributes)
    FIELD(dev);

    FIELD(mode);

    FIELD(nlink);

    FIELD(uid);

    FIELD(gid);

    FIELD(rdev);

    FIELD(blksize);

    FIELD(ino);

    FIELD(size);

    FIELD(blocks);
#undef FIELD
    auto time = [&](auto name, uv_timespec_t value) {
        GC::Root entry { JS::Object::create(realm, nullptr) };
        entry->define_direct_property(
            "seconds"_utf16, integer(value.tv_sec), JS::default_attributes);

        entry->define_direct_property(
            "nanos"_utf16, JS::Value(static_cast<double>(value.tv_nsec)), JS::default_attributes);

        object->define_direct_property(name, entry.ptr(), JS::default_attributes);
    };

    time("atime"_utf16, stat.st_atim);

    time("mtime"_utf16, stat.st_mtim);

    time("ctime"_utf16, stat.st_ctim);

    time("birthtime"_utf16, stat.st_birthtim);

    return *object;
}
}
