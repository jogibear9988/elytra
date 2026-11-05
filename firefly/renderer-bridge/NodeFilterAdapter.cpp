#include "CallbackGate.h"
#include <LibWeb/DOM/BindingsGlue.h>
#include <LibWeb/DOM/Node.h>
namespace Web::Bindings {
JS::ThrowCompletionOr<DOM::NodeFilter::Result> firefly_original_accept_node(JS::Realm&, DOM::NodeFilter&, DOM::Node&);
}
#include <NodeFilter.inc>

namespace Firefly::Callbacks {
Web::DOM::NodeFilter* probe_filter(Web::DOM::TreeWalker& walker) { return walker.filter().ptr(); }
JS::ThrowCompletionOr<Web::DOM::NodeFilter::Result> invoke_filter_original(JS::Realm& realm, Web::DOM::NodeFilter& filter, Web::DOM::Node& node)
{
    return Web::Bindings::firefly_original_accept_node(realm, filter, node);
}
}
namespace Web::Bindings {
JS::ThrowCompletionOr<DOM::NodeFilter::Result> accept_node(JS::Realm& realm, DOM::NodeFilter& filter, DOM::Node& node)
{
    return Firefly::Callbacks::dispatch_filter(realm, filter, node);
}
}
