/** Pair native HMONITOR identities/metadata with Qt's actual per-monitor DIP geometry. */
#include "ScreenWindows.h"
#include "Screen.h"
#include <QCursor>
#include <QGuiApplication>
#include <QJsonArray>
#include <QScreen>
#include <QSet>
#include <QThread>
#include <QtCore/qt_windows.h>
#include <cstdint>
#include <optional>
#include <vector>

namespace Firefly {
namespace {
    QJsonObject failure(char const* message)
    {
        return { { "error",
            QJsonObject {
                { "code", "ERR_FIREFLY_DISPLAY_UNAVAILABLE" }, { "message", message } } } };
    }

    bool available()
    {
        auto* application = qobject_cast<QGuiApplication*>(QCoreApplication::instance());
        return application && QGuiApplication::platformName() == QStringLiteral("windows")
            && QThread::currentThread() == application->thread();
    }

    QJsonObject rectangle(QRect bounds)
    {
        return { { "x", bounds.x() }, { "y", bounds.y() }, { "width", bounds.width() },
            { "height", bounds.height() } };
    }

    struct Monitor {
        HMONITOR handle;
        MONITORINFOEXW info;
    };

    BOOL CALLBACK collect(HMONITOR monitor, HDC, LPRECT, LPARAM context)
    {
        auto& result = *reinterpret_cast<std::vector<Monitor>*>(context);
        if (result.size() >= 128)
            return FALSE;
        MONITORINFOEXW info {};
        info.cbSize = sizeof(info);
        if (!GetMonitorInfoW(monitor, reinterpret_cast<MONITORINFO*>(&info)))
            return FALSE;
        result.push_back({ monitor, info });
        return TRUE;
    }

    std::optional<bool> internal_display(
        QString const& name, std::vector<DISPLAYCONFIG_PATH_INFO> const& paths)
    {
        std::optional<bool> result;
        for (auto const& path : paths) {
            DISPLAYCONFIG_SOURCE_DEVICE_NAME source {};
            source.header = { DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME, sizeof(source),
                path.sourceInfo.adapterId, path.sourceInfo.id };
            if (DisplayConfigGetDeviceInfo(&source.header) != ERROR_SUCCESS)
                return {};
            if (name.compare(QString::fromWCharArray(source.viewGdiDeviceName), Qt::CaseInsensitive)
                != 0)
                continue;
            switch (path.targetInfo.outputTechnology) {
            case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_INTERNAL:
            case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_LVDS:
            case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_DISPLAYPORT_EMBEDDED:
            case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_UDI_EMBEDDED:
                result = true;
                break;
            default:
                if (!result.has_value())
                    result = false;
                break;
            }
        }

        return result;
    }
}

int windows_display_rotation(unsigned long native_orientation)
{
    // DEVMODEW defines DMDO_* counter-clockwise; Electron Display.rotation
    // exposes clockwise degrees. Unknown native values must not become zero.
    switch (native_orientation) {
    case DMDO_DEFAULT:
        return 0;
    case DMDO_90:
        return 270;
    case DMDO_180:
        return 180;
    case DMDO_270:
        return 90;
    default:
        return -1;
    }
}

QJsonObject screen_snapshot()
{
    if (!available())
        return failure("Display snapshots require the native Windows GUI application thread");
    std::vector<Monitor> monitors;
    monitors.reserve(128);
    if (!EnumDisplayMonitors(nullptr, nullptr, collect, reinterpret_cast<LPARAM>(&monitors)))
        return failure("Cannot enumerate native Windows monitors");
    UINT32 path_count = 0;
    UINT32 mode_count = 0;
    if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &path_count, &mode_count)
            != ERROR_SUCCESS
        || path_count > 128 || mode_count > 512)
        return failure("Cannot size native Windows display configuration");
    std::vector<DISPLAYCONFIG_PATH_INFO> paths(path_count);
    std::vector<DISPLAYCONFIG_MODE_INFO> modes(mode_count);
    if (QueryDisplayConfig(
            QDC_ONLY_ACTIVE_PATHS, &path_count, paths.data(), &mode_count, modes.data(), nullptr)
        != ERROR_SUCCESS)
        return failure("Native Windows display configuration changed during read");
    paths.resize(path_count);
    QJsonArray displays;
    double primary_id = -1;
    QSet<quintptr> seen;
    for (auto* screen : QGuiApplication::screens()) {
        Monitor const* match = nullptr;
        for (auto const& monitor : monitors) {
            if (screen->name().compare(
                    QString::fromWCharArray(monitor.info.szDevice), Qt::CaseInsensitive)
                == 0) {
                if (match)
                    return failure("Ambiguous native Windows monitor identity");
                match = &monitor;
            }
        }

        if (!match)
            return failure("Cannot pair a Qt screen with its native Windows monitor");
        auto id = reinterpret_cast<uintptr_t>(match->handle);
        if (id == 0 || id > 9007199254740991ULL || seen.contains(id))
            return failure("Native Windows monitor identity cannot be represented exactly");
        seen.insert(id);
        DEVMODEW mode {};
        mode.dmSize = sizeof(mode);
        if (!EnumDisplaySettingsExW(match->info.szDevice, ENUM_CURRENT_SETTINGS, &mode, 0)
            || !(mode.dmFields & DM_DISPLAYORIENTATION))
            return failure("Cannot read native Windows display orientation");
        auto rotation = windows_display_rotation(mode.dmDisplayOrientation);
        if (rotation < 0)
            return failure("Unknown native Windows display orientation");
        if (screen == QGuiApplication::primaryScreen())
            primary_id = static_cast<double>(id);
        auto internal = internal_display(screen->name(), paths);
        if (!internal.has_value())
            return failure("Cannot pair native Windows display output metadata");
        auto bounds = rectangle(screen->geometry());
        auto work = rectangle(screen->availableGeometry());
        displays.append(QJsonObject { { "id", static_cast<double>(id) }, { "bounds", bounds },
            { "workArea", work },
            { "size",
                QJsonObject { { "width", bounds["width"] }, { "height", bounds["height"] } } },
            { "workAreaSize",
                QJsonObject { { "width", work["width"] }, { "height", work["height"] } } },
            { "scaleFactor", screen->devicePixelRatio() }, { "rotation", rotation },
            { "internal", *internal }, { "detected", true },
            { "displayFrequency",
                (mode.dmFields & DM_DISPLAYFREQUENCY) && mode.dmDisplayFrequency > 1
                    ? static_cast<double>(mode.dmDisplayFrequency)
                    : 0.0 },
            { "label", screen->name() }, { "touchSupport", "unknown" },
            { "accelerometerSupport", "unknown" } });
    }

    if (displays.isEmpty() || primary_id < 0)
        return failure("No native primary Windows display is available");
    return { { "primaryId", primary_id }, { "displays", displays } };
}

QJsonObject cursor_screen_point()
{
    if (!available())
        return failure("Cursor snapshots require the native Windows GUI application thread");
    auto point = QCursor::pos();
    return { { "x", point.x() }, { "y", point.y() } };
}
}
