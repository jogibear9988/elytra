/** Drive a real Ladybird Qt view for native input, painting, navigation and renderer-loss
 * acceptance. */
#include "hub/Frontend.h"
#include "hub/Host.h"
#include <LibCore/ArgsParser.h>
#include <LibCore/EventLoop.h>
#include <LibCore/Promise.h>
#include <LibMain/Main.h>
#include <LibURL/Parser.h>
#include <LibWebView/Application.h>
#include <QApplication>
#include <QColorSpace>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScreen>
#include <QSurfaceFormat>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <UI/Qt/EventLoopImplementationQt.h>
#include <UI/Qt/WebContentView.h>
#include <stdio.h>

template <> constexpr bool AllocatedWithSystemAllocator<QApplication> = true;

namespace Firefly {
class WindowApplication final {
    AK_ALLOC_WITH_KMALLOC;

public:
    static ErrorOr<OwnPtr<WindowApplication>> create(Main::Arguments arguments)
    {
        auto application = make<WindowApplication>();
        application->m_arguments.append(arguments.strings.first());
        application->m_arguments.append("--disable-sql-database"sv);
        application->m_arguments.append("--disable-content-blocker"sv);
        for (size_t index = 1; index < arguments.strings.size(); ++index) {
            auto argument = arguments.strings[index];
            if (argument == "--probe-self-test"sv) {
                application->m_self_test = true;
                continue;
            }

            if (argument == "--probe-capture"sv) {
                if (++index == arguments.strings.size())
                    return Error::from_string_literal("Missing owned probe argument");
                application->m_capture = ByteString(arguments.strings[index]);
                continue;
            }

            if (argument.starts_with("--probe-capture="sv)) {
                application->m_capture = ByteString(argument.substring_view(16));
                continue;
            }

            application->m_arguments.append(argument);
        }

        application->m_argc = arguments.argc;
        application->m_qt = make<QApplication>(application->m_argc, arguments.argv);

        Core::EventLoopManager::install(*new Ladybird::EventLoopManagerQt);
        application->m_loop = &Core::EventLoop::initialize_for_current_thread();
        static_cast<Ladybird::EventLoopImplementationQt&>(application->m_loop->impl())
            .set_main_loop();
        QObject::connect(application->m_qt.ptr(), &QApplication::lastWindowClosed,
            application->m_qt.ptr(), [owner = application.ptr()] {
                owner->m_loop->quit(owner->m_self_test && !owner->m_passed ? 1 : 0);
            });
        application->m_hub = make<HubHost>(
            *application->m_loop, ByteString(FIREFLY_LADYBIRD_DIRECTORY),
            [] { HubFrontend::initialize(); },
            [] {
                // run_window's stack-owned widgets retire before this owner.
                HubFrontend::shutdown();
            });
        arguments.strings = application->m_arguments.span();
        TRY(application->m_hub->initialize(arguments));
        return application;
    }

