/**
 * Allow the browser module loader to reach the existing file subresource loader.
 * Keep programmatic fetch, network origins, workers and non-script destinations
 * on their upstream paths. This supplies no new JS/native capability.
 */
#include "loading/FileModules.h"
#include <LibWeb/Fetch/Infrastructure/ClientContextSnapshot.h>
#include <LibWeb/Fetch/Infrastructure/HTTP/Requests.h>
#include <atomic>

namespace Firefly::FileModules {
namespace {
    std::atomic<bool> enabled { false };
}

void set_enabled(bool value)
{
    enabled.store(value, std::memory_order_relaxed);
}

bool allows(Web::Fetch::Infrastructure::Request const& request)
{
    using Request = Web::Fetch::Infrastructure::Request;
    using Client = Web::Fetch::Infrastructure::ClientContext;
    if (!enabled.load(std::memory_order_relaxed) || request.mode() != Request::Mode::CORS
        || request.destination() != Request::Destination::Script
        || request.current_url().scheme() != "file"sv)
        return false;
    auto const* origin = request.origin().get_pointer<URL::Origin>();
    auto const& client = request.client_snapshot();
    return origin && origin->is_file_origin() && client
        && client->global_kind == Client::GlobalKind::Window
        && client->creation_url.scheme() == "file"sv && client->origin.is_file_origin();
}
}
