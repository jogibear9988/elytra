/** Native path selection and HTML file-input snapshots. Paths stay in the browser process. */
#pragma once
#include "../file-access/Grants.h"
#include "Limits.h"
#include <AK/Function.h>
#include <LibWebCommon/HTML/FileFilter.h>
#include <LibWebCommon/HTML/SelectedFile.h>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QStringList>
class QFileDialog;
class QMessageBox;
class QWidget;

namespace Firefly {
class FilePicker final : public QObject {
public:
    static constexpr size_t max_files = FileSelectionPolicy::max_files;
    static constexpr size_t max_file_bytes = FileSelectionPolicy::max_file_bytes;
    static constexpr size_t max_total_bytes = FileSelectionPolicy::max_total_bytes;
    enum class Mode { OpenFile, OpenFiles, SaveFile, Directory };

    struct Selection {
        Mode mode { Mode::OpenFile };
        Web::HTML::FileFilter accepted_types;
        QString initial_directory;
        QString suggested_name;
        QStringList name_filters {};
        bool writable_directory { false };
        QString reminder_key {};
    };

    using PathCompletion = Function<void(QStringList)>;
    using Completion = Function<void(Vector<Web::HTML::SelectedFile>)>;
    // Internal native test seam: production uses the real platform dialog.
    using Factory = Function<QFileDialog*(QWidget*)>;
    FilePicker(QWidget&, Completion, Factory = {});

    ~FilePicker();

    void request(Web::HTML::FileFilter const&, Web::HTML::AllowMultipleFiles);

    // Internal browser-process provider. Rejected requests own no dialog or completion.
    bool request_selection(Selection const&, PathCompletion);

    using GrantCompletion = Function<void(ErrorOr<Vector<FileAccessGrants::Handle>>)>;
    ErrorOr<void> request_grants(FileAccessGrants::Document, Selection const&, GrantCompletion);

    using PermissionCompletion = Function<void(bool)>;
    bool request_permission(
        QString const& origin, QString const& name, bool writable, PermissionCompletion);

    QString remembered_directory(QString const& key) const;

    FileAccessGrants& grants()
    {
        return m_grants;
    }

    void cancel();

    void retire_document();

    static ErrorOr<Vector<Web::HTML::SelectedFile>> snapshot(QStringList const&, bool multiple);

private:
    QWidget& m_parent;
    Completion m_complete;
    Factory m_factory;
    PathCompletion m_path_completion;
    FileAccessGrants m_grants;
    QPointer<QFileDialog> m_dialog;
    QPointer<QMessageBox> m_permission_dialog;
    PermissionCompletion m_permission_completion;
    QHash<QString, QString> m_recent_directories;
};
}
