/** Native file-input chooser. Snapshot only accepted regular files; never expose native paths. */
#pragma once
#include <AK/Function.h>
#include <LibWebCommon/HTML/FileFilter.h>
#include <LibWebCommon/HTML/SelectedFile.h>
#include <QObject>
#include <QPointer>
#include <QStringList>
class QFileDialog;
class QWidget;

namespace Firefly {
class FilePicker final : public QObject {
public:
    static constexpr size_t max_files = 16;
    static constexpr size_t max_file_bytes = 4 * 1024 * 1024;
    static constexpr size_t max_total_bytes = 16 * 1024 * 1024;
    using Completion = Function<void(Vector<Web::HTML::SelectedFile>)>;
    // Internal native test seam: production uses the real platform dialog.
    using Factory = Function<QFileDialog*(QWidget*)>;
    FilePicker(QWidget&, Completion, Factory = {});

    ~FilePicker();

    void request(Web::HTML::FileFilter const&, Web::HTML::AllowMultipleFiles);

    void cancel();

    static ErrorOr<Vector<Web::HTML::SelectedFile>> snapshot(QStringList const&, bool multiple);

private:
    QWidget& m_parent;
    Completion m_complete;
    Factory m_factory;
    QPointer<QFileDialog> m_dialog;
};
}
