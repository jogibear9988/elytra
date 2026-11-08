/** Exact provider registry with controlled native operation drivers; never opens a dialog. */
#include "native/dialog/Operation.h"
#include <QApplication>
#include <QJsonArray>
#include <QWidget>
#include <cstdio>
#include <thread>
#include <vector>

#if !defined(FIREFLY_DIALOG_PROVIDER_TESTING)
#error "This controlled peer requires the compile-time-only test factory"
#endif
using Firefly::Dialogs::Completion;
using Firefly::Dialogs::Provider;
using Firefly::Dialogs::Result;
namespace Detail = Firefly::Dialogs::Detail;

namespace {
bool passed = true;

void check(bool value, char const* message)
{
    if (!value) {
        fprintf(stderr, "FAIL controlled native dialog provider: %s\n", message);
        passed = false;
    }
}

QJsonObject message()
{
    return { { "type", "warning" }, { "title", "Owned title" },
        { "message", "<b>plain message</b>" }, { "detail", "Visible detail Grüße" },
        { "buttons", QJsonArray { "Keep", "Abort" } }, { "defaultId", 0 }, { "cancelId", 1 },
        { "checkboxLabel", "Remember" }, { "checkboxChecked", false },
        { "normalizeAccessKeys", true }, { "noLink", true } };
}

QJsonObject files()
{
    return { { "title", "Owned files" }, { "defaultPath", "/owned/Grüße" },
        { "filters", QJsonArray {} },
        { "properties",
            QJsonArray {
                "openFile", "openDirectory", "multiSelections", "treatPackageAsDirectory" } } };
}

struct Peer {
    Completion complete;
    bool canceled {}, finished {};

    void finish(Result result)
    {
        finished = true;
        auto callback = std::move(complete);
        if (callback)
            callback(std::move(result));
    }
};

class Operation final : public Detail::Operation {
public:
    explicit Operation(std::shared_ptr<Peer> peer)
        : peer(std::move(peer))
    {
    }

    void cancel() override
    {
        if (!peer->finished)
            peer->canceled = true;
    }

private:
    std::shared_ptr<Peer> peer;
};

struct Driver {
    std::vector<std::shared_ptr<Peer>> peers;
    bool active {}, early {}, reject {}, null_operation {};
    QWidget* last_parent {};
    QJsonObject last_options;
    std::function<void()> during;

    Detail::Started start(
        int, QString const&, QJsonObject const& options, QWidget* parent, Completion complete)
    {
        active = true;
        last_parent = parent;
        last_options = options;
        auto peer = std::make_shared<Peer>();
        peer->complete = std::move(complete);
        peers.push_back(peer);
        if (early)
            peer->finish({ { { "response", 0 }, { "checkboxChecked", true } }, {}, {} });
        if (during)
            during();
        active = false;
        return { null_operation ? nullptr : std::make_shared<Operation>(peer),
            reject ? Result { {}, "ERR_CONTROLLED_NATIVE", "Controlled failure" } : Result {} };
    }

