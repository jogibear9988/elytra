/** Install command-line controls only on the trusted main host. */
#pragma once

namespace JS {
class Realm;
class Object;
}

namespace Firefly::CommandLine {
class State;
void install_host(JS::Realm&, JS::Object&, State&);
}
