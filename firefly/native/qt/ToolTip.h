#pragma once
#include <QPoint>
#include <QRect>
#include <QString>
class QWidget;
namespace Ladybird {
// Match upstream PlainText.h's embedding entry points without linking browser tabs.
void show_tooltip(QPoint,QString const&,QWidget&,QRect,int);
void hide_tooltip();
}
