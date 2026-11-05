#pragma once
#include <AK/Error.h>
#include <LibJS/Forward.h>
ErrorOr<void> check_bridge_data(JS::VM&, JS::Realm&, JS::Realm&);
