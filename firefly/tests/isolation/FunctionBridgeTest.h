#pragma once
#include "IsolatedWorld.h"
namespace Web::DOM { class Document; }
ErrorOr<void> check_function_bridge(JS::VM&, Web::DOM::Document&);
