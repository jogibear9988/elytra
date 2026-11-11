/** Native media-element gesture state, retired by the actual element destructor. */
#pragma once
#include <AK/Optional.h>
#include <LibWeb/Export.h>

namespace Web::DOM {
class Document;
}

namespace Web::HTML {
class HTMLMediaElement;
}

namespace Firefly::Autoplay {
WEB_API void initialize(Web::HTML::HTMLMediaElement&);

WEB_API void destroyed(Web::HTML::HTMLMediaElement&);

WEB_API void adopted(Web::HTML::HTMLMediaElement&, Web::DOM::Document const& old_document);

WEB_API void request_play(Web::HTML::HTMLMediaElement&);

WEB_API void request_load(Web::HTML::HTMLMediaElement&);

WEB_API void request_audible(Web::HTML::HTMLMediaElement&);

WEB_API void native_play(Web::HTML::HTMLMediaElement&);

WEB_API Optional<bool> allowed(Web::HTML::HTMLMediaElement const&);
}
