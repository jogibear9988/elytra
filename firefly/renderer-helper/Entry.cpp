/**
 * Owned WebContent executable entry. Reuse the upstream bootstrap/sandbox and install the
 * selected service only after the renderer VM is initialized.
 */

// Consume the owned native policy before upstream bootstrap/sandbox/connection
// setup. Rename only its entry and VM-initialization call inside the composition.
#include "RendererSession.h"
#include "autoplay/Session.h"
#include "canvas/Session.h"
#include "native/protocol/FilePolicy.h"
#include "native/protocol/OriginConfig.h"
#include "permissions/Session.h"
#include "preload/Process.h"
#include "renderer-bridge/autoplay/Policy.h"
#include "resources/Session.h"
#include "web-request/Session.h"
#include <LibCore/System.h>
#include <LibMain/Main.h>
#include <LibWeb/Bindings/MainThreadVM.h>

namespace Web::Bindings {
void firefly_initialize_renderer_vm(HTML::AgentType);
}

ErrorOr<int> firefly_upstream_renderer_main(Main::Arguments);
#define ladybird_main firefly_upstream_renderer_main
#define initialize_agent firefly_initialize_renderer_vm
#include <Services/WebContent/main.cpp>
#undef initialize_agent
#undef ladybird_main

namespace Web::Bindings {
void firefly_initialize_renderer_vm(HTML::AgentType type)
{
    initialize_agent(type);
    Firefly::Autoplay::initialize_owned();
    Firefly::HelperCanvas::initialize();
    Firefly::HelperPermissions::initialize();
    Firefly::HelperWebRequest::initialize();
    Firefly::RendererSession::initialize();
    Firefly::Resources::initialize();
}
}

ErrorOr<int> ladybird_main(Main::Arguments arguments)
{
    TRY(Firefly::PreloadProcess::prepare());
    auto origin = TRY(Firefly::Protocols::consume_origin_configuration(arguments.strings));
    auto file_policy
        = TRY(Firefly::Protocols::consume_file_policy_configuration(origin.strings.span()));
    auto permissions = TRY(Firefly::HelperPermissions::prepare(file_policy.strings.span()));
    auto filtered = TRY(Firefly::HelperWebRequest::prepare(permissions.strings.span()));
    // Read-only startup evidence while this internal protocol path is under
    // development. No page/preload command can set or replace this policy.
    if (auto configuration = Firefly::Protocols::configuration_for_helper();
        configuration.has_value())
        dbgln("FIREFLY_ORIGIN_POLICY_READY pid={} configuration={}", Core::System::getpid(),
            *configuration);
    auto result = firefly_upstream_renderer_main({ static_cast<int>(filtered.strings.size()),
        filtered.argv.data(), filtered.strings.span() });
    Firefly::Autoplay::shutdown();
    Firefly::HelperCanvas::shutdown();
    Firefly::HelperWebRequest::shutdown();
    Firefly::HelperPermissions::shutdown();
    Firefly::Resources::shutdown();
    Firefly::RendererSession::shutdown();
    return result;
}