    ErrorOr<int> run_window()
    {
        if (browser_options().raw_urls.size() != 1 || browser_options().headless_mode.has_value())
            return Error::from_string_literal(
                "Native window probe requires one URL and no headless mode");
        outln("Native Qt platform: {}", QApplication::platformName().toUtf8().constData());
        QWidget window;
        window.setWindowTitle("Firefly native window probe");
        window.resize(browser_options().window_width, browser_options().window_height);
        auto* layout = new QVBoxLayout(&window);
        layout->setContentsMargins(0, 0, 0, 0);
        auto* view = new Ladybird::WebContentView(&window);
        layout->addWidget(view);
        view->on_close = [&] { window.close(); };
        view->on_web_content_crashed = [&](auto) {
            warnln("Firefly WebContent crashed");
            m_loop->quit(1);
        };

        int stage = 0;
        view->on_load_finish = [&](auto const&) {
            outln("WebContent PID: {}", view->web_content_pid());
            fflush(stdout);
            if (!m_self_test)
                return;
            if (stage == 0) {
                stage = 1;
                QTimer::singleShot(1000, &window, [&] {
                    window.resize(800, 500);
                    view->setFocus();
                    auto click = [&](QPointF position) {
                        auto global = view->mapToGlobal(position.toPoint());
                        QMouseEvent press(QEvent::MouseButtonPress, position, global,
                            Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                        QMouseEvent release(QEvent::MouseButtonRelease, position, global,
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                        QCoreApplication::sendEvent(view, &press);
                        QCoreApplication::sendEvent(view, &release);
                    };

                    click({ 80, 60 });
                    click({ 80, 140 });
                    QKeyEvent press(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, "a");
                    QKeyEvent release(QEvent::KeyRelease, Qt::Key_A, Qt::NoModifier, "a");
                    QCoreApplication::sendEvent(view, &press);
                    QCoreApplication::sendEvent(view, &release);
                    QTimer::singleShot(1000, &window, [&] {
                        if (!HubFrontend::canonical_view(*view).has_value()) {
                            warnln("Native Hub view retired before input result query");
                            m_loop->quit(1);
                            return;
                        }

                        HubFrontend::canonical_view(*view)
                            ->request_internal_page_info(WebView::PageInfoType::Text)
                            ->when_resolved([&](auto const& text) {
                                if (!text.contains("CLICK_OK"sv) || !text.contains("KEY_OK"sv)
                                    || !text.contains("RESIZE_OK"sv)) {
                                    warnln("Native input/resize failed: {}", text);
                                    m_loop->quit(1);
                                    return;
                                }

                                if (!m_capture.is_empty()) {
                                    auto snapshot = window.screen()->grabWindow(window.winId());
                                    if (snapshot.isNull()
                                        || !snapshot.save(
                                            QString::fromUtf8(m_capture.characters()))) {
                                        warnln("Native window capture failed");
                                        m_loop->quit(1);
                                        return;
                                    }

                                    // A valid PNG can still be an empty/native background. These
                                    // page-owned swatches prove actual compositor presentation.
                                    auto image = snapshot.toImage();
                                    auto sample = [&](QPoint point) {
                                        auto pixel = point * snapshot.devicePixelRatio();
                                        return image.rect().contains(pixel)
                                            ? image.pixelColor(pixel)
                                            : QColor();
                                    };

                                    auto pink = sample({ 430, 350 });
                                    auto teal = sample({ 530, 350 });
                                    // Native display profiles can transform exact RGB values.
                                    // Require both deliberately contrasting channel signatures;
                                    // neutral background/blank/monochrome captures cannot pass.
                                    if (!pink.isValid() || !teal.isValid() || pink.red() < 128
                                        || teal.green() < 112 || teal.blue() < 112
                                        || pink.red() - pink.green() < 64
                                        || pink.red() - pink.blue() < 48
                                        || teal.green() - teal.red() < 48
                                        || teal.blue() - teal.red() < 48) {
                                        warnln("Native window capture lacks rendered page colors");
                                        m_loop->quit(1);
                                        return;
                                    }

                                    outln(
                                        "PASS native window capture contains rendered page colors");
                                }

                                outln("PASS native Qt mouse/key input and viewport resize");
                                stage = 2;
                                view->load(URL::Parser::basic_parse(
                                    "window-next.html"sv, browser_options().urls.last())
                                        .release_value());
                            })
                            .when_rejected([&](auto const& error) {
                                warnln("Page information: {}", error);
                                m_loop->quit(1);
                            });
                    });
                });
            } else if (stage == 2) {
                stage = 3;
                if (!HubFrontend::canonical_view(*view).has_value()) {
                    warnln("Native Hub view retired before navigation result query");
                    m_loop->quit(1);
                    return;
                }

                HubFrontend::canonical_view(*view)
                    ->request_internal_page_info(WebView::PageInfoType::Text)
                    ->when_resolved([&](auto const& text) {
                        if (!text.contains("NEXT_NAVIGATION_OK"sv)) {
                            warnln("Navigation failed: {}", text);
                            m_loop->quit(1);
                            return;
                        }

                        outln("PASS native view navigation and window close");
                        m_passed = true;
                        window.close();
                    })
                    .when_rejected([&](auto const& error) {
                        warnln("Page information: {}", error);
                        m_loop->quit(1);
                    });
            }
        };

        window.show();
        view->load(browser_options().urls.last());
        // A probe timeout is a failure, never an inferred successful close.
        QTimer::singleShot(30000, &window, [&] {
            warnln("Native window probe timed out");
            m_loop->quit(1);
        });
        return m_loop->exec();
    }

private:
    static WebView::BrowserOptions const& browser_options()
    {
        return WebView::Application::browser_options();
    }

    bool m_self_test { false };
    bool m_passed { false };
    ByteString m_capture;
    int m_argc {}; // QApplication borrows this until its own destruction.
    OwnPtr<QApplication> m_qt;
    Core::EventLoop* m_loop { nullptr };
    Vector<StringView> m_arguments;
    OwnPtr<HubHost> m_hub;
};
}

ErrorOr<int> ladybird_main(Main::Arguments arguments)
{
    QCoreApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
    auto surface_format = QSurfaceFormat::defaultFormat();
    surface_format.setColorSpace(QColorSpace::SRgb);
    QSurfaceFormat::setDefaultFormat(surface_format);
    Vector<StringView> isolated;
    isolated.append(arguments.strings.first());
    isolated.append("--temporary-profile"sv);
    for (auto argument : arguments.strings.slice(1))
        isolated.append(argument);
    arguments.strings = isolated.span();
    auto application = TRY(Firefly::WindowApplication::create(arguments));
    return application->run_window();
}
