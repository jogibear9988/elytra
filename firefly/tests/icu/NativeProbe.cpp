/** Exercise actual Qt/ICU data through numeric collation; no GUI or locale mutation. */
#include <QCollator>
#include <QCoreApplication>
#include <QLocale>
#include <QString>
#include <cstdio>

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    QCollator collator(QLocale(QLocale::English, QLocale::UnitedStates));
    collator.setNumericMode(true);
    // Qt's uninitialized ICU fallback compares lexically and orders item10
    // before item2. A successful native collator must honor numeric mode.
    if (collator.compare(QStringLiteral("item2"), QStringLiteral("item10")) >= 0
        || collator.compare(QStringLiteral("item10"), QStringLiteral("item2")) <= 0) {
        std::fprintf(stderr, "FAIL Qt/ICU numeric collation with actual external data\n");
        return 1;
    }

    std::puts("PASS Qt/ICU numeric collation with actual external data");
    return 0;
}
