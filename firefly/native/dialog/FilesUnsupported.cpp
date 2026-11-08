/** File chooser semantics await native acceptance outside macOS; never silently narrow mixed modes.
 */
#include "Operation.h"

namespace Firefly::Dialogs::Detail {
Started start_files(QString const&, QJsonObject const&, QWidget*, Completion)
{
    return { {},
        { {}, "ERR_FIREFLY_UNSUPPORTED_API",
            "Native file dialogs are not implemented on this platform" } };
}
}
