/** Native-only controlled grant component against an actual owned renderer document.
 * No Web permission/device API is called and no OS authorization is requested.
 */
#include "native/permissions/Hooks.h"
#include "native/platform/GCIncludes.h"
#include "native/qt/hub/Frontend.h"
#include <LibIPC/FireflyBootstrap.h>
#include <LibWebView/CanonicalDocument.h>
#include <LibWebView/CanonicalTraversable.h>
#include <LibWebView/WebContentClient.h>
#include <LibWebView/WebContentPage.h>
#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <UI/Qt/WebContentView.h>
#include <memory>

namespace {
struct ProbeState {
    int stage {};
    int attempts {};
    Firefly::Permissions::DocumentId document {};
    bool decided {};
    bool granted {};
};

void begin_boundary_probe()
{
    auto report = qEnvironmentVariable("ELYTRA_PERMISSION_BOUNDARY_REPORT");
    if (report.isEmpty())
        return;
    auto* timer = new QTimer(QCoreApplication::instance());
    timer->setInterval(10);
    auto state = std::make_shared<ProbeState>();
    QObject::connect(timer, &QTimer::timeout, timer, [timer, report, state] {
        auto write = [&](bool passed, QString stage, QString message, int cases, bool terminal) {
            if (terminal)
                timer->stop();
            QFile file(report);
            if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                QJsonObject result { { "passed", passed }, { "stage", stage },
                    { "message", message }, { "cases", cases } };
                file.write(QJsonDocument(result).toJson(QJsonDocument::Compact));
                file.close();
            }
        };

        auto fail = [&](QString message) { write(false, "failed", message, 0, true); };
        if (++state->attempts > 3000) {
            fail("Actual owned document/policy was not ready");
            return;
        }

        auto policy = Firefly::Permissions::policy();
        if (policy.epoch < 2 || !policy.check_enabled)
            return;
        for (auto* widget : QApplication::allWidgets()) {
            auto* view = qobject_cast<Ladybird::WebContentView*>(widget);
            if (!view)
                continue;
            auto canonical = Firefly::HubFrontend::canonical_view(*view);
            if (!canonical.has_value() || !canonical->has_traversable())
                continue;
            auto& page = canonical->page();
            if (!page.is_open())
                continue;
            auto serialized = page.traversable().active_document_url().serialize().to_byte_string();
            if (!QByteArray(serialized.characters(), static_cast<qsizetype>(serialized.length()))
                    .endsWith("/boundary.html"))
                continue;
            auto doc = page.traversable().active_document().id();
            auto& stub = static_cast<WebContentClientStub&>(page.client());
            auto query = [&](u64 page_id, u64 ns, u64 local, u64 epoch, String const& name) {
                return Firefly::Bytes::accept_permission_check(
                    stub, page_id, ns, local, epoch, name);
            };

            auto page_id = page.id().value();
            using Name = Firefly::Permissions::Name;
            if (state->stage == 0) {
                if (policy.epoch != 2) {
                    fail("Identity stage generation changed unexpectedly");
                    return;
                }

                bool stale
                    = query(page_id, doc.namespace_id, doc.local_id, 1, "geolocation"_string);
                bool wrong_doc = query(
                    page_id, doc.namespace_id, 0xffffffffffffffffull, 2, "geolocation"_string);
                bool wrong_ns
                    = query(page_id, 0xffffffffffffffffull, doc.local_id, 2, "geolocation"_string);
                bool wrong_page = query(
                    0xffffffffffffffffull, doc.namespace_id, doc.local_id, 2, "geolocation"_string);
                bool wrong_name
                    = query(page_id, doc.namespace_id, doc.local_id, 2, "unknown"_string);
                bool zero = query(page_id, doc.namespace_id, doc.local_id, 0, "geolocation"_string);
                bool valid
                    = query(page_id, doc.namespace_id, doc.local_id, 2, "geolocation"_string);
                if (!valid || stale || wrong_doc || wrong_ns || wrong_page || wrong_name || zero) {
                    fail("Forged/native identity checks failed");
                    return;
                }

                state->document = doc;
                state->stage = 1;
                write(true, "identity", "Exact native identity boundaries passed", 7, false);
                return;
            }

            if (doc != state->document) {
                fail("Grant component document was replaced");
                return;
            }

            if (state->stage == 1) {
                if (policy.epoch != 3 || !policy.request_enabled)
                    return;
                state->stage = 2;
                Firefly::Permissions::request(page, doc, Name::Geolocation, [state](auto decision) {
                    state->decided = true;
                    state->granted = decision == Web::Permissions::EffectiveState::Granted;
                });
                return;
            }

            if (state->stage == 2) {
                if (!state->decided)
                    return;
                bool actual = Firefly::Permissions::may_use(page, doc, Name::Geolocation);
                bool forged = Firefly::Permissions::may_use(
                    page, { doc.namespace_id, 0xffffffffffffffffull }, Name::Geolocation);
                if (!state->granted || !actual || forged) {
                    fail("Controlled native exact-document grant failed");
                    return;
                }

                state->stage = 3;
                write(true, "granted", "Production broker granted only the owned exact document",
                    10, false);
                return;
            }

            if (state->stage == 3) {
                if (policy.epoch != 4)
                    return;
                bool retained = Firefly::Permissions::may_use(page, doc, Name::Geolocation);
                bool stale
                    = query(page_id, doc.namespace_id, doc.local_id, 3, "geolocation"_string);
                if (retained || stale) {
                    fail("Handler replacement retained a grant or accepted an old generation");
                    return;
                }

                write(true, "done",
                    "Native identity and controlled grant/revocation component passed", 12, true);
                return;
            }
        }
    });
    timer->start();
}
}

Q_COREAPP_STARTUP_FUNCTION(begin_boundary_probe)
