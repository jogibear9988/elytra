#pragma once
#include <AK/Error.h>
#include <LibJS/Forward.h>
ErrorOr<void> check_bridge_data(JS::VM&, JS::Realm&, JS::Realm&);

ErrorOr<void> check_bridge_wire(JS::VM&, JS::Realm&, JS::Realm&);

ErrorOr<void> check_binary_wire(JS::VM&, JS::Realm&, JS::Realm&);

ErrorOr<void> check_binary_views(JS::VM&, JS::Realm&, JS::Realm&);
