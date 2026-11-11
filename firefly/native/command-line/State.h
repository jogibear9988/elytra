/** Native argv snapshot and transactional supported-engine switches, independent of Qt/LibWeb. */
#pragma once
#include "../../../mite/runtime/core/NativeCaptureLifetime.h"
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace Firefly::CommandLine {
struct Failure {
    std::string code;
    std::string message;
};

class State : public Mite::NativeCaptureOwner<State> {
public:
    ~State()
    {
        this->revoke_native_captures();
    }

    template <typename Visitor> void visit_capture_edges(Visitor&) const
    {
        // Only native policy/Qt data; callbacks are native transport hooks.
    }

    // Native engine-policy adapter only; never a JS callback container.
    using Apply = std::function<std::optional<Failure>(
        std::string_view, std::optional<std::string> const&, bool initial)>;
    State(int argc, char** argv, Apply);

    std::optional<Failure> const& error() const
    {
        return m_error;
    }

    std::string value(std::string_view) const;

    bool has(std::string_view) const;

    std::optional<Failure> change(std::string_view, std::optional<std::string>);

    std::optional<Failure> apply_initial();

    void seal()
    {
        m_sealed = true;
    }

    static bool valid_name(std::string_view);

private:
    static bool supported(std::string_view);

    bool within_budget() const;

    std::map<std::string, std::string, std::less<>> m_switches;
    Apply m_apply;
    std::optional<Failure> m_error;
    bool m_sealed { false };
};
}
