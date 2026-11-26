/** Own one nonmodal chooser per view and retire it before navigation/crash/destruction. */
#include "Picker.h"
#include <AK/ScopeGuard.h>
#include <QFileDialog>
#include <QFileInfo>
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
    m_path_completion = {};
    cancel();
}

void FilePicker::cancel()
{
    auto dialog = m_dialog;
    if (!dialog)
        return;

    m_dialog = nullptr; // Invalidate finished/queued signals before rejecting the dialog.
    auto completion = move(m_path_completion);
    dialog->reject();
    dialog->deleteLater();
    if (completion)
        completion({});
}

void FilePicker::retire_document()
{
    m_grants.revoke_all();
    cancel();
}

ErrorOr<void> FilePicker::request_grants(
    FileAccessGrants::Document document, Selection const& selection, GrantCompletion complete)
{
    if (!FileAccessGrants::supported() || selection.mode == Mode::SaveFile)
        return Error::from_errno(ENOTSUP);
    if (!document.namespace_id || !document.local_id || !complete)
        return Error::from_errno(EINVAL);

    auto kind = selection.mode == Mode::Directory ? FileAccessGrants::Kind::Directory
                                                  : FileAccessGrants::Kind::File;
    bool admitted = request_selection(
        selection, [this, document, kind, complete = move(complete)](QStringList paths) mutable {
            auto issue_grants = [&]() -> ErrorOr<Vector<FileAccessGrants::Handle>> {
                if (paths.size() > static_cast<qsizetype>(max_files))
                    return Error::from_errno(E2BIG);
                Vector<FileAccessGrants::Handle> handles;
                TRY(handles.try_ensure_capacity(paths.size()));
                ArmedScopeGuard rollback = [&] {
                    for (auto const& handle : handles)
                        m_grants.release(document, handle.token);
                };

                for (auto const& path : paths) {
                    auto bytes = path.toUtf8();
                    handles.append(TRY(m_grants.admit(
                        document, StringView(bytes.constData(), bytes.size()), kind)));
                }

                rollback.disarm();
                return handles;
            };

            complete(issue_grants());
        });
    if (!admitted)
        return Error::from_errno(EBUSY);
    return {};
}

void FilePicker::request(Web::HTML::FileFilter const& accepted, Web::HTML::AllowMultipleFiles mode)
{
    if (m_dialog) {
        cancel();
        return;
    }

    bool multiple = mode == Web::HTML::AllowMultipleFiles::Yes;
    auto selection = Selection {
        .mode = multiple ? Mode::OpenFiles : Mode::OpenFile,
        .accepted_types = accepted,
        .initial_directory = {},
        .suggested_name = {},
    };

    request_selection(selection, [this, multiple](QStringList paths) {
        Vector<Web::HTML::SelectedFile> files;
        auto selected = snapshot(paths, multiple);
        if (selected.is_error())
            warnln("Firefly file selection refused: {}", selected.error());
        else
            files = selected.release_value();
        if (m_complete)
            m_complete(move(files));
    });
}

bool FilePicker::request_selection(Selection const& selection, PathCompletion completion)
{
    if (m_dialog || !completion)
        return false;
    auto const& name = selection.suggested_name;
    if (!name.isEmpty()
        && (selection.mode != Mode::SaveFile || name == "." || name == ".."
            || name.contains(QChar::Null) || name.contains('/') || name.contains('\\')
            || QFileInfo(name).fileName() != name))
        return false;

    auto* dialog = m_factory ? m_factory(&m_parent) : new QFileDialog(&m_parent);
    m_dialog = dialog;
    m_path_completion = move(completion);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    if (!selection.initial_directory.isEmpty())
        dialog->setDirectory(selection.initial_directory);
    switch (selection.mode) {
    case Mode::OpenFile:
        dialog->setWindowTitle("Select file");
        dialog->setFileMode(QFileDialog::ExistingFile);
        break;
    case Mode::OpenFiles:
        dialog->setWindowTitle("Select files");
        dialog->setFileMode(QFileDialog::ExistingFiles);
        break;
    case Mode::SaveFile:
        dialog->setWindowTitle("Save file");
        dialog->setAcceptMode(QFileDialog::AcceptSave);
        dialog->setFileMode(QFileDialog::AnyFile);
        break;
    case Mode::Directory:
        dialog->setWindowTitle("Select folder");
        dialog->setFileMode(QFileDialog::Directory);
        dialog->setOption(QFileDialog::ShowDirsOnly);
        break;
    }

    if (selection.mode != Mode::Directory)
        dialog->setNameFilter(filters(selection.accepted_types));
    if (!name.isEmpty())
        dialog->selectFile(name);

    auto multiple = selection.mode == Mode::OpenFiles;
    QObject::connect(dialog, &QDialog::finished, this,
        [this, dialog = QPointer<QFileDialog> { dialog }, multiple](int result) {
            if (!dialog || m_dialog != dialog)
                return;
            m_dialog = nullptr;
            auto completion = move(m_path_completion);
            auto paths = result == QDialog::Accepted ? dialog->selectedFiles() : QStringList {};
            if (!multiple && paths.size() != 1)
                paths.clear();
            completion(move(paths));
        });
    dialog->open();
    return true;
}
}
