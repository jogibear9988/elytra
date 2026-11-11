/** Exercise real file input through a hidden native Qt view and owned renderer. */
#include "native/file-picker/Picker.h"
#include "native/qt/hub/Frontend.h"
#include "native/qt/hub/Host.h"
#include <LibCore/ArgsParser.h>
#include <LibCore/EventLoop.h>
#include <LibMain/Main.h>
#include <LibURL/Parser.h>
#include <LibWebView/Application.h>
#include <QApplication>
#include <QFileDialog>
#include <QMouseEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <UI/Qt/EventLoopImplementationQt.h>
#include <UI/Qt/WebContentView.h>
#include <stdio.h>

template <> constexpr bool AllocatedWithSystemAllocator<QApplication> = true;

namespace Firefly {
class PickerApplication final {
    AK_ALLOC_WITH_KMALLOC;

public:
    static ErrorOr<OwnPtr<PickerApplication>> create(Main::Arguments arguments)
    {
        auto application = make<PickerApplication>();
        application->m_arguments.append(arguments.strings.first());
        application->m_arguments.append("--disable-sql-database"sv);
        application->m_arguments.append("--disable-content-blocker"sv);
        for (size_t index = 1; index < arguments.strings.size(); ++index) {
            auto argument = arguments.strings[index];
            if (argument == "--probe-file"sv) {
                if (++index == arguments.strings.size())
                    return Error::from_string_literal("Missing owned probe argument");
                application->m_file = ByteString(arguments.strings[index]);
                continue;
            }

            if (argument.starts_with("--probe-file="sv)) {
                application->m_file = ByteString(argument.substring_view(13));
                continue;
            }

            if (argument == "--probe-next"sv) {
                if (++index == arguments.strings.size())
                    return Error::from_string_literal("Missing owned probe argument");
                application->m_next = ByteString(arguments.strings[index]);
                continue;
            }

            if (argument.starts_with("--probe-next="sv)) {
                application->m_next = ByteString(argument.substring_view(13));
                continue;
            }

            application->m_arguments.append(argument);
        }

        application->m_argc = arguments.argc;
        application->m_qt = make<QApplication>(application->m_argc, arguments.argv);
        application->m_qt->setQuitOnLastWindowClosed(false);
        Core::EventLoopManager::install(*new Ladybird::EventLoopManagerQt);
        application->m_loop = &Core::EventLoop::initialize_for_current_thread();
        static_cast<Ladybird::EventLoopImplementationQt&>(application->m_loop->impl())
            .set_main_loop();

        application->m_hub = make<HubHost>(
            *application->m_loop, ByteString(FIREFLY_LADYBIRD_DIRECTORY),
            [] { HubFrontend::initialize(); },
            [] {
                // run_picker's stack-owned picker/widgets retire before this owner.
                HubFrontend::shutdown();
            });
        arguments.strings = application->m_arguments.span();
        TRY(application->m_hub->initialize(arguments));
        return application;
    }

    ErrorOr<int> run_picker()
    {
        if (browser_options().raw_urls.size() != 1 || m_file.is_empty() || m_next.is_empty())
            return Error::from_string_literal(
                "Picker probe needs URL, --probe-file and --probe-next");
        QWidget window;
        window.setAttribute(Qt::WA_DontShowOnScreen);
        window.resize(640, 400);
        auto* layout = new QVBoxLayout(&window);
        layout->setContentsMargins(0, 0, 0, 0);
        auto* view = new Ladybird::WebContentView(&window);
        layout->addWidget(view);
        int stage = 0, requests = 0;
        FilePicker picker(
            window, [view](auto files) { view->file_picker_closed(move(files)); },
            [&](QWidget* parent) -> QFileDialog* {
                auto* dialog = new QFileDialog(parent);
                dialog->setOption(QFileDialog::DontUseNativeDialog);
                dialog->setAttribute(Qt::WA_DontShowOnScreen);
                ++requests;
                QTimer::singleShot(20, dialog, [&, dialog = QPointer<QFileDialog> { dialog }] {
                    if (!dialog)
                        return;
                    if (stage == 0) {
                        dialog->selectFile(QString::fromUtf8(m_file.characters(), m_file.length()));
                        QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
                    } else if (stage == 1)
                        dialog->reject();
                    else if (stage == 2) {
                        stage = 3;
                        auto next = URL::Parser::basic_parse(m_next);
                        if (!next.has_value()) {
                            m_loop->quit(1);
                            return;
                        }

                        view->load(*next);
                    }
                });
                return dialog;
            });
        view->on_request_file_picker
            = [&](auto const& filters, auto multiple) { picker.request(filters, multiple); };
        view->on_load_start = [&] { picker.cancel(); };
        view->on_web_content_crashed = [&](auto) {
            warnln("File picker helper crashed");
            m_loop->quit(1);
        };

        auto click = [&] {
            QTimer::singleShot(50, &window, [&] {
                QPointF position(90, 40);
                auto global = view->mapToGlobal(position.toPoint());
                QMouseEvent press(QEvent::MouseButtonPress, position, global, Qt::LeftButton,
                    Qt::LeftButton, Qt::NoModifier);
                QMouseEvent release(QEvent::MouseButtonRelease, position, global, Qt::LeftButton,
                    Qt::NoButton, Qt::NoModifier);
                QCoreApplication::sendEvent(view, &press);
                QCoreApplication::sendEvent(view, &release);
            });
        };

        view->on_title_change = [&](auto const& title) {
            auto text = title.to_byte_string();
            if (text == "PASS_SELECTED"sv && stage == 0) {
                stage = 1;
                click();
            } else if (text == "PASS_CANCELLED"sv && stage == 1) {
                stage = 2;
                click();
            } else if (text == "PASS_NEW_DOCUMENT"sv && stage == 3 && requests == 3) {
                outln("PASS hidden native file input: trusted input/Qt accept/SelectedFile "
                      "IPC/File text/basename/cancel/navigation");
                m_loop->quit(0);
            } else if (text.starts_with("FAIL"sv)) {
                warnln("{}", text);
                m_loop->quit(1);
            }
        };

        view->on_load_finish = [&](auto const&) {
            if (stage == 0)
                click();
        };

        window.show(); // DontShowOnScreen dispatches widget geometry without presenting desktop UI.
        view->load(browser_options().urls.last());
        QTimer::singleShot(15000, &window, [&] {
            warnln("Picker helper timed out: stage {} requests {}", stage, requests);
            m_loop->quit(1);
        });
        return m_loop->exec();
    }

private:
    static WebView::BrowserOptions const& browser_options()
    {
        return WebView::Application::browser_options();
    }

    int m_argc {}; // QApplication borrows this until its own destruction.
    OwnPtr<QApplication> m_qt;
    Core::EventLoop* m_loop { nullptr };
    ByteString m_file, m_next;
    Vector<StringView> m_arguments;
    OwnPtr<HubHost> m_hub;
};
}

ErrorOr<int> ladybird_main(Main::Arguments arguments)
{
    QCoreApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
    Vector<StringView> isolated;
    isolated.append(arguments.strings.first());
    isolated.append("--temporary-profile"sv);
    for (auto value : arguments.strings.slice(1))
        isolated.append(value);
    arguments.strings = isolated.span();
    auto app = TRY(Firefly::PickerApplication::create(arguments));
    return app->run_picker();
}
