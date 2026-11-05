#pragma once
#include <AK/Error.h>
#include <LibJS/Forward.h>
namespace Web::DOM { class Document; }
ErrorOr<void> check_world_job_guard(JS::VM&, Web::DOM::Document&);
