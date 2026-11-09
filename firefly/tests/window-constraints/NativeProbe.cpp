/** Observe and exercise real Qt constraints in the exact production application objects. */
#include "../../native/qt/ChromeWindow.h"
#include "../../native/window/Geometry.h"
#include <QApplication>
#include <QTimer>
#include <QWindow>
#include <cstdio>

namespace {
void start_constraint_observer()
{
    QTimer::singleShot(0, QCoreApplication::instance(), [] {
        auto* timer = new QTimer(QCoreApplication::instance());
        QObject::connect(timer, &QTimer::timeout, timer, [] {
            for (auto* widget : QApplication::topLevelWidgets()) {
                auto* window = dynamic_cast<Firefly::ChromeWindow*>(widget);
                if (!window || !window->windowTitle().startsWith("elytra-constraints-stage-"))
                    continue;
                bool valid = false;
                auto stage = window->windowTitle()
                                 .mid(QString("elytra-constraints-stage-").size())
                                 .toInt(&valid);
                if (!valid || stage < 0 || stage > 2)
                    continue;
                window->winId();
                auto* native = window->windowHandle();
                auto margins = native->frameMargins();
                QSize frame(margins.left() + margins.right(), margins.top() + margins.bottom());
                constexpr int minimums[][2] = { { 240, 160 }, { 280, 180 }, { 0, 0 } };
                constexpr int maximums[][2] = { { 500, 400 }, { 460, 360 }, { 0, 0 } };
                auto expected_minimum = stage == 2
                    ? QSize(0, 0)
                    : QSize(qMax(0, minimums[stage][0] - frame.width()),
                          qMax(0, minimums[stage][1] - frame.height()));
                auto expected_maximum = stage == 2
                    ? QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX)
                    : QSize(qMax(0, maximums[stage][0] - frame.width()),
                          qMax(0, maximums[stage][1] - frame.height()));
                if (native->minimumSize() != expected_minimum
                    || native->maximumSize() != expected_maximum) {
                    std::fprintf(stderr,
                        "FAIL native constraint stage %d widget min=%dx%d max=%dx%d window "
                        "min=%dx%d max=%dx%d frame=%dx%d\n",
                        stage, window->minimumWidth(), window->minimumHeight(),
                        window->maximumWidth(), window->maximumHeight(), native->minimumWidth(),
                        native->minimumHeight(), native->maximumWidth(), native->maximumHeight(),
                        frame.width(), frame.height());
                    window->setWindowTitle("elytra-constraints-failed");
                    continue;
                }

                auto original = Firefly::window_bounds(*window, false);
                auto requested = original;
                bool passed;
                if (stage < 2) {
                    // Exercise the genuine native adapter without any public JS method.
                    requested.setSize(QSize(1, 1));
                    passed = Firefly::set_window_bounds(*window, requested, false, false)
                        && Firefly::window_bounds(*window, false).size()
                            == QSize(minimums[stage][0], minimums[stage][1]);
                    requested.setSize(QSize(10000, 10000));
                    passed = passed && Firefly::set_window_bounds(*window, requested, false, false)
                        && Firefly::window_bounds(*window, false).size()
                            == QSize(maximums[stage][0], maximums[stage][1]);
                } else {
                    requested.setSize(QSize(701, 503));
                    passed = Firefly::set_window_bounds(*window, requested, false, false)
                        && Firefly::window_bounds(*window, false).size() == QSize(701, 503);
                }

                passed = passed && Firefly::set_window_bounds(*window, original, false, false);
                if (!passed || window->isVisible()) {
                    std::fprintf(stderr, "FAIL native Qt resize enforcement stage %d\n", stage);
                    window->setWindowTitle("elytra-constraints-failed");
                    continue;
                }

                std::printf("PASS native constraints stage %d actual QWindow hints and native "
                            "geometry enforcement\n",
                    stage);
                std::fflush(stdout);
                window->setWindowTitle(QString("elytra-constraints-ok-") + QString::number(stage));
            }
        });
        timer->start(20);
    });
}

Q_COREAPP_STARTUP_FUNCTION(start_constraint_observer)
}
