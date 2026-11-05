#include "ToolTip.h"
#include <QLabel>
#include <QPointer>
#include <QTimer>
#include <algorithm>

namespace Ladybird {
static QPointer<QLabel> tooltip;
static unsigned long long generation=0;
void hide_tooltip(){++generation;if(tooltip)tooltip->hide();}
void show_tooltip(QPoint position,QString const& text,QWidget& owner,QRect,int duration)
{
    if(text.isEmpty()){hide_tooltip();return;}
    if(!tooltip||tooltip->parentWidget()!=&owner){
        delete tooltip.data();tooltip=new QLabel(&owner,Qt::ToolTip);
        tooltip->setObjectName("firefly-plain-tooltip");
        tooltip->setAttribute(Qt::WA_ShowWithoutActivating);
        tooltip->setTextFormat(Qt::PlainText);tooltip->setWordWrap(true);
        tooltip->setMargin(6);tooltip->setMaximumWidth(480);
        tooltip->setMaximumHeight(24*tooltip->fontMetrics().lineSpacing()+12);
        tooltip->setForegroundRole(QPalette::ToolTipText);tooltip->setBackgroundRole(QPalette::ToolTipBase);tooltip->setAutoFillBackground(true);
    }
    tooltip->setText(text.left(2048));tooltip->adjustSize();tooltip->move(position+QPoint(12,18));tooltip->show();
    auto current=++generation;
    QTimer::singleShot(duration>0?std::min(duration,30000):10000,tooltip,[current]{if(current==generation)hide_tooltip();});
}
}
