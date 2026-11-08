/** Exercise untrusted helper metadata before allocating a source-realm process object. */
#include "PreloadProcessTest.h"
#include "native/preload/ProcessConfig.h"
#include <AK/StringBuilder.h>

ErrorOr<void> check_preload_process_configuration()
{
    using namespace Firefly::PreloadProcess;
    auto repeated = [](size_t count) {
        StringBuilder value;
        for (size_t i = 0; i < count; ++i)
            value.append('x');
        return MUST(value.to_string());
    };

    auto data = [](JsonArray arguments = {}, JsonObject environment = {}) {
        JsonObject value;
        value.set("version"sv, 1);
        value.set("arguments"sv, move(arguments));
        value.set("environment"sv, move(environment));
        return value;
    };

    auto require = [](bool condition) -> ErrorOr<void> {
        if (!condition)
            return Error::from_string_literal("Preload process configuration boundary failed");
        return {};
    };

    TRY(require(decode(data()).value().bytes == 0));
    TRY(require(decode(JsonArray {}).is_error()));
    auto wrong = data();
    wrong.set("version"sv, 2);
    TRY(require(decode(wrong).is_error()));
    wrong = data();
    wrong.set("extra"sv, true);
    TRY(require(decode(wrong).is_error()));
    JsonArray arguments;
    for (size_t i = 0; i < max_arguments; ++i)
        arguments.must_append(""sv);
    TRY(require(!decode(data(arguments)).is_error()));
    arguments.must_append(""sv);
    TRY(require(decode(data(arguments)).is_error()));
    arguments = {};
    for (size_t i = 0; i < 4; ++i)
        arguments.must_append(repeated(4096));
    TRY(require(decode(data(arguments)).value().bytes == max_argument_bytes));
    arguments.must_append("x"sv);
    TRY(require(decode(data(arguments)).is_error()));
    arguments = {};
    arguments.must_append(repeated(4097));
    TRY(require(decode(data(arguments)).is_error()));
    arguments = {};
    arguments.must_append(1);
    TRY(require(decode(data(arguments)).is_error()));
    arguments = {};
    arguments.must_append(MUST(String::from_utf8("a\0b"sv)));
    TRY(require(decode(data(arguments)).is_error()));
    JsonObject environment;
    environment.set("__proto__"sv, "ordinary-data"sv);
    environment.set("UNICODE"sv, "ä=✓"sv);
    auto valid = TRY(decode(data({}, environment)));
    TRY(require(
        TRY(decode(encode(valid))).environment.get_string("__proto__"sv) == "ordinary-data"sv));
    environment.set("BAD"sv, 1);
    TRY(require(decode(data({}, environment)).is_error()));
    environment = {};
    environment.set(""sv, "empty-key"sv);
    TRY(require(decode(data({}, environment)).is_error()));
    environment = {};
    environment.set(MUST(String::from_utf8("bad\0key"sv)), "value"sv);
    TRY(require(decode(data({}, environment)).is_error()));
    environment = {};
    environment.set("key"sv, MUST(String::from_utf8("bad\0value"sv)));
    TRY(require(decode(data({}, environment)).is_error()));
    environment = {};
    environment.set("a"sv, repeated(max_metadata_bytes - 1));
    TRY(require(decode(data({}, environment)).value().bytes == max_metadata_bytes));
    environment.set("b"sv, ""sv);
    TRY(require(decode(data({}, environment)).is_error()));
    environment = {};
    for (size_t i = 0; i < max_environment_entries; ++i)
        environment.set(String::number(i), ""sv);
    TRY(require(!decode(data({}, environment)).is_error()));
    environment.set("overflow"sv, ""sv);
    TRY(require(decode(data({}, environment)).is_error()));
    outln("PASS sandbox process metadata schema/roundtrip/argument/environment byte and count "
          "limits");
    return {};
}
