/** Real native path selection/directories, aliasing, rollback and lifecycle; no GUI. */
#include "native/app-paths/Paths.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <cstdlib>
#include <iostream>
using Firefly::AppPaths::Paths;

namespace {
void check(bool condition, char const* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    check(directory.isValid(), "temporary test root");
    auto root = directory.path();
    QDir().mkpath(root + "/app");
    QDir().mkpath(root + "/data ünicode");
    QDir().mkpath(root + "/other");
    Paths paths(root + "/app");
    check(!paths.set("userData", root + "/data ünicode"), "userdata selection");
    QString value;
    check(!paths.get("sessionData", value) && value == root + "/data ünicode",
        "session default did not follow userdata");
    check(paths.set("userData", root + "/missing").has_value(), "nonexistent override accepted");
    check(!paths.get("userData", value) && value == root + "/data ünicode",
        "failed setter consumed previous value");
    check(paths.set("userData", "relative").has_value(), "relative override accepted");
    check(paths.set("userData", root + "/" + QString(32769, QChar('x'))).has_value(),
        "native path budget ignored");
    check(paths.set("sessionData", root + QChar(0) + "hidden").has_value(), "NUL path accepted");

    check(!paths.set("sessionData", root + "/other"), "independent session selection");
    check(!paths.set("userData", root + "/app"), "second userdata selection");
    check(paths.session_root() == root + "/other", "explicit session path changed with userdata");
    check(!paths.seal(), "native path seal");
    check(paths.set("sessionData", root + "/data ünicode").has_value(),
        "live session rebinding accepted");
    check(!paths.set_logs(root + "/new/logs") && QDir(root + "/new/logs").exists(),
        "real recursive log directory creation");
    check(!paths.get("logs", value) && value == root + "/new/logs", "log selection");
    check(paths.get("unsupported", value).has_value(), "unknown path accepted");
    check(!paths.get("exe", value) && QFile::exists(value), "actual executable path");
    std::cout << "PASS native app paths/real directories, session inheritance/override, failure "
                 "rollback and sealing\n";
}
