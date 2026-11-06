/**
 * Renders bounded plain-text native tooltips. Application/page text must not be interpreted
 * as rich markup or a privileged script surface.
 */

#pragma once
#include <QPoint>
#include <QRect>
#include <QString>
class QWidget;

namespace Ladybird {
// Match upstream PlainText.h's embedding entry points without linking browser tabs.
void show_tooltip(QPoint, QString const&, QWidget&, QRect, int);

void hide_tooltip();
}
