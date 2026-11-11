/** Owner/context-bound native app bindings, unavailable to renderer/preload realms. */
#pragma once
class QObject;

namespace JS {
class Realm;
class Object;
}

namespace Firefly::RecentDocuments {
void install_host(JS::Realm&, JS::Object&, QObject& context);
}
