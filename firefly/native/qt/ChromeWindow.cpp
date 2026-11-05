#include "ChromeWindow.h"
#include <QCloseEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QWindowStateChangeEvent>

namespace Firefly {
ChromeWindow::ChromeWindow(bool frame):m_frame(frame)
{
    if(!frame)setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
}
void ChromeWindow::observe(QWidget* content) { if(!m_frame)content->installEventFilter(this); }
bool ChromeWindow::draggable(QPoint position) const
{
    if(!m_updated.isValid()||m_updated.elapsed()>1000)return false;
    bool drag=false;
    for(auto const& region:m_regions)if(region.bounds.contains(position)) {
        if(!region.drag)return false;
        drag=true;
    }
    return drag;
}
bool ChromeWindow::eventFilter(QObject* object,QEvent* event)
{
    auto* content=qobject_cast<QWidget*>(object);
    if(!content)return false;
    if(event->type()==QEvent::MouseButtonPress || event->type()==QEvent::MouseButtonDblClick) {
        auto* mouse=static_cast<QMouseEvent*>(event);
        auto point=content->mapTo(this,mouse->position().toPoint());
        if(mouse->button()!=Qt::LeftButton || !draggable(point))return false;
        if(event->type()==QEvent::MouseButtonDblClick) {
            isMaximized()?showNormal():showMaximized();return true;
        }
        // Keep the input native, without giving page JS a window-move capability.
        m_dragging=true;m_press=mouse->globalPosition().toPoint();m_origin=pos();
        content->grabMouse();return true;
    }
    if(event->type()==QEvent::MouseMove && m_dragging) {
        auto* mouse=static_cast<QMouseEvent*>(event);
        if(!isMaximized())move(m_origin+mouse->globalPosition().toPoint()-m_press);
        return true;
    }
    if(event->type()==QEvent::MouseButtonRelease && m_dragging) {
        m_dragging=false;content->releaseMouse();return true;
    }
    if(event->type()==QEvent::UngrabMouse)m_dragging=false;
    return false;
}
void ChromeWindow::closeEvent(QCloseEvent* event) {if(request_close)request_close();event->ignore();}
void ChromeWindow::resizeEvent(QResizeEvent* event) {QWidget::resizeEvent(event);if(resized)resized();}
void ChromeWindow::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if(event->type()==QEvent::WindowStateChange && state_changed)
        state_changed(static_cast<QWindowStateChangeEvent*>(event)->oldState(),windowState());
}
}
