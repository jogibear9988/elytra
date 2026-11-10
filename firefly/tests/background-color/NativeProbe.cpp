/** Test-only observer linked with exact production application/view/renderer adapters. */
#include "../../native/canvas/UI.h"
#include "../../native/qt/ChromeWindow.h"
#include "../../native/qt/ContentView.h"
#include "native/platform/GCIncludes.h"
#include <QApplication>
#include <QImage>
#include <QTimer>
#include <UI/Qt/WebContentView.h>
#include <cstdio>
#include <memory>
#include <unordered_map>

namespace {
constexpr auto prefix = "elytra-background-stage-";
constexpr auto acknowledged = "elytra-background-ok-";

struct ObservedFrame {
    unsigned swaps {};
};

auto& observed_frames()
{
    static std::unordered_map<Ladybird::WebContentView*, std::shared_ptr<ObservedFrame>> frames;
    return frames;
}

void observe_frames(Ladybird::WebContentView& view)
{
    auto& frames = observed_frames();
    if (frames.contains(&view))
        return;
    auto frame = std::make_shared<ObservedFrame>();
    frames.emplace(&view, frame);
    auto previous = move(view.on_ready_to_paint);
    view.on_ready_to_paint = [frame, previous = move(previous)] {
        // Upstream invokes this only after accepting/swapping a compositor bitmap.
        // Preserve its native presentation callback rather than replacing behavior.
        ++frame->swaps;
        if (previous)
            previous();
    };

    QObject::connect(&view, &QObject::destroyed, [key = &view] { observed_frames().erase(key); });
}

bool pixel_matches(QImage const& image, unsigned expected)
{
    if (image.isNull() || image.width() < 2 || image.height() < 2)
        return false;
    auto color = image.pixelColor(image.width() / 2, image.height() / 2);
    auto close = [](int actual, unsigned channel) {
        return actual >= static_cast<int>(channel) - 1 && actual <= static_cast<int>(channel) + 1;
    };

    return color.alpha() == 255 && close(color.red(), (expected >> 16) & 255)
        && close(color.green(), (expected >> 8) & 255) && close(color.blue(), expected & 255);
}

unsigned registered_background(Ladybird::WebContentView const& view)
{
    auto color = Firefly::CanvasUI::color(Firefly::content_engine_view(view));
    return color.has_value() ? color->value() & 0xffffff : 0xffffffff;
}

QImage committed_canvas(Ladybird::WebContentView const& view)
{
    // Public upstream preview reads visible_snapshot's real CPU backing-store pixels.
    // QWidget::grab cannot read macOS's separate CALayer and is not a canvas oracle.
    auto preview = view.tab_preview_pixmap(QSize(640, 480));
    return preview.has_value() ? preview->toImage() : QImage {};
}

bool committed_canvas_pixel_matches(Ladybird::WebContentView& view, unsigned expected)
{
    auto frame = observed_frames().find(&view);
    if (frame == observed_frames().end() || frame->second->swaps == 0)
        return false;
    // Require an accepted compositor bitmap and inspect its real CPU canvas pixels.
    // Physical CALayer presentation remains outside this offscreen acceptance gate.
    return pixel_matches(committed_canvas(view), expected);
}

void start_observer()
{
    // Startup registration runs before production's native main entry. Actual observation
    // waits for the application's event loop and never exposes a JavaScript capability.
    QTimer::singleShot(0, QCoreApplication::instance(), [] {
        auto* timer = new QTimer(QCoreApplication::instance());
        QObject::connect(timer, &QTimer::timeout, timer, [] {
            static int waiting_stage = -1;
            static unsigned checks = 0;
            for (auto* widget : QApplication::topLevelWidgets()) {
                auto* window = dynamic_cast<Firefly::ChromeWindow*>(widget);
                if (!window || !window->windowTitle().startsWith("elytra-background-"))
                    continue;
                Ladybird::WebContentView* main = nullptr;
                Ladybird::WebContentView* child = nullptr;
                for (auto* descendant : window->findChildren<QWidget*>()) {
                    auto* view = dynamic_cast<Ladybird::WebContentView*>(descendant);
                    if (!view)
                        continue;
                    observe_frames(*view);
                    if (view->width() == 96 && view->height() == 64)
                        child = view;
                    else if (!main)
                        main = view;
                }

                if (!main)
                    continue;
                if (window->windowTitle() == "elytra-background-arm-child") {
                    if (child && registered_background(*child) == 0x0a2540)
                        window->setWindowTitle("elytra-background-armed-child");
                    continue;
                }

                if (!window->windowTitle().startsWith(prefix))
                    continue;
                bool valid = false;
                int stage = window->windowTitle().mid(QString(prefix).size()).toInt(&valid);
                if (!valid || stage < 0 || stage > 6)
                    continue;
                if (waiting_stage != stage) {
                    waiting_stage = stage;
                    checks = 0;
                }

                ++checks;
                if (checks == 100) {
                    auto image = committed_canvas(*main);
                    auto pixel = image.isNull()
                        ? QColor()
                        : image.pixelColor(image.width() / 2, image.height() / 2);
                    std::fprintf(stderr,
                        "Native background waiting stage %d base=%06x fallback=%08x bitmap=%dx%d "
                        "center=%s child=%d swaps=%u\n",
                        stage, registered_background(*main), main->page_background_color().value(),
                        image.width(), image.height(),
                        pixel.name(QColor::HexArgb).toUtf8().constData(), child != nullptr,
                        observed_frames().at(main)->swaps);
                    std::fflush(stderr);
                }

                constexpr unsigned bases[]
                    = { 0x123456, 0x123456, 0x2468ac, 0x2468ac, 0x4c6e2a, 0x4c6e2a, 0x4c6e2a };
                constexpr unsigned pixels[]
                    = { 0x123456, 0x123456, 0x2468ac, 0x9137d1, 0x9137d1, 0x4c6e2a, 0x4c6e2a };
                if (registered_background(*main) != bases[stage])
                    continue;
                if (stage == 0) {
                    auto fallback = main->page_background_color();
                    if (fallback.red() != 0x12 || fallback.green() != 0x34
                        || fallback.blue() != 0x56)
                        continue;
                } else {
                    // Stored JS/native metadata cannot satisfy committed canvas pixels.
                    if (!committed_canvas_pixel_matches(*main, pixels[stage]))
                        continue;
                }

                if (stage == 6
                    && (!child || registered_background(*child) != 0x0a2540
                        || !committed_canvas_pixel_matches(*child, 0x0a2540)))
                    continue;
                std::printf("PASS native background stage %d actual %s\n", stage,
                    stage == 0 ? "viewport fallback" : "committed CPU canvas pixels");
                std::fflush(stdout);
                window->setWindowTitle(QString(acknowledged) + QString::number(stage));
            }
        });
        timer->start(20);
    });
}

Q_COREAPP_STARTUP_FUNCTION(start_observer)
}
