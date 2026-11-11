/** Exact application objects: provider admission only, without spelling RPC or OS dictionary
 * access. */
#include "../../native/qt/ChromeWindow.h"
#include <LibWebView/Application.h>
#include <LibWebView/SpellChecker.h>
#include <QApplication>
#include <QTimer>
#include <UI/Qt/WebContentView.h>
#include <cstdio>
#include <memory>

namespace {
struct State {
    int windows {};
    int views {};
    unsigned provider_calls {};
    bool initialized {};
};

class ProbeChecker final : public WebView::SpellChecker {
public:
    explicit ProbeChecker(std::shared_ptr<State> state)
        : m_state(move(state))
    {
    }

    DocumentTag create_document_tag() override
    {
        ++m_state->provider_calls;
        return 1;
    }

    void close_document_tag(DocumentTag) override
    {
        ++m_state->provider_calls;
    }

    void check(
        DocumentTag, Utf16String const&, Function<void(Vector<Web::SpellingError>)> done) override
    {
        ++m_state->provider_calls;
        done({});
    }

    void guesses(DocumentTag, Utf16String const&, Function<void(Vector<Utf16String>)> done) override
    {
        ++m_state->provider_calls;
        done({});
    }

    void ignore(DocumentTag, Utf16String const&) override
    {
        ++m_state->provider_calls;
    }

    void learn(DocumentTag, Utf16String const&) override
    {
        ++m_state->provider_calls;
    }

private:
    std::shared_ptr<State> m_state;
};

void start_observer()
{
    QTimer::singleShot(0, QCoreApplication::instance(), [] {
        auto state = std::make_shared<State>();
        auto* timer = new QTimer(QCoreApplication::instance());
        QObject::connect(timer, &QTimer::timeout, timer, [state] {
            int windows = 0;
            int views = 0;
            bool visible = false;
            Firefly::ChromeWindow* control = nullptr;
            for (auto* widget : QApplication::allWidgets()) {
                if (dynamic_cast<Ladybird::WebContentView*>(widget))
                    ++views;
                if (auto* window = dynamic_cast<Firefly::ChromeWindow*>(widget)) {
                    ++windows;
                    visible |= window->isVisible();
                    if (window->windowTitle().startsWith("elytra-spellcheck-"))
                        control = window;
                }
            }

            if (!control)
                return;
            auto& app = WebView::Application::the();
            auto title = control->windowTitle();
            auto fail = [&](char const* message) {
                std::fprintf(stderr, "Native spellcheck probe: %s\n", message);
                std::fflush(stderr);
                app.set_spell_checker({});
                control->setWindowTitle("elytra-spellcheck-failed");
            };

            if (title == "elytra-spellcheck-baseline") {
                if (visible || app.spell_checker() || windows != 1 || views != 2) {
                    fail("Initial hidden/null-provider counts differ");
                    return;
                }

                state->windows = windows;
                state->views = views;
                state->initialized = true;
                control->setWindowTitle("elytra-spellcheck-baseline-ok");
            } else if (title == "elytra-spellcheck-install" && state->initialized) {
                if (app.spell_checker()) {
                    fail("Unexpected preexisting provider");
                    return;
                }

                app.set_spell_checker(adopt_own(*new ProbeChecker(state)));
                control->setWindowTitle("elytra-spellcheck-install-ok");
            } else if (title == "elytra-spellcheck-blocked" && state->initialized) {
                if (!dynamic_cast<ProbeChecker*>(app.spell_checker()) || visible
                    || windows != state->windows || views != state->views
                    || state->provider_calls) {
                    fail("Rejected factories allocated widgets or changed native provider");
                    return;
                }

                std::puts(
                    "PASS native spellcheck real provider rejects factories without allocation");
                std::fflush(stdout);
                control->setWindowTitle("elytra-spellcheck-blocked-ok");
            } else if (title == "elytra-spellcheck-reset" && state->initialized) {
                app.set_spell_checker({});
                control->setWindowTitle("elytra-spellcheck-reset-ok");
            } else if (title == "elytra-spellcheck-restored" && state->initialized) {
                if (app.spell_checker() || visible || windows != state->windows + 1
                    || views != state->views + 2 || state->provider_calls) {
                    fail("Null-provider factories did not recover exact hidden counts");
                    return;
                }

                std::puts("PASS native spellcheck null provider restores hidden window and view "
                          "admission");
                std::fflush(stdout);
                control->setWindowTitle("elytra-spellcheck-restored-ok");
            }
        });
        timer->start(10);
    });
}

Q_COREAPP_STARTUP_FUNCTION(start_observer)
}
