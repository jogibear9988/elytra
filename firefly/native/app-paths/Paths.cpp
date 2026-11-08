/** Use native standard paths and real directories; never redirect an already-open browser profile.
 */
#include "Paths.h"
#include <LibCore/Directory.h>
#include <LibCore/StandardPaths.h>
#include <LibCore/System.h>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace Firefly::AppPaths {
namespace {
    QString text(ByteString const& value)
    {
        return QString::fromUtf8(value.characters(), value.length());
    }
}

Paths::Paths(QString directory)
    : m_application_directory(QDir::cleanPath(std::move(directory)))
{
#ifdef Q_OS_MACOS
    m_app_data = text(Core::StandardPaths::user_data_directory());
#else
    m_app_data = text(Core::StandardPaths::config_directory());
#endif
    QString identity = m_application_directory;
    QString label = QFileInfo(m_application_directory).fileName();
    QFile package(m_application_directory + "/package.json");
    if (package.open(QIODevice::ReadOnly) && package.size() <= 65536) {
        auto name = QJsonDocument::fromJson(package.readAll()).object().value("name").toString();
        if (!name.isEmpty() && name.size() <= 256) {
            identity = "package:" + name;
            label = name;
        }
    }

    label.replace(QRegularExpression("[^A-Za-z0-9._-]"), "_");
    label = label.left(48);
    if (label.isEmpty() || label == "." || label == "..")
        label = "application";
    m_identity = label + "-"
        + QString::fromLatin1(
            QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha256)
                .toHex()
                .left(16));
}

QString Paths::user_data() const
{
    return m_user_data.value_or(m_app_data + "/Firefly/" + m_identity);
}

QString Paths::session_root() const
{
    return m_session_data.value_or(user_data());
}

std::optional<Failure> Paths::validate(QString const& path)
{
    if (path.isEmpty() || path.contains(QChar(0)) || path.toUtf8().size() > 32768
        || !QDir::isAbsolutePath(path))
        return Failure { "ERR_INVALID_ARG_VALUE",
            "Application paths must be absolute bounded native paths without NUL" };

    return {};
}

std::optional<Failure> Paths::ensure_directory(QString const& path)
{
    if (auto error = validate(path); error.has_value())
        return error;
    auto bytes = path.toUtf8();
    if (auto result = Core::Directory::create(
            StringView(bytes.data(), bytes.size()), Core::Directory::CreateDirectories::Yes, 0700);
        result.is_error())
        return Failure { "ERR_FIREFLY_PATH_FAILED", "Cannot create application directory" };
    return {};
}

std::optional<Failure> Paths::get(QString const& name, QString& result)
{
    if (name == "userData" || name == "sessionData" || name == "logs") {
        result = name == "userData" ? user_data()
            : name == "sessionData" ? session_root()
                                    : m_logs.value_or(user_data() + "/logs");
        return ensure_directory(result);
    }

    if (name == "appData")
        result = m_app_data;
    else if (name == "home")
        result = text(Core::StandardPaths::home_directory());
    else if (name == "temp")
        result = text(Core::StandardPaths::tempfile_directory());
    else if (name == "exe") {
        auto executable = Core::System::current_executable_path();
        if (executable.is_error())
            return Failure { "ERR_FIREFLY_PATH_FAILED", "Cannot resolve executable path" };
        result = text(executable.value());
    } else
        return Failure { "ERR_FIREFLY_UNSUPPORTED_API", "Unsupported application path name" };
    return {};
}

std::optional<Failure> Paths::set(QString const& name, QString const& path)
{
    if (name != "userData" && name != "sessionData" && name != "logs")
        return Failure { "ERR_FIREFLY_UNSUPPORTED_API",
            "Unsupported mutable application path name" };

    if (m_sealed && name != "logs")
        return Failure { "ERR_FIREFLY_PATHS_TOO_LATE",
            "Profile paths must be selected before native readiness" };

    if (auto error = validate(path); error.has_value())
        return error;
    if (!QFileInfo(path).isDir())
        return Failure { "ERR_INVALID_ARG_VALUE", "setPath requires an existing directory" };
    auto selected = QDir::cleanPath(path);
    if (name == "userData")
        m_user_data = selected;
    else if (name == "sessionData")
        m_session_data = selected;
    else
        m_logs = selected;
    return {};
}

std::optional<Failure> Paths::set_logs(std::optional<QString> const& path)
{
    auto selected = path.value_or(user_data() + "/logs");
    if (auto error = ensure_directory(selected); error.has_value())
        return error;
    m_logs = QDir::cleanPath(selected);
    return {};
}

std::optional<Failure> Paths::seal()
{
    if (auto error = ensure_directory(user_data()); error.has_value())
        return error;
    if (auto error = ensure_directory(session_root()); error.has_value())
        return error;
    m_sealed = true;
    return {};
}
}
