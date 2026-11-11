/** Application paths are selected before native profile/session initialization. */
#pragma once
#include "../../../mite/runtime/core/NativeCaptureLifetime.h"
#include <QString>
#include <optional>

namespace Firefly::AppPaths {
struct Failure {
    char const* code;
    char const* message;
};

class Paths : public Mite::NativeCaptureOwner<Paths> {
public:
    ~Paths()
    {
        this->revoke_native_captures();
    }

    template <typename Visitor> void visit_capture_edges(Visitor&) const
    {
        // Only native policy/Qt data; callbacks are native transport hooks.
    }

    explicit Paths(QString application_directory);

    std::optional<Failure> get(QString const& name, QString& result);

    std::optional<Failure> set(QString const& name, QString const& path);

    std::optional<Failure> set_logs(std::optional<QString> const&);

    std::optional<Failure> seal();

    QString session_root() const;

    QString const& application_directory() const
    {
        return m_application_directory;
    }

private:
    static std::optional<Failure> ensure_directory(QString const&);

    static std::optional<Failure> validate(QString const&);

    QString user_data() const;

    QString m_application_directory, m_app_data, m_identity;
    std::optional<QString> m_user_data, m_session_data, m_logs;
    bool m_sealed { false };
};
}
