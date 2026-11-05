#include <LibCore/ArgsParser.h>
#include <LibCore/EventLoop.h>
#include <LibCore/Promise.h>
#include <LibURL/Parser.h>
#include <LibMain/Main.h>
#include <LibWebView/Application.h>
#include <UI/Qt/EventLoopImplementationQt.h>
#include <UI/Qt/WebContentView.h>
#include <QApplication>
#include <QTimer>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QScreen>
#include <QColorSpace>
#include <QSurfaceFormat>
#include <QVBoxLayout>
#include <QWidget>
#include <stdio.h>

template<>
constexpr bool AllocatedWithSystemAllocator<QApplication> = true;

namespace Firefly {
class WindowApplication final : public WebView::Application {
    WEB_VIEW_APPLICATION(WindowApplication)
public:
    WindowApplication() : WebView::Application(ByteString(FIREFLY_LADYBIRD_DIRECTORY)) { }
    ErrorOr<int> run_window()
    {
        if (browser_options().raw_urls.size() != 1 || browser_options().headless_mode.has_value())
            return Error::from_string_literal("Native window probe requires one URL and no headless mode");
        QWidget window;
        window.setWindowTitle("Firefly native window probe");
        window.resize(browser_options().window_width, browser_options().window_height);
        auto* layout = new QVBoxLayout(&window);
        layout->setContentsMargins(0, 0, 0, 0);
        auto* view = new Ladybird::WebContentView(&window);
        layout->addWidget(view);
        view->on_close = [&] { window.close(); };
        view->on_web_content_crashed = [&](auto) { warnln("Firefly WebContent crashed"); m_loop->quit(1); };
        int stage = 0;
        view->on_load_finish = [&](auto const&) {
            outln("WebContent PID: {}", view->web_content_pid());
            fflush(stdout);
            if (!m_self_test) return;
            if (stage == 0) {
                stage = 1;
                QTimer::singleShot(1000, &window, [&] {
                    window.resize(800, 500);
                    view->setFocus();
                    auto click = [&](QPointF position) {
                        auto global = view->mapToGlobal(position.toPoint());
                        QMouseEvent press(QEvent::MouseButtonPress, position, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                        QMouseEvent release(QEvent::MouseButtonRelease, position, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                        QCoreApplication::sendEvent(view, &press);
                        QCoreApplication::sendEvent(view, &release);
                    };
                    click({80, 60});
                    click({80, 140});
                    QKeyEvent press(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, "a");
                    QKeyEvent release(QEvent::KeyRelease, Qt::Key_A, Qt::NoModifier, "a");
                    QCoreApplication::sendEvent(view, &press);
                    QCoreApplication::sendEvent(view, &release);
                    QTimer::singleShot(1000, &window, [&] {
                        view->request_internal_page_info(WebView::PageInfoType::Text)
                            ->when_resolved([&](auto const& text) {
                                if (!text.contains("CLICK_OK"sv) || !text.contains("KEY_OK"sv) || !text.contains("RESIZE_OK"sv)) {
                                    warnln("Native input/resize failed: {}", text); m_loop->quit(1); return;
                                }
                                if (!m_capture.is_empty()) {
                                    auto snapshot = window.screen()->grabWindow(window.winId());
                                    if (snapshot.isNull() || !snapshot.save(QString::fromUtf8(m_capture.characters()))) {
                                        warnln("Native window capture failed"); m_loop->quit(1); return;
                                    }
                                }
                                outln("PASS native Qt mouse/key input and viewport resize");
                                stage = 2;
                                view->load(URL::Parser::basic_parse("window-next.html"sv, browser_options().urls.last()).release_value());
                            }).when_rejected([&](auto const& error) { warnln("Page information: {}", error); m_loop->quit(1); });
                    });
                });
            } else if (stage == 2) {
                stage = 3;
                view->request_internal_page_info(WebView::PageInfoType::Text)
                    ->when_resolved([&](auto const& text) {
                        if (!text.contains("NEXT_NAVIGATION_OK"sv)) { warnln("Navigation failed: {}", text); m_loop->quit(1); return; }
                        outln("PASS native view navigation and window close");
                        m_passed = true;
                        window.close();
                    }).when_rejected([&](auto const& error) { warnln("Page information: {}", error); m_loop->quit(1); });
            }
        };
        window.show();
        view->load(browser_options().urls.last());
        // A probe timeout is a failure, never an inferred successful close.
        QTimer::singleShot(30000, &window, [&] { warnln("Native window probe timed out"); m_loop->quit(1); });
        return m_loop->exec();
    }
private:
    void create_platform_arguments(Core::ArgsParser& parser) override
    {
        parser.add_option(m_self_test, "Exercise the native fixture", "probe-self-test");
        parser.add_option(m_capture, "Capture the native window", "probe-capture", 0, "path");
    }
    void create_platform_options(WebView::BrowserOptions& browser, WebView::RequestServerOptions&, WebView::WebContentOptions&) override
    {
        browser.disable_sql_database = WebView::DisableSQLDatabase::Yes;
        browser.enable_content_blocker = WebView::EnableContentBlocker::No;
    }
    bool should_coordinate_browser_process() const override { return false; }
    Core::EventLoop& create_platform_event_loop() override
    {
        m_qt = make<QApplication>(arguments().argc, arguments().argv);
        Core::EventLoopManager::install(*new Ladybird::EventLoopManagerQt);
        m_loop = &Core::EventLoop::initialize_for_current_thread();
        static_cast<Ladybird::EventLoopImplementationQt&>(m_loop->impl()).set_main_loop();
        QObject::connect(m_qt.ptr(), &QApplication::lastWindowClosed, m_qt.ptr(), [&] { m_loop->quit(m_self_test && !m_passed ? 1 : 0); });
        return *m_loop;
    }
    bool m_self_test { false };
    bool m_passed { false };
    ByteString m_capture;
    OwnPtr<QApplication> m_qt;
    Core::EventLoop* m_loop { nullptr };
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
    for (auto argument : arguments.strings.slice(1)) isolated.append(argument);
    arguments.strings = isolated.span();
    auto application = TRY(Firefly::WindowApplication::create(arguments));
    return application->run_window();
}
