#include "Provider.h"

namespace Firefly::RecentDocuments {
bool available()
{
    return false;
}

Result clear()
{
    return { "ERR_FIREFLY_UNSUPPORTED_API",
        "Native recent documents are unavailable on this platform" };
}

Result add(std::string const&)
{
    return clear();
}
}
