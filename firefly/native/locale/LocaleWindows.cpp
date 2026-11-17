/** Read the actual Windows user's regional locale and ordered UI languages. */
#include "Locale.h"
#include <QtCore/qt_windows.h>
#include <iterator>
#include <vector>

namespace Firefly::Locale {
QString system_locale()
{
    wchar_t name[LOCALE_NAME_MAX_LENGTH] {};
    auto length = GetUserDefaultLocaleName(name, LOCALE_NAME_MAX_LENGTH);
    return length > 1 ? QString::fromWCharArray(name, length - 1) : QString {};
}

QString country_code()
{
    wchar_t country[4] {};
    auto length = GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SISO3166CTRYNAME, country,
        static_cast<int>(std::size(country)));
    return length == 3 ? QString::fromWCharArray(country, 2) : QString {};
}

QStringList preferred_languages()
{
    ULONG count = 0;
    ULONG length = 0;
    if (!GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &count, nullptr, &length) || length < 2
        || length > 65536)
        return {};
    std::vector<wchar_t> names(length);
    if (!GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &count, names.data(), &length)
        || length > names.size())
        return {};
    QStringList result;
    size_t offset = 0;
    while (offset < length && names[offset] != L'\0') {
        auto end = offset;
        while (end < length && names[end] != L'\0')
            ++end;
        if (end == length)
            return {};
        result.append(QString::fromWCharArray(names.data() + offset, end - offset));
        offset = end + 1;
    }

    return result.size() == static_cast<qsizetype>(count) ? result : QStringList {};
}
}
