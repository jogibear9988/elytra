/** Owned adapter to existing public Ladybird settings/options; no upstream patch. */
#pragma once
#include "State.h"
#include <AK/String.h>
#include <AK/Vector.h>

namespace Firefly::CommandLine {
class EnginePolicy {
public:
    explicit EnginePolicy(bool startup_cpu);

    void activate();

    String locale() const;

    std::optional<Failure> apply(std::string_view, std::optional<std::string> const&, bool initial);

private:
    Vector<String> m_languages;
    Optional<String> m_requested_language;
    bool m_active { false };
    bool m_startup_cpu;
};
}
