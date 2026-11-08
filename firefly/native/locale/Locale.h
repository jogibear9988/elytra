/** Main-process OS regional settings, independent of application/renderer language overrides. */
#pragma once
#include <QString>
#include <QStringList>

namespace Firefly::Locale {
QString system_locale();

QString country_code();

QStringList preferred_languages();
}
