/** Owned adapter to existing public Ladybird settings/options; no upstream patch. */
#pragma once
#include "State.h"
#include <AK/String.h>
#include <AK/Vector.h>

namespace Firefly::CommandLine {
class EnginePolicy {
public:
    EnginePolicy();

    std::optional<Failure> apply(std::string_view, std::optional<std::string> const&, bool initial);

private:
    Vector<String> m_languages;
};
}