    std::unique_ptr<Provider> create()
    {
        return Detail::create_provider_for_testing(
            [this](int id, QString const& kind, QJsonObject const& options, QWidget* parent,
                Completion complete) {
                return start(id, kind, options, parent, std::move(complete));
            },
            [](QJsonObject const&, QWidget*) {
                return Result { { { "response", 1 }, { "checkboxChecked", true } }, {}, {} };
            });
    }
};
}

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication application(argc, argv);
    application.setQuitOnLastWindowClosed(false);
    Driver driver;
    auto provider = driver.create();
    unsigned completions {};
    Result observed;
    auto collect = [&](Result value) {
        ++completions;
        observed = std::move(value);
    };

    auto options = message();
    check(provider->start(1, "message", options, nullptr, collect).code.empty(),
        "actual provider admits native operation");
    check(driver.last_options == options && !completions,
        "message/detail/buttons/options remain native snapshot; no fake immediate result");
    provider->cancel(1);
    provider->cancel(1);
    check(driver.peers.back()->canceled && !completions,
        "cancel requests native close but cannot invent its completion");
    auto duplicate = driver.peers.back()->complete;
    driver.peers.back()->finish({ { { "response", 1 }, { "checkboxChecked", true } }, {}, {} });
    duplicate({ { { "response", 0 }, { "checkboxChecked", false } }, {}, {} });
    check(completions == 1 && observed.value.value("response").toInt() == 1
            && observed.value.value("checkboxChecked").toBool(),
        "actual completion delivered once with original index/checkbox payload");
    check(provider->start(1, "message", options, nullptr, collect).code == "ERR_INVALID_ARG_VALUE",
        "identities cannot be reused");
    auto parent = std::make_unique<QWidget>();
    check(provider->start(2, "open", files(), parent.get(), collect).code.empty()
            && driver.last_parent == parent.get(),
        "actual hidden QWidget parent forwarded without opening UI");
    auto parent_peer = driver.peers.back();
    parent.reset();
    check(parent_peer->canceled && completions == 1,
        "parent destruction requests genuine native cancel");
    parent_peer->finish({ { { "canceled", true }, { "filePaths", QJsonArray {} } }, {}, {} });
    check(completions == 2 && observed.value.value("canceled").toBool(),
        "parent-close completion remains real native outcome");
    auto sync = provider->message_sync(options, nullptr);
    check(sync.code.empty() && sync.value.value("response").toInt() == 1
            && sync.value.value("checkboxChecked").toBool(),
        "sync path returns actual native driver resultobject");
    auto invalid = options;
    invalid.insert("securityScopedBookmarks", true);
    check(provider->start(3, "message", invalid, nullptr, collect).code
            == "ERR_FIREFLY_UNSUPPORTED_API",
        "unknown/native-privilege options rejected before driver");
    Result off_owner;
    std::thread worker(
        [&] { off_owner = provider->start(3, "message", options, nullptr, collect); });
    worker.join();
    check(off_owner.code == "ERR_FIREFLY_DIALOG_NATIVE", "off-owner UI admission rejected");
    provider->stop();

    Driver early;
    early.early = true;
    auto eager = early.create();
    check(eager
              ->start(1, "message", options, nullptr,
                  [&](Result) {
                      check(
                          !early.active, "native completion is held until valid admission returns");
                      ++completions;
                  })
              .code.empty(),
        "synchronous native callback safely admitted");
    auto before = completions;
    early.reject = true;
    check(eager->start(2, "message", options, nullptr, collect).code == "ERR_CONTROLLED_NATIVE"
            && completions == before,
        "failed admission cannot publish early success");
    eager->stop();
    Driver absent;
    absent.null_operation = true;
    auto missing = absent.create();
    check(
        missing->start(1, "message", options, nullptr, collect).code == "ERR_FIREFLY_DIALOG_NATIVE",
        "missing native operation never reports admission success");
    missing->stop();

    Driver reentry;
    auto pending = reentry.create();
    check(pending
              ->start(1, "message", options, nullptr,
                  [&](Result) {
                      pending->stop();
                      ++completions;
                  })
              .code.empty(),
        "reentrant completion fixture admitted");
    reentry.peers.back()->finish({ { { "response", 0 }, { "checkboxChecked", false } }, {}, {} });
    check(pending->start(2, "message", options, nullptr, collect).code == "ERR_FIREFLY_APP_EXITED",
        "completion may stop provider after registry retirement");
    Driver retired;
    auto destroyed = retired.create();
    retired.during = [&] { destroyed.reset(); };
    auto outcome = destroyed->start(1, "message", options, nullptr, collect);
    check(outcome.code == "ERR_FIREFLY_APP_EXITED" && retired.peers.back()->canceled,
        "destruction during native admission cancels eventual operation safely");
    before = completions;
    retired.peers.back()->finish({ { { "response", 0 }, { "checkboxChecked", false } }, {}, {} });
    check(completions == before,
        "late native completion after provider destruction cannot revive callback");

    Driver quota;
    for (int index = 0; index < 16; ++index) {
        auto generation = quota.create();
        check(generation->start(1, "open", files(), nullptr, collect).code.empty(),
            "pending native operation reserves actual quota");
        generation->stop();
    }

    auto limited = quota.create();
    check(limited->start(1, "open", files(), nullptr, collect).code == "ERR_FIREFLY_DIALOG_LIMIT",
        "canceled/closing native operations remain globally bounded across provider generations");
    quota.peers[0]->finish({ { { "canceled", true }, { "filePaths", QJsonArray {} } }, {}, {} });
    check(limited->start(2, "open", files(), nullptr, collect).code.empty(),
        "only actual native completion releases admission");
    limited->stop();
    before = completions;
    for (auto const& peer : quota.peers)
        if (!peer->finished)
            peer->finish({ { { "canceled", true }, { "filePaths", QJsonArray {} } }, {}, {} });
    check(completions == before, "stop retires every user callback before late native closure");
    auto production = Provider::create();
    auto widgets = QApplication::topLevelWidgets().size();
    check(production->start(1, "message", options, nullptr, collect).code
                == "ERR_FIREFLY_UNSUPPORTED_API"
            && QApplication::topLevelWidgets().size() == widgets,
        "production offscreen backend cannot present fake desktop dialogs");
    production->stop();
    if (!passed)
        return 1;
    puts("PASS exact native dialog registry controlled "
         "admission/completion/parent/cancel/reentry/stop/quota ownership; no native dialog was "
         "opened");
    return 0;
}
