/** Native Qt locale backend; UI-language fallbacks differ from Electron's GLib list. */
#include "Locale.h"
#include <QLocale>

namespace Firefly::Locale {
QString system_locale()
{
    auto locale = QLocale::system();
    // POSIX has no regional language preference; retain its explicit identity
    // as a well-formed locale rather than inventing a configured UI language.
    if (locale.language() == QLocale::C)
        return QStringLiteral("en-US-POSIX");
    return locale.name(QLocale::TagSeparator::Dash);
}

QString country_code()
{
    auto locale = QLocale::system();
    if (locale.language() == QLocale::C)
        return {};
    auto result = QLocale::territoryToCode(locale.territory());
    return result.size() == 2 ? result : QString {};
}

QStringList preferred_languages()
{
    auto locale = QLocale::system();
    if (locale.language() == QLocale::C)
        return {};
    return locale.uiLanguages();
}
}
