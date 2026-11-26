/** Route trusted input and hidden Qt choices through the full production Firefly application. */
#include "native/qt/hub/Frontend.h"
#include <QApplication>
#include <QFileDialog>
#include <QMouseEvent>
#include <QPointer>
#include <QTimer>
#include <UI/Qt/WebContentView.h>

namespace {
class Fixture final : public QObject {
public:
    explicit Fixture(QObject* parent)
        : QObject(parent)
    {
        auto* timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this] { click_ready_view(); });
        timer->start(20);
    }

    bool eventFilter(QObject* object, QEvent* event) override
    {
        if (event->type() == QEvent::Polish) {
            if (auto* widget = qobject_cast<QWidget*>(object); widget && widget->isWindow())
                widget->setAttribute(Qt::WA_DontShowOnScreen);
        }

        if (event->type() != QEvent::Show)
            return false;
        auto* dialog = qobject_cast<QFileDialog*>(object);
        if (!dialog || dialog->property("fileAccessProbeHandled").toBool())
            return false;
        dialog->setProperty("fileAccessProbeHandled", true);
        QTimer::singleShot(0, dialog, [dialog = QPointer { dialog }, index = ++m_dialogs] {
            if (!dialog)
                return;
            switch (index) {
            case 1:
                dialog->selectFile(qEnvironmentVariable("ELYTRA_TEST_OPEN_FILE"));
                break;
            case 2:
                dialog->selectFile(qEnvironmentVariable("ELYTRA_TEST_OPEN_DIRECTORY"));
                break;
            case 3:
                dialog->selectFile(qEnvironmentVariable("ELYTRA_TEST_SAVE_FILE"));
                break;
            default:
                dialog->reject();
                return;
            }

            QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
        });
        return false;
    }

private:
    void click_ready_view()
    {
        for (auto* widget : QApplication::allWidgets()) {
            auto* view = qobject_cast<Ladybird::WebContentView*>(widget);
            if (!view)
                continue;
            auto canonical = Firefly::HubFrontend::canonical_view(*view);
            if (!canonical.has_value())
                continue;
            auto title = canonical->title().to_byte_string();
            if (!title.starts_with("PICKER_READY_"sv) || title == m_last_title)
                continue;

            m_last_title = move(title);
            QPointF position(90, 40);
            auto global = view->mapToGlobal(position.toPoint());
            QMouseEvent press(QEvent::MouseButtonPress, position, global, Qt::LeftButton,
                Qt::LeftButton, Qt::NoModifier);
            QMouseEvent release(QEvent::MouseButtonRelease, position, global, Qt::LeftButton,
                Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(view, &press);
            QCoreApplication::sendEvent(view, &release);
        }
    }

    ByteString m_last_title;
    unsigned m_dialogs { 0 };
};

void begin_file_access_probe()
{
    if (!qEnvironmentVariableIsSet("ELYTRA_TEST_OPEN_FILE"))
        return;
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    auto* application = QCoreApplication::instance();
    application->installEventFilter(new Fixture(application));
}
}

Q_COREAPP_STARTUP_FUNCTION(begin_file_access_probe)
