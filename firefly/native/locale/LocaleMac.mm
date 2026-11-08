/** Read Foundation directly: Qt's UI-language expansion changes the user's ordered language list.
 */
#include "Locale.h"
#import <Foundation/Foundation.h>

namespace Firefly::Locale {
namespace {
    QString text(NSString* value)
    {
        return value ? QString::fromUtf8([value UTF8String]) : QString {};
    }
}

QString system_locale()
{
    @autoreleasepool {
        auto* identifier = [[NSLocale currentLocale] localeIdentifier];
        // Language identifiers use BCP 47 dashes; regional preferences stay
        // independent of --lang and of WebView's persisted Accept-Language list.
        return text([NSLocale canonicalLanguageIdentifierFromString:identifier]);
    }
}

QString country_code()
{
    @autoreleasepool {
        auto result = text([[NSLocale currentLocale] objectForKey:NSLocaleCountryCode]);
        return result.size() == 2 ? result : QString {};
    }
}

QStringList preferred_languages()
{
    @autoreleasepool {
        QStringList result;
        for (NSString* language in [NSLocale preferredLanguages])
            result.append(text(language));
        return result;
    }
}
}
