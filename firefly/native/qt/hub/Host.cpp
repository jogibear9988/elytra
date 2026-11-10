/** In-process UI/Hub transport and native platform observations, without browser stores or startup
 * tabs. */
#include "Host.h"
#include "../../crash/Reporter.h"
#include "../../locale/Locale.h"
#include <LibCore/EventLoop.h>
#include <LibIPC/Transport.h>
#include <LibIPC/TransportHandle.h>
#include <QApplication>
#include <QFont>
#include <QStyleHints>
#include <QThread>

namespace Firefly {
HubHost::HubHost(Core::EventLoop& loop, ByteString binary_directory,
    Function<void()> initialize_frontend, Function<void()> close_frontend)
    : m_loop(loop)
    , m_configuration { loop, move(binary_directory) }
    , m_initialize_frontend(move(initialize_frontend))
    , m_close_frontend(move(close_frontend))
{
    VERIFY(qApp && QThread::currentThread() == qApp->thread());
    VERIFY(m_initialize_frontend && m_close_frontend);
    HubLifecycle::install(m_configuration);
}

HubHost::~HubHost()
{
    shutdown();
    HubLifecycle::clear(m_configuration);
}

ErrorOr<void> HubHost::initialize(Main::Arguments const& arguments)
{
    VERIFY(qApp && QThread::currentThread() == qApp->thread());
    if (m_stopped || m_hub)
        return Error::from_string_literal("Firefly Hub ownership cannot be restarted");
    CrashReporting::begin_embedded_profile();
    auto hub = TRY(Hub::Application::create(arguments, this));
    auto transports = TRY(IPC::TransportHandle::create_pair());
    auto server = Hub::ConnectionFromUI::construct(TRY(transports.first.create_transport()));
    auto client
        = adopt_ref(*new Ladybird::HubConnection(TRY(transports.second.create_transport())));
    m_hub = move(hub);
    m_server = move(server);
    m_client = move(client);
    return {};
}

void HubHost::shutdown()
{
    if (m_stopped)
        return;
    m_stopped = true;
    // RemoteView destruction still sends actual destroy-view messages through
    // the live UI transport. The close callback must finish before it goes away.
    m_close_frontend();
    if (m_hub)
        m_hub->destroy_all_views();
    m_client = nullptr;
    m_server = nullptr;
    if (m_hub)
        m_hub->detach_from_host();
    m_hub = nullptr;
}

void HubHost::will_create_event_loop(Main::Arguments&)
{
    // Firefly creates QApplication and installs EventLoopManagerQt before the
    // deferred profile initialization. The Hub must reuse that exact owner.
    VERIFY(qApp && QThread::currentThread() == qApp->thread());
}

void HubHost::did_create_event_loop(Core::EventLoop& loop)
{
    VERIFY(&loop == &m_loop);
}

void HubHost::create_platform_actions()
{
    // The application-owned frontend installs only the services used by its
    // embedded views. It does not initialize browser bookmarks/history/actions.
    m_initialize_frontend();
}

Optional<String> HubHost::system_font_family() const
{
    auto name = QApplication::font().family().toUtf8();
    return MUST(String::from_utf8(StringView(name.data(), name.size())));
}

Optional<String> HubHost::ui_font_family() const
{
    return system_font_family();
}

Web::CSS::PreferredColorScheme HubHost::interface_color_scheme() const
{
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark
        ? Web::CSS::PreferredColorScheme::Dark
        : Web::CSS::PreferredColorScheme::Light;
}

// Browser-only surfaces have no Firefly implementation and must not be offered
// to pages merely because the upstream browser supports them.
bool HubHost::supports_picture_in_picture() const
{
    return false;
}

bool HubHost::supports_clipboard_type(WebView::Application::ClipboardType type) const
{
    return type == WebView::Application::ClipboardType::Text;
}

bool HubHost::supports_system_menu_bar() const
{
    return false;
}

bool HubHost::supports_vertical_tabs() const
{
    return false;
}

bool HubHost::supports_private_browsing_windows() const
{
    return false;
}

bool HubHost::supports_client_side_window_decorations() const
{
    return false;
}

bool HubHost::platform_reports_scroll_momentum() const
{
    return QGuiApplication::platformName() == QStringLiteral("cocoa");
}

bool HubHost::platform_scrolls_elastically() const
{
    return platform_reports_scroll_momentum();
}

bool HubHost::platform_scrollbar_track_press_jumps() const
{
    return false;
}

Vector<String> HubHost::platform_preferred_languages() const
{
    Vector<String> result;
    for (auto const& language : Locale::preferred_languages()) {
        auto bytes = language.toUtf8();
        auto text = String::from_utf8(StringView(bytes.data(), bytes.size()));
        if (!text.is_error())
            result.append(text.release_value());
    }

    return result;
}

Optional<Gfx::GpuDeviceIdentity> HubHost::platform_presentation_gpu_device() const
{
    return {};
}

Vector<u64> HubHost::platform_presentation_drm_format_modifiers() const
{
    return {};
}
}
