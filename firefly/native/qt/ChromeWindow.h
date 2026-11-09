/**
 * Owns native custom-window chrome and bounded drag/no-drag input handling. Geometry
 * snapshots expire so stale page layout cannot keep intercepting native input.
 */

#pragma once
#include <QElapsedTimer>
#include <QPoint>
#include <QRect>
#include <QWidget>
#include <functional>
#include <vector>

namespace Firefly {
// Window-local hit regions only: no renderer object or callback enters the host VM.
class ChromeWindow final : public QWidget {
public:
    struct Region {
        QRect bounds;
        bool drag;
    };

    explicit ChromeWindow(bool frame = true, bool hidden_title_bar = false);

    bool is_custom_chrome() const
    {
        return !m_frame || m_hidden_title_bar;
    }

    void observe(QWidget* content);

    void set_regions(std::vector<Region> regions)
    {
        m_regions = std::move(regions);

        m_updated.start();
    }

    bool draggable(QPoint position) const;

    std::function<bool()> request_close;
    std::function<void()> resized;
    std::function<void(Qt::WindowStates, Qt::WindowStates)> state_changed;

protected:
    bool eventFilter(QObject*, QEvent*) override;

    void closeEvent(QCloseEvent*) override;

    void resizeEvent(QResizeEvent*) override;

    void changeEvent(QEvent*) override;

private:
    QElapsedTimer m_updated;
    bool m_frame;
    bool m_hidden_title_bar;
    bool m_dragging { false };
    QPoint m_origin, m_press;
    std::vector<Region> m_regions;
};
}
