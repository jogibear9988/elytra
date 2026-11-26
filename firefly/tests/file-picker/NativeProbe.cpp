/** Hidden Qt dialog acceptance plus real bounded regular-file snapshots, with no desktop
 * automation. */
#include "native/file-picker/Picker.h"
#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QTemporaryDir>
#include <QWidget>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

static void require(bool condition, char const* message)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        std::exit(1);
    }
}

int main(int argc, char** argv)
{
    QApplication application(argc, argv);
    QTemporaryDir directory;
    require(directory.isValid(), "temporary files");
    auto path = directory.filePath("ä selected.txt");
    QFile output(path);
    require(output.open(QIODevice::WriteOnly) && output.write("hello\0file", 10) == 10,
        "write fixture");
    output.close();
    auto check = Firefly::FilePicker::snapshot({ path }, false);
    require(!check.is_error() && check.value().size() == 1, "regular snapshot");
    require(check.value()[0].name() == Utf16String::from_utf8("ä selected.txt"sv), "basename only");
    auto bytes = check.value()[0].take_contents();
    require(bytes.size() == 10 && memcmp(bytes.data(), "hello\0file", 10) == 0,
        "exact binary contents");
    auto empty = directory.filePath("empty.txt");
    QFile empty_file(empty);
    require(empty_file.open(QIODevice::WriteOnly), "empty fixture");
    empty_file.close();
    QStringList exact_count;
    for (int i = 0; i < 16; ++i)
        exact_count.append(empty);
    require(!Firefly::FilePicker::snapshot(exact_count, true).is_error(),
        "exact count and empty files");
    QString nul = path + QChar::Null + "suffix";
    require(Firefly::FilePicker::snapshot({ nul }, false).is_error(), "NUL path refusal");
    require(
        Firefly::FilePicker::snapshot({ path, path }, false).is_error(), "single selection count");
    require(!Firefly::FilePicker::snapshot({ path, path }, true).is_error(), "multiple selection");
    require(
        Firefly::FilePicker::snapshot({ directory.path() }, false).is_error(), "directory refusal");
    require(Firefly::FilePicker::snapshot({ directory.filePath("missing.txt") }, false).is_error(),
        "missing file refusal");
    require(Firefly::FilePicker::snapshot({ "relative.txt" }, false).is_error(),
        "relative path refusal");
    QStringList count;
    for (int i = 0; i < 17; ++i)
        count.append(path);
    require(Firefly::FilePicker::snapshot(count, true).is_error(), "count budget");
    auto large = directory.filePath("large.bin");
    QFile oversized(large);
    require(oversized.open(QIODevice::WriteOnly)
            && oversized.resize(Firefly::FilePicker::max_file_bytes),
        "large fixture");
    oversized.close();
    require(!Firefly::FilePicker::snapshot({ large }, false).is_error(), "exact file budget");
    require(!Firefly::FilePicker::snapshot({ large, large, large, large }, true).is_error(),
        "exact aggregate budget");
    require(Firefly::FilePicker::snapshot({ large, large, large, large, large }, true).is_error(),
        "aggregate overflow");
    require(oversized.open(QIODevice::WriteOnly)
            && oversized.resize(Firefly::FilePicker::max_file_bytes + 1),
        "oversize fixture");
    oversized.close();
    require(Firefly::FilePicker::snapshot({ path, large }, true).is_error(),
        "all-or-nothing byte refusal");
#ifndef _WIN32
    auto fifo = directory.filePath("pipe").toUtf8();
    require(mkfifo(fifo.constData(), 0600) == 0, "FIFO fixture");
    require(Firefly::FilePicker::snapshot({ QString::fromUtf8(fifo) }, false).is_error(),
        "nonblocking FIFO refusal");
#endif
    QWidget parent;
    unsigned callbacks = 0;
    size_t files = 0;
    QPointer<QFileDialog> dialog;
    auto factory = [&](QWidget* owner) {
        auto* result = new QFileDialog(owner);
        result->setOption(QFileDialog::DontUseNativeDialog);
        result->setAttribute(Qt::WA_DontShowOnScreen);
        dialog = result;
        return result;
    };

    Firefly::FilePicker picker(
        parent,
        [&](auto selected) {
            ++callbacks;
            files = selected.size();
        },
        [&](QWidget* owner) { return factory(owner); });
    picker.request({}, Web::HTML::AllowMultipleFiles::No);
    require(dialog && dialog->fileMode() == QFileDialog::ExistingFile, "real Qt single mode");
    dialog->selectFile(path);
    require(QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection), "Qt accept");
    require(callbacks == 1 && files == 1, "accepted chooser snapshots");
    picker.request({}, Web::HTML::AllowMultipleFiles::Yes);
    require(dialog && dialog->fileMode() == QFileDialog::ExistingFiles, "real Qt multiple mode");
    auto stale = dialog;
    picker.cancel();
    require(callbacks == 2 && files == 0, "cancel once");
    picker.cancel();
    require(callbacks == 2, "idempotent cancel");
    if (stale)
        QMetaObject::invokeMethod(stale, "accept", Qt::DirectConnection);
    require(callbacks == 2, "stale finish cannot deliver files");
    picker.request({}, Web::HTML::AllowMultipleFiles::No);
    picker.request({}, Web::HTML::AllowMultipleFiles::No);
    require(callbacks == 3 && files == 0, "duplicate request cancels without data");
    auto* owned = new Firefly::FilePicker(
        parent, [&](auto) { ++callbacks; }, [&](QWidget* owner) { return factory(owner); });
    owned->request({}, Web::HTML::AllowMultipleFiles::No);
    delete owned;
    application.processEvents();
    require(callbacks == 3, "destruction cannot invoke dead view");
    using Picker = Firefly::FilePicker;
    unsigned path_callbacks = 0;
    QStringList chosen_paths;
    auto receive = [&]() -> Picker::PathCompletion {
        return [&](QStringList paths) {
            ++path_callbacks;
            chosen_paths = move(paths);
        };
    };

    auto select_folder = Picker::Selection {
        .mode = Picker::Mode::Directory,
        .accepted_types = {},
        .initial_directory = directory.path(),
        .suggested_name = {},
    };

    require(picker.request_selection(select_folder, receive()), "folder selection admitted");
    require(dialog && dialog->fileMode() == QFileDialog::Directory
            && dialog->testOption(QFileDialog::ShowDirsOnly),
        "folder chooser mode");
    require(!picker.request_selection(select_folder, receive()), "second path request refused");
    require(QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection), "folder accept");
    require(path_callbacks == 1 && chosen_paths == QStringList { directory.path() },
        "folder path stays in the native completion");
    auto save_file = Picker::Selection {
        .mode = Picker::Mode::SaveFile,
        .accepted_types = {},
        .initial_directory = directory.path(),
        .suggested_name = "../outside.txt",
    };

    require(!picker.request_selection(save_file, receive()), "path-like suggested name refused");
    save_file.suggested_name = "new.txt";
    require(picker.request_selection(save_file, receive()), "save selection admitted");
    require(dialog && dialog->acceptMode() == QFileDialog::AcceptSave
            && dialog->fileMode() == QFileDialog::AnyFile,
        "save chooser mode");
    require(QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection), "save accept");
    require(path_callbacks == 2 && chosen_paths == QStringList { directory.filePath("new.txt") },
        "save chooser resolves the suggested file");
    require(!QFile::exists(chosen_paths.front()), "save chooser does not write file bytes");
    require(callbacks == 3, "path selections bypass HTML input snapshot completion");
    if (Firefly::FileAccessGrants::supported()) {
        using Grants = Firefly::FileAccessGrants;
        Grants::Document owner { 1, 1 };
        Grants::Document foreign { 1, 2 };
        unsigned grant_completions = 0;
        Vector<Grants::Handle> handles;
        auto grant_request = picker.request_grants(
            owner, select_folder, [&](ErrorOr<Vector<Grants::Handle>> result) {
                require(!result.is_error(), "native selection creates grants");
                handles = result.release_value();
                ++grant_completions;
            });
        require(!grant_request.is_error(), "native folder grant request");
        require(QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection), "grant accept");
        require(grant_completions == 1 && handles.size() == 1
                && handles.first().kind == Grants::Kind::Directory,
            "one document-scoped directory capability");
        auto& grants = picker.grants();
        auto token = handles.first().token;
        auto child = grants.child(owner, token, "ä selected.txt"sv, Grants::Kind::File);
        require(!child.is_error(), "open child relative to selected directory");
        auto child_token = child.value().token;
        auto part = grants.read(owner, child_token, 6, 64);
        require(!part.is_error() && part.value().size() == 4
                && memcmp(part.value().data(), "file", 4) == 0,
            "offset read from selected file");
        require(grants.read(foreign, child_token, 0, 1).is_error(), "foreign document rejected");
        require(grants.child(owner, token, "../outside"sv, Grants::Kind::File).is_error(),
            "directory traversal refused");
        require(grants.read(owner, child_token, 0, Grants::max_read_size + 1).is_error(),
            "read budget enforced");
        auto link = directory.filePath("alias.txt");
        require(QFile::link(path, link), "create symlink fixture");
        require(grants.child(owner, token, "alias.txt"sv, Grants::Kind::File).is_error(),
            "directory capability never follows a child symlink");
        auto cursor = grants.begin_iteration(owner, token);
        auto independent = grants.begin_iteration(owner, token);
        require(!cursor.is_error() && !independent.is_error(), "create independent iterators");
        require(grants.next_entries(foreign, cursor.value(), 1).is_error(),
            "foreign document cannot advance iterator");
        require(grants.next_entries(owner, cursor.value(), 0).is_error()
                && grants.next_entries(owner, cursor.value(), Grants::max_entries_per_page + 1)
                    .is_error(),
            "reject invalid batch sizes without consuming the cursor");
        auto first_page = grants.next_entries(owner, cursor.value(), 1);
        auto independent_page = grants.next_entries(owner, independent.value(), 1);
        require(!first_page.is_error() && !independent_page.is_error()
                && first_page.value().entries.size() == 1
                && independent_page.value().entries.size() == 1
                && first_page.value().entries.first().name
                    == independent_page.value().entries.first().name,
            "independent iterators start at the same entry");
        auto is_selected = [&](Grants::DirectoryEntry const& entry) {
            // Some native filesystems return canonically decomposed names.
            // Compare the display spelling, then verify the returned bytes work as a lookup.
            if (QString::fromUtf8(
                    entry.name.bytes_as_string_view().characters_without_null_termination(),
                    entry.name.byte_count())
                    .normalized(QString::NormalizationForm_C)
                != QString::fromUtf8("ä selected.txt"))
                return false;
            require(
                !grants.child(owner, token, entry.name.bytes_as_string_view(), Grants::Kind::File)
                    .is_error(),
                "enumerated name resolves using its original bytes");
            return true;
        };

        bool found_selected = is_selected(first_page.value().entries.first());
        auto last_page = grants.next_entries(owner, cursor.value(), Grants::max_entries_per_page);
        require(!last_page.is_error() && last_page.value().done, "enumeration reaches end");
        for (auto const& entry : last_page.value().entries) {
            require(entry.name != "alias.txt"sv && entry.name != "pipe"sv,
                "enumeration skips symlinks and special files");
            found_selected |= is_selected(entry);
        }

        require(found_selected, "enumeration includes regular files");
        require(grants.next_entries(owner, cursor.value(), 1).is_error(),
            "end of iteration releases the cursor");
        grants.close_iteration(owner, independent.value());
        require(grants.next_entries(owner, independent.value(), 1).is_error(),
            "explicit close releases the cursor");
        auto pending_cursor = grants.begin_iteration(owner, token);
        require(!pending_cursor.is_error(), "create cursor before navigation");
        auto moved = directory.filePath("renamed.txt");
        require(QFile::rename(path, moved), "rename selected file");
        QFile replacement(path);
        require(replacement.open(QIODevice::WriteOnly) && replacement.write("replacement") == 11,
            "replace old path");
        replacement.close();
        part = grants.read(owner, child_token, 6, 64);
        require(!part.is_error() && part.value().size() == 4
                && memcmp(part.value().data(), "file", 4) == 0,
            "an existing capability retains the selected descriptor");
        grants.release(foreign, child_token);
        require(!grants.read(owner, child_token, 0, 1).is_error(), "foreign release ignored");

        grant_request = picker.request_grants(
            owner, select_folder, [&](ErrorOr<Vector<Grants::Handle>> result) {
                require(!result.is_error() && result.value().is_empty(), "cancel grants nothing");
                ++grant_completions;
            });
        require(!grant_request.is_error(), "admit pending grant prompt");
        picker.retire_document();
        require(grant_completions == 2, "navigation cancels the pending chooser once");
        require(grants.read(owner, child_token, 0, 1).is_error(), "navigation revokes file");
        require(grants.child(owner, token, "empty.txt"sv, Grants::Kind::File).is_error(),
            "navigation revokes directory");
        require(grants.next_entries(owner, pending_cursor.value(), 1).is_error(),
            "navigation revokes iteration");
        std::puts("PASS document-bound descriptor grants, bounded reads and paging, navigation "
                  "revocation");
    }

    std::puts("PASS native folder and save modes with owned path completion");
    std::puts("PASS file picker: real hidden Qt accept/cancel/stale/destruction; "
              "binary/basename/type/FIFO/count/exact/overflow snapshot limits");
    return 0;
}
