/** Unsupported platforms never impersonate an observed empty enterprise policy set. */
#include "Provider.h"

namespace Mite::Policy {
ErrorOr<NonnullOwnPtr<Provider>> Provider::create(ByteString, Vector<Definition>)
{
    return Error::from_errno(ENOTSUP);
}
}
