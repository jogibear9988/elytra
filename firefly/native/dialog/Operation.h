/** Private platform operations and compile-time-only controlled-provider test seam. */
#pragma once
#include "Provider.h"

namespace Firefly::Dialogs::Detail {
class Operation {
public:
    virtual ~Operation() = default;
    // Requests actual native cancellation; admission remains charged until native
    // completion/closure. stop may retire VM-facing callbacks first.
    virtual void cancel() = 0;
};

struct Started {
    std::shared_ptr<Operation> operation;
    Result result;
};

Started start_files(QString const&, QJsonObject const&, QWidget*, Completion);
#if defined(FIREFLY_DIALOG_PROVIDER_TESTING)
Started start_message_for_testing(QJsonObject const&, QWidget*, Completion);

Result message_sync_for_testing(QJsonObject const&, QWidget*);

using AsyncDriver
    = std::function<Started(int, QString const&, QJsonObject const&, QWidget*, Completion)>;
using SyncDriver = std::function<Result(QJsonObject const&, QWidget*)>;
std::unique_ptr<Provider> create_provider_for_testing(AsyncDriver, SyncDriver);
#endif
}
