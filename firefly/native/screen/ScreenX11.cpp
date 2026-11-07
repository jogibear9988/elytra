/** Combine real RandR output identities/rotation with Qt's native DIP geometry and cursor.
 * An unmatched/ambiguous Qt screen fails instead of acquiring a fabricated index/hash ID.
 */
#include "X11.h"
#include <QCoreApplication>
#include <QCursor>
#include <QGuiApplication>
#include <QJsonArray>
#include <QScreen>
#include <QThread>
#include <cstdlib>
#include <memory>
#include <xcb/randr.h>

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
        return application && QGuiApplication::platformName() == "xcb"
            && QThread::currentThread() == application->thread();
    }

    template <typename T> using Reply = std::unique_ptr<T, decltype(&std::free)>;

    QJsonObject rectangle(QRect bounds)
    {
        return { { "x", bounds.x() }, { "y", bounds.y() }, { "width", bounds.width() },
            { "height", bounds.height() } };
    }

    int rotation(uint16_t value)
    {
        // RandR rotations are counterclockwise; Electron reports clockwise degrees.
        // Reflections cannot be represented by Electron's rotation field.
        if (value & (XCB_RANDR_ROTATION_REFLECT_X | XCB_RANDR_ROTATION_REFLECT_Y))
            return -1;
        switch (value) {
        case XCB_RANDR_ROTATION_ROTATE_0:
            return 0;
        case XCB_RANDR_ROTATION_ROTATE_90:
            return 270;
        case XCB_RANDR_ROTATION_ROTATE_180:
            return 180;
        case XCB_RANDR_ROTATION_ROTATE_270:
            return 90;
        default:
            return -1;
        }
    }
}

QJsonObject x11_screen_snapshot()
{
    if (!available())
        return failure("Display snapshots require the native xcb GUI application thread");
    auto* primary = QGuiApplication::primaryScreen();
    if (!primary)
        return failure("No native primary display is available");
    int screen_number = 0;
    auto connection = std::unique_ptr<xcb_connection_t, decltype(&xcb_disconnect)>(
        xcb_connect(nullptr, &screen_number), &xcb_disconnect);
    if (!connection || xcb_connection_has_error(connection.get()))
        return failure("Cannot connect to the native X11 server");
    auto* wire = connection.get();
    auto roots = xcb_setup_roots_iterator(xcb_get_setup(wire));
    for (int i = 0; i < screen_number && roots.rem; ++i)
        xcb_screen_next(&roots);
    if (!roots.rem)
        return failure("No matching native X11 root screen");
    auto* extension = xcb_get_extension_data(wire, &xcb_randr_id);
    if (!extension || !extension->present)
        return failure("Native display identities require the RandR extension");
    Reply<xcb_randr_query_version_reply_t> version(
        xcb_randr_query_version_reply(wire, xcb_randr_query_version(wire, 1, 3), nullptr),
        &std::free);
    if (!version || version->major_version < 1
        || (version->major_version == 1 && version->minor_version < 3))
        return failure("Native display identities require RandR 1.3");
    Reply<xcb_randr_get_screen_resources_current_reply_t> resources(
        xcb_randr_get_screen_resources_current_reply(
            wire, xcb_randr_get_screen_resources_current(wire, roots.data->root), nullptr),
        &std::free);
    if (!resources)
        return failure("Cannot read native RandR resources");
    QJsonArray displays;
    QJsonArray used_ids;
    double primary_id = -1;
    auto origin = primary->geometry().topLeft();
    auto* outputs = xcb_randr_get_screen_resources_current_outputs(resources.get());
    auto count = xcb_randr_get_screen_resources_current_outputs_length(resources.get());
    for (auto* screen : QGuiApplication::screens()) {
        xcb_randr_output_t identity = XCB_NONE;
        int native_rotation = -1;
        auto name = screen->name().toUtf8();
        for (int i = 0; i < count; ++i) {
            Reply<xcb_randr_get_output_info_reply_t> info(
                xcb_randr_get_output_info_reply(wire,
                    xcb_randr_get_output_info(wire, outputs[i], resources->config_timestamp),
                    nullptr),
                &std::free);
            if (!info || info->status != XCB_RANDR_SET_CONFIG_SUCCESS)
                return failure("RandR output changed during the display snapshot");
            if (info->connection != XCB_RANDR_CONNECTION_CONNECTED || info->crtc == XCB_NONE)
                continue;
            QByteArray output_name(
                reinterpret_cast<char const*>(xcb_randr_get_output_info_name(info.get())),
                xcb_randr_get_output_info_name_length(info.get()));
            if (name != output_name)
                continue;
            if (identity != XCB_NONE)
                return failure("Ambiguous native RandR output name");
            Reply<xcb_randr_get_crtc_info_reply_t> crtc(
                xcb_randr_get_crtc_info_reply(wire,
                    xcb_randr_get_crtc_info(wire, info->crtc, resources->config_timestamp),
                    nullptr),
                &std::free);
            if (!crtc || crtc->status != XCB_RANDR_SET_CONFIG_SUCCESS)
                return failure("RandR CRTC changed during the display snapshot");
            identity = outputs[i];
            native_rotation = rotation(crtc->rotation);
        }

        if (identity == XCB_NONE || native_rotation < 0)
            return failure("No unique unreflected RandR output matches the Qt display");
        auto id = static_cast<double>(identity);
        if (used_ids.contains(id))
            return failure("Multiple Qt displays map to one RandR output");
        used_ids.append(id);
        if (screen == primary)
            primary_id = id;
        auto bounds = screen->geometry().translated(-origin);
        auto work = screen->availableGeometry().translated(-origin);
        displays.append(QJsonObject { { "id", id }, { "bounds", rectangle(bounds) },
            { "workArea", rectangle(work) },
            { "size", QJsonObject { { "width", bounds.width() }, { "height", bounds.height() } } },
            { "workAreaSize",
                QJsonObject { { "width", work.width() }, { "height", work.height() } } },
            { "scaleFactor", screen->devicePixelRatio() }, { "rotation", native_rotation },
            { "detected", true }, { "displayFrequency", screen->refreshRate() },
            { "label", screen->name() }, { "touchSupport", "unknown" },
            { "accelerometerSupport", "unknown" } });
    }

    if (primary_id < 0)
        return failure("Primary Qt display has no native RandR identity");
    // Internal-panel/color/depth fields are deliberately absent until backed by
    // verified native metadata; a connector name alone does not prove a panel.
    return { { "primaryId", primary_id }, { "displays", displays } };
}

QJsonObject x11_cursor_screen_point()
{
    if (!available() || !QGuiApplication::primaryScreen())
        return failure("Cursor snapshots require the native xcb GUI application thread");
    auto point = QCursor::pos() - QGuiApplication::primaryScreen()->geometry().topLeft();
    return { { "x", point.x() }, { "y", point.y() } };
}
}
