/** Compose the genuine in-process Hub with Firefly's Qt owner; never create Ladybird browser UI. */
#pragma once
#include "Lifecycle.h"
#include <Hub/Application.h>
#include <Hub/ConnectionFromUI.h>
#include <UI/Qt/HubConnection.h>

#ifdef LADYBIRD_HUB_IN_OWN_PROCESS
#error "Firefly's synchronous canonical-document policies require the in-process Hub"
#endif

namespace Firefly {
class HubHost final : public Hub::ApplicationHost {
    AK_ALLOC_WITH_KMALLOC;

public:
    // Qt, its event loop and both callbacks outlive this owner. close_frontend
    // destroys actual widgets before the Hub destroys canonical views/helpers.
    HubHost(Core::EventLoop&, ByteString binary_directory, Function<void()> initialize_frontend,
        Function<void()> close_frontend);

    ~HubHost() override;

    ErrorOr<void> initialize(Main::Arguments const&);

    void shutdown();

    bool initialized() const
    {
        return !!m_hub;
    }

    Hub::Application& application()
    {
        VERIFY(m_hub);

        return *m_hub;
    }

private:
    void will_create_event_loop(Main::Arguments&) override;

    void did_create_event_loop(Core::EventLoop&) override;

    void create_platform_actions() override;

    Optional<String> ui_font_family() const override;

    Web::CSS::PreferredColorScheme interface_color_scheme() const override;

    Optional<String> system_font_family() const override;

    bool supports_picture_in_picture() const override;

    bool supports_clipboard_type(WebView::Application::ClipboardType) const override;

    bool supports_system_menu_bar() const override;

    bool supports_vertical_tabs() const override;

    bool supports_private_browsing_windows() const override;

    bool supports_client_side_window_decorations() const override;

    bool platform_reports_scroll_momentum() const override;

    bool platform_scrolls_elastically() const override;

    bool platform_scrollbar_track_press_jumps() const override;

    Vector<String> platform_preferred_languages() const override;

    Optional<Gfx::GpuDeviceIdentity> platform_presentation_gpu_device() const override;

    Vector<u64> platform_presentation_drm_format_modifiers() const override;

    Core::EventLoop& m_loop;
    HubLifecycle::Configuration m_configuration;
    Function<void()> m_initialize_frontend;
    Function<void()> m_close_frontend;
    OwnPtr<Hub::Application> m_hub;
    RefPtr<Hub::ConnectionFromUI> m_server;
    RefPtr<Ladybird::HubConnection> m_client;
    bool m_stopped { false };
};
}
