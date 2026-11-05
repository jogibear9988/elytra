#include "ChromeWindow.h"
#include "ToolTip.h"
#include <QLabel>
#include <QApplication>
#include <QMouseEvent>
#include <QEventLoop>
#include <QTimer>
#include <cstdio>
#include <cstdlib>

static void check(bool ok,char const* message){if(!ok){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}}
int main(int argc,char** argv)
{
    QApplication app(argc,argv);
    Firefly::ChromeWindow window(false);QWidget content(&window);content.setGeometry(0,0,640,400);
    window.resize(640,400);window.move(200,200);window.observe(&content);window.show();app.processEvents();
    check(window.windowFlags().testFlag(Qt::FramelessWindowHint),"native frameless flag");
    auto regions=std::vector<Firefly::ChromeWindow::Region>{{QRect(0,0,640,38),true},{QRect(500,0,140,38),false}};
    window.set_regions(regions);
    check(window.draggable({100,20})&&!window.draggable({520,20})&&!window.draggable({100,80}),"drag/no-drag subtraction");
    auto send=[&](QEvent::Type type,QPoint local,QPoint global,Qt::MouseButton button,Qt::MouseButtons buttons){
        QMouseEvent event(type,local,global,button,buttons,Qt::NoModifier);QCoreApplication::sendEvent(&content,&event);
    };
    auto before=window.pos(),global=content.mapToGlobal(QPoint(100,20));
    send(QEvent::MouseButtonPress,{100,20},global,Qt::LeftButton,Qt::LeftButton);
    send(QEvent::MouseMove,{130,40},global+QPoint(30,20),Qt::NoButton,Qt::LeftButton);
    send(QEvent::MouseButtonRelease,{130,40},global+QPoint(30,20),Qt::LeftButton,Qt::NoButton);
    check(window.pos()==before+QPoint(30,20),"native drag movement");
    before=window.pos();global=content.mapToGlobal(QPoint(520,20));
    send(QEvent::MouseButtonPress,{520,20},global,Qt::LeftButton,Qt::LeftButton);
    send(QEvent::MouseMove,{550,40},global+QPoint(30,20),Qt::NoButton,Qt::LeftButton);
    send(QEvent::MouseButtonRelease,{550,40},global+QPoint(30,20),Qt::LeftButton,Qt::NoButton);
    check(window.pos()==before,"no-drag does not move window");
    window.set_regions(regions);global=content.mapToGlobal(QPoint(100,20));
    send(QEvent::MouseButtonDblClick,{100,20},global,Qt::LeftButton,Qt::LeftButton);check(window.isMaximized(),"title double-click maximizes");
    window.set_regions(regions);
    send(QEvent::MouseButtonDblClick,{100,20},global,Qt::LeftButton,Qt::LeftButton);check(!window.isMaximized(),"title double-click restores");
    window.set_regions(regions);QEventLoop wait;QTimer::singleShot(1100,&wait,&QEventLoop::quit);wait.exec();
    check(!window.draggable({100,20}),"stale snapshot cannot keep intercepting input");
    Ladybird::show_tooltip({100,100},QString("<img src='https://invalid.invalid/image'>")+QString(3000,'x'),window,{},10000);
    auto* tip=window.findChild<QLabel*>("firefly-plain-tooltip");
    check(tip&&tip->textFormat()==Qt::PlainText&&tip->text().size()==2048,"untrusted tooltip is bounded plain text");
    Ladybird::hide_tooltip();check(!tip->isVisible(),"tooltip hides");
    std::puts("PASS bounded plain-text tooltip adapter");
    std::puts("PASS native frameless, drag/no-drag input, position change, double-click and stale-region rejection");
}
