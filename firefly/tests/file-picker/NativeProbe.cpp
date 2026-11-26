/** Hidden Qt dialog acceptance plus real bounded regular-file snapshots, with no desktop
 * automation. */
#include "native/file-picker/Picker.h"
#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
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

        require(grants.child(owner, token, "refused.txt"sv, Grants::Kind::File, true).is_error(),
            "read-only directory cannot create entries");
        require(grants.remove(owner, token, "empty.txt"sv, false).is_error(),
            "read-only directory cannot remove entries");
        auto folder_bytes = directory.path().toUtf8();
        auto writable_directory
            = grants.admit(owner, StringView(folder_bytes.constData(), folder_bytes.size()),
                Grants::Kind::Directory, true);
        require(!writable_directory.is_error(), "create a writable directory capability");
        auto writable_token = writable_directory.value().token;
        require(grants.remove(foreign, writable_token, "empty.txt"sv, true).is_error(),
            "foreign document cannot remove entries");
        auto nested
            = grants.child(owner, writable_token, "nested"sv, Grants::Kind::Directory, true);
        require(!nested.is_error(), "create a child directory");
        auto nested_file
            = grants.child(owner, nested.value().token, "entry.txt"sv, Grants::Kind::File, true);
        require(!nested_file.is_error(), "create a child file");
        auto duplicate
            = grants.child(owner, nested.value().token, "entry.txt"sv, Grants::Kind::File);
        require(!duplicate.is_error(), "open the same child again");
        auto same = grants.is_same_entry(owner, nested_file.value().token, duplicate.value().token);
        require(
            !same.is_error() && same.value(), "compare separate descriptors for the same entry");
        auto relative = grants.resolve(owner, writable_token, nested_file.value().token);
        require(!relative.is_error() && relative.value().has_value()
                && relative.value()->size() == 2 && relative.value()->at(0) == "nested"sv
                && relative.value()->at(1) == "entry.txt"sv,
            "resolve path from the selected ancestor");

        auto outside = directory.filePath("outside");
        require(QDir().mkdir(outside), "create sibling outside the removal subtree");
        auto nested_link = directory.filePath("nested/link");
        require(QFile::link(outside, nested_link),
            "place a link to the sibling in the removal subtree");
        require(grants.remove(owner, writable_token, "nested"sv, false).is_error(),
            "nonrecursive removal refuses a nonempty directory");
        require(!grants.remove(owner, writable_token, "nested"sv, true).is_error()
                && QFileInfo::exists(outside) && !QFileInfo::exists(directory.filePath("nested")),
            "recursive removal unlinks the link and retains its target");

        QTemporaryDir profile;
        require(profile.isValid(), "prepare private native profile");
        auto profile_dir = QFileInfo(profile.path()).canonicalFilePath();
        auto profile_bytes = profile_dir.toUtf8();
        auto private_area = profile_dir + "/ElytraOriginFiles";
        auto private_bytes = private_area.toUtf8();
        grants.set_private_area(
            MUST(String::from_utf8(StringView(private_bytes.constData(), private_bytes.size()))));
        auto bucket = grants.admit_private(
            owner, StringView(profile_bytes.constData(), profile_bytes.size()), "fixture-bucket"sv);
        require(!bucket.is_error() && bucket.value().name.is_empty(), "create origin-private root");
        require(grants
                    .admit(owner, StringView(private_bytes.constData(), private_bytes.size()),
                        Grants::Kind::Directory, true)
                    .is_error(),
            "picker refuses a native path into the private backing area");
        auto private_file
            = grants.child(owner, bucket.value().token, "secret.txt"sv, Grants::Kind::File, true);
        require(!private_file.is_error(), "create file within private bucket");
        auto private_writer = grants.begin_write(owner, private_file.value().token, false);
        require(!private_writer.is_error(), "create private writer");
        require(grants.truncate(owner, private_writer.value(), 65 * 1024 * 1024).is_error(),
            "private bucket quota blocks sparse growth");
        require(!grants.finish_write(owner, private_writer.value(), false).is_error(),
            "release private writer after quota failure");
        auto sync = MUST(grants.begin_sync(owner, private_file.value().token, true, true));
        require(grants.begin_write(owner, private_file.value().token, false).is_error(),
            "exclusive sync handle excludes a staged writer");
        require(grants.begin_sync(owner, private_file.value().token, false, false).is_error(),
            "exclusive sync handle excludes other sync handles");
        require(grants.sync_read(foreign, sync, 0, 5).is_error(), "foreign sync lease rejected");
        require(grants.remove(owner, bucket.value().token, "secret.txt"sv, false).is_error(),
            "a locked private file cannot be removed");
        require(!grants.sync_write(owner, sync, 0, "secret"sv.bytes()).is_error(), "sync write");
        require(!grants.sync_truncate(owner, sync, 5).is_error(), "sync truncate");
        require(!grants.sync_flush(owner, sync).is_error(), "sync flush");
        require(MUST(grants.sync_size(owner, sync)) == 5, "sync size after truncate");
        auto synced = MUST(grants.sync_read(owner, sync, 1, 9));
        require(synced.bytes() == "ecre"sv.bytes(), "sync read respects offset and EOF");
        grants.close_sync(owner, sync);
        grants.close_sync(owner, sync);
        require(grants.sync_size(owner, sync).is_error(), "closed sync lease rejected");
        auto readonly = MUST(grants.begin_sync(owner, private_file.value().token, false, false));
        require(grants.sync_write(owner, readonly, 0, "x"sv.bytes()).is_error(),
            "read-only sync lease cannot write");
        grants.close_sync(owner, readonly);
        auto profile_path = StringView(profile_bytes.constData(), profile_bytes.size());
        auto reopened = MUST(grants.reopen_private(foreign, profile_path, "fixture-bucket"sv,
            { "secret.txt"_string }, Grants::Kind::File));
        require(!grants.read(foreign, reopened.token, 0, 5).is_error(),
            "same bucket reopens a serialized location under a fresh document grant");
        require(grants.read(owner, reopened.token, 0, 5).is_error(),
            "restored grant belongs to the receiving document");
        require(grants
                    .reopen_private(owner, profile_path, "different-bucket"sv,
                        { "secret.txt"_string }, Grants::Kind::File)
                    .is_error(),
            "a different origin bucket cannot restore the file");
        require(grants
                    .reopen_private(owner, profile_path, "fixture-bucket"sv, { ".."_string },
                        Grants::Kind::Directory)
                    .is_error(),
            "reopening a serialized location rejects parent traversal");
        grants.release(foreign, reopened.token);

        auto selected_path = QFileInfo(moved).canonicalFilePath().toUtf8();
        auto selected_handle = MUST(grants.admit(owner,
            StringView(selected_path.constData(), selected_path.size()), Grants::Kind::File));
        require(
            !grants.issue_native_reference(owner, selected_handle, profile_path, "fixture-bucket"sv)
                .is_error(),
            "issue an origin-bound reference to the selected native file");
        require(selected_handle.native_reference.has_value(), "issue opaque native reference");
        auto const& reference = *selected_handle.native_reference;
        require(grants
                    .reopen_native(
                        foreign, profile_path, "different-bucket"sv, reference, Grants::Kind::File)
                    .is_error(),
            "native reference refuses a different origin bucket");
        auto restored = MUST(grants.reopen_native(
            foreign, profile_path, "fixture-bucket"sv, reference, Grants::Kind::File));
        require(MUST(grants.permission(foreign, restored.token, false)) == "prompt"sv
                && grants.read(foreign, restored.token, 0, 4).is_error(),
            "restored native location does not authorize disk access");
        require(!grants.set_read_permission(foreign, restored.token, true).is_error(),
            "approve access to restored native location");
        require(!grants.read(foreign, restored.token, 0, 4).is_error()
                && grants.begin_write(foreign, restored.token, false).is_error(),
            "read approval allows bytes but does not allow edits");
        require(!grants
                    .remember_permission(
                        foreign, restored.token, profile_path, "fixture-bucket"sv, false)
                    .is_error(),
            "record a browser-approved native read grant");
        auto after_reopen = MUST(grants.reopen_native(
            owner, profile_path, "fixture-bucket"sv, reference, Grants::Kind::File));
        require(MUST(grants.permission(owner, after_reopen.token, false)) == "granted"sv
                && MUST(grants.permission(owner, after_reopen.token, true)) == "prompt"sv,
            "native read decision persists without granting write permission");
        require(!grants.set_write_permission(owner, after_reopen.token, true).is_error()
                && !grants
                    .remember_permission(
                        owner, after_reopen.token, profile_path, "fixture-bucket"sv, true)
                    .is_error(),
            "record a browser-approved native write grant");
        auto writable_reopen = MUST(grants.reopen_native(
            foreign, profile_path, "fixture-bucket"sv, reference, Grants::Kind::File));
        auto durable_writer = MUST(grants.begin_write(foreign, writable_reopen.token, false));
        require(!grants.finish_write(foreign, durable_writer, false).is_error(),
            "remembered write opens a staged writer");
        require(!grants.remove(owner, bucket.value().token, "secret.txt"sv, false).is_error(),
            "remove private test file");
        grants.set_private_area({});

        auto save_path = directory.filePath("writable.txt");
        auto save_utf8 = save_path.toUtf8();
        auto save = grants.admit_save(owner, StringView(save_utf8.constData(), save_utf8.size()));
        require(!save.is_error(), "save selection creates a native file capability");
        auto save_token = save.value().token;
        require(!grants.read(owner, save_token, 0, 8).is_error(), "new save target is readable");
        require(
            grants.begin_write(owner, child_token, false).is_error(), "read grant cannot write");
        require(
            grants.begin_write(foreign, save_token, false).is_error(), "foreign writer rejected");
        auto writer = grants.begin_write(owner, save_token, false);
        require(!writer.is_error(), "create isolated writer");
        require(
            grants.begin_write(owner, save_token, false).is_error(), "one writer per capability");
        require(!grants.write(owner, writer.value(), 0, "hello world"sv.bytes()).is_error(),
            "staged write");
        require(!grants.write(owner, writer.value(), 6, "Elytra"sv.bytes()).is_error(),
            "positional overwrite");
        require(!grants.truncate(owner, writer.value(), 9).is_error(), "staged truncate");
        QFile saved(save_path);
        require(saved.open(QIODevice::ReadOnly) && saved.readAll().isEmpty(),
            "edits are invisible before close");
        saved.close();
        require(grants.finish_write(foreign, writer.value(), true).is_error(),
            "foreign close rejected");
        require(
            !grants.finish_write(owner, writer.value(), true).is_error(), "commit staged contents");
        require(
            saved.open(QIODevice::ReadOnly) && saved.readAll() == "hello Ely", "committed bytes");
        saved.close();
        part = grants.read(owner, save_token, 0, 64);
        require(!part.is_error() && part.value().bytes() == "hello Ely"sv.bytes(),
            "save capability follows its own committed replacement");

        writer = grants.begin_write(owner, save_token, true);
        require(!writer.is_error(), "copy an existing save target");
        require(
            !grants.write(owner, writer.value(), 8, "ytra"sv.bytes()).is_error(), "extend copy");
        require(
            !grants.finish_write(owner, writer.value(), true).is_error(), "commit copied contents");
        require(saved.open(QIODevice::ReadOnly) && saved.readAll() == "hello Elytra",
            "keepExistingData retains bytes");
        saved.close();

        writer = grants.begin_write(owner, save_token, false);
        require(!writer.is_error()
                && !grants.write(owner, writer.value(), 0, "discard"sv.bytes()).is_error(),
            "stage aborted writer");
        require(!grants.finish_write(owner, writer.value(), false).is_error(), "abort writer");
        require(saved.open(QIODevice::ReadOnly) && saved.readAll() == "hello Elytra",
            "abort preserves previous contents");
        saved.close();

        auto race_path = directory.filePath("race-save.txt");
        auto race_utf8 = race_path.toUtf8();
        auto race = grants.admit_save(owner, StringView(race_utf8.constData(), race_utf8.size()));
        require(!race.is_error(), "choose the original save destination");
        auto race_writer = grants.begin_write(owner, race.value().token, false);
        require(!race_writer.is_error(), "stage changes before replacing the destination");
        auto old_race_path = directory.filePath("old-race-save.txt");
        require(QFile::rename(race_path, old_race_path), "move the original save target");
        QFile new_race(race_path);
        require(new_race.open(QIODevice::WriteOnly) && new_race.write("replacement") == 11,
            "create an unrelated destination under the old name");
        new_race.close();
        require(grants.finish_write(owner, race_writer.value(), true).is_error(),
            "close refuses an externally replaced destination");
        require(new_race.open(QIODevice::ReadOnly) && new_race.readAll() == "replacement",
            "refused close preserves the replacement");
        new_race.close();

        writer = grants.begin_write(owner, save_token, false);
        require(!writer.is_error(), "leave writer pending during navigation");

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
        require(grants.finish_write(owner, writer.value(), true).is_error(),
            "navigation revokes writer");
        require(QDir(directory.path())
                    .entryList({ ".elytra-file-*" }, QDir::Files | QDir::Hidden)
                    .isEmpty(),
            "retired writers remove their temporary files");
        std::puts(
            "PASS document-bound save, positional write/truncate, commit, abort and retirement");
        std::puts("PASS document-bound descriptor grants, bounded reads and paging, navigation "
                  "revocation");
    }

    std::puts("PASS native folder and save modes with owned path completion");
    std::puts("PASS file picker: real hidden Qt accept/cancel/stale/destruction; "
              "binary/basename/type/FIFO/count/exact/overflow snapshot limits");
    return 0;
}
