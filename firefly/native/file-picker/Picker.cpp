/** Own one nonmodal chooser per view and retire it before navigation/crash/destruction. */
#include "Picker.h"
#include <QFileDialog>
#include <QMimeDatabase>
#include <QWidget>

namespace Firefly {
namespace {
    QString text(Utf16String const& value)
    {
        auto bytes = value.to_byte_string();
        return QString::fromUtf8(bytes.characters(), bytes.length());
    }

    QString filters(Web::HTML::FileFilter const& accepted)
    {
        QStringList patterns;
        QMimeDatabase database;
        size_t inspected = 0;
        for (auto const& filter : accepted.filters) {
            if (++inspected > 64 || patterns.size() >= 64)
                break;
            filter.visit(
                [&](Web::HTML::FileFilter::Extension const& item) {
                    if (item.value.length_in_code_units() <= 256)
                        patterns.append("*." + text(item.value));
                },
                [&](Web::HTML::FileFilter::MimeType const& item) {
                    if (item.value.length_in_code_units() <= 256) {
                        auto mime = database.mimeTypeForName(text(item.value));
                        if (mime.isValid())
                            patterns.append(mime.globPatterns());
                    }
                },
                [&](Web::HTML::FileFilter::FileType item) {
                    auto prefix = item == Web::HTML::FileFilter::FileType::Audio ? "audio/"
                        : item == Web::HTML::FileFilter::FileType::Image         ? "image/"
                                                                                 : "video/";
                    for (auto const& mime : database.allMimeTypes())
                        if (mime.name().startsWith(prefix) && patterns.size() < 64)
                            patterns.append(mime.globPatterns());
                });
        }

        if (patterns.isEmpty())
            return "All files (*)";
        return "Accepted files (" + patterns.mid(0, 64).join(' ') + ");;All files (*)";
    }
}

FilePicker::FilePicker(QWidget& parent, Completion complete, Factory factory)
    : QObject(&parent)
    , m_parent(parent)
    , m_complete(move(complete))
    , m_factory(move(factory))
{
}

FilePicker::~FilePicker()
{
    m_complete = {};
    cancel();
}

void FilePicker::cancel()
{
    auto dialog = m_dialog;
    m_dialog = nullptr; // Invalidate finished/queued signals before rejecting the dialog.
    if (dialog) {
        dialog->reject();
        dialog->deleteLater();
        if (m_complete)
            m_complete({});
    }
}

void FilePicker::request(Web::HTML::FileFilter const& accepted, Web::HTML::AllowMultipleFiles mode)
{
    if (m_dialog) {
        cancel();
        return;
    }

    bool multiple = mode == Web::HTML::AllowMultipleFiles::Yes;
    auto* dialog = m_factory ? m_factory(&m_parent) : new QFileDialog(&m_parent);
    m_dialog = dialog;
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(multiple ? "Select files" : "Select file");
    dialog->setNameFilter(filters(accepted));
    dialog->setFileMode(multiple ? QFileDialog::ExistingFiles : QFileDialog::ExistingFile);
    QObject::connect(dialog, &QDialog::finished, this,
        [this, dialog = QPointer<QFileDialog> { dialog }, multiple](int result) {
            if (!dialog || m_dialog != dialog)
                return;
            m_dialog = nullptr;
            Vector<Web::HTML::SelectedFile> files;
            if (result == QDialog::Accepted) {
                auto selected = snapshot(dialog->selectedFiles(), multiple);
                if (selected.is_error())
                    warnln("Firefly file selection refused: {}", selected.error());
                else
                    files = selected.release_value();
            }

            if (m_complete)
                m_complete(move(files));
        });
    dialog->open();
}
}
