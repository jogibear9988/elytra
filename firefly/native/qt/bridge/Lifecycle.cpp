/** Own hook installation, callback dispatch and teardown before the VM is destroyed. */
#include "../ChromeWindow.h"
#include "../PreloadHooks.h"
#include "Impl.h"
#include "TransportHooks.h"
#include "Value.h"
#include "core/EventLoop.h"
#include <LibCore/EventLoop.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <QTimer>
#include <UI/Qt/WebContentView.h>

namespace Firefly {
using BridgeDetail::text;

Bridge::Impl::Impl(JS::VM& vm, JS::Realm& realm, Mite::EventLoop& jobs, Core::EventLoop& gui,
    std::optional<int>& exit_code, bool electron_compat)
    : vm(vm)
    , realm(realm)
    , jobs(jobs)
    , gui(gui)
    , exit_code(exit_code)
    , electron_compat(electron_compat)
{
    set_preload_configurator([this](auto&, auto& page) { configure_preloads(page); });
    set_native_ipc_handler([this](auto& page, auto navigable, auto params) {
        receive_ipc(page, navigable, move(params));
    });
}

Bridge::Impl::~Impl()
{
    stop();
}

void Bridge::Impl::stop()
{
    if (stopping)
        return;
    stopping = true;
    set_preload_configurator({});
    set_native_ipc_handler({});
    while (!windows.empty())
        delete windows.begin()->second.window;
    context.reset(); // Cancel queued Qt callbacks before the Mite VM goes away.
    handler = nullptr;
    api = nullptr;
}

void Bridge::Impl::deliver(int id, ByteString const& type, ByteString const& data)
{
    if (stopping || !handler)
        return;
    if (jobs.is_busy()) {
        QTimer::singleShot(1, context.get(), [this, id, type, data] { deliver(id, type, data); });
        return;
    }

    jobs.dispatch([&] {
        return JS::call(
            vm, *handler, JS::js_undefined(), JS::Value(id), text(vm, type), text(vm, data));
    });
    if (jobs.status())
        gui.quit(jobs.status());
}

void Bridge::Impl::enqueue(int id, ByteString type, ByteString data)
{
    if (stopping)
        return;
    QTimer::singleShot(0, context.get(),
        [this, id, type = move(type), data = move(data)] { deliver(id, type, data); });
}
}
