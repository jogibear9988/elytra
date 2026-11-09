/** Single UI-library instance shared by helper launch composition and the application broker. */
#include "Policy.h"
#include <AK/NeverDestroyed.h>

namespace Firefly::WebRequest {
namespace {
    NeverDestroyed<Policy> current_policy;
}

Policy policy()
{
    return *current_policy;
}

void set_policy(Policy policy)
{
    *current_policy = policy;
}
}
