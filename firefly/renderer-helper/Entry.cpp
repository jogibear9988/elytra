/**
 * Owned WebContent executable entry. Reuse the upstream bootstrap/sandbox and install the
 * selected service only after the renderer VM is initialized.
 */

// Reuse upstream's complete bootstrap/sandbox/connection setup. Only its entry
// name and VM-initialization call are adapted in this translation unit.
#include "RendererSession.h"
#include <LibMain/Main.h>
#include <LibWeb/Bindings/MainThreadVM.h>

namespace Web::Bindings {
void firefly_initialize_renderer_vm(HTML::AgentType);
}

ErrorOr<int> firefly_upstream_renderer_main(Main::Arguments);
#define ladybird_main firefly_upstream_renderer_main
#define initialize_main_thread_vm firefly_initialize_renderer_vm
#include <Services/WebContent/main.cpp>
#undef initialize_main_thread_vm
#undef ladybird_main

namespace Web::Bindings {
void firefly_initialize_renderer_vm(HTML::AgentType type)
{
    initialize_main_thread_vm(type);
    Firefly::RendererSession::initialize();
}
}

ErrorOr<int> ladybird_main(Main::Arguments arguments)
{
    auto result = firefly_upstream_renderer_main(arguments);
    Firefly::RendererSession::shutdown();
    return result;
}
