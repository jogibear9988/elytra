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
    std::puts("PASS file picker: real hidden Qt accept/cancel/stale/destruction; "
              "binary/basename/type/FIFO/count/exact/overflow snapshot limits");
    return 0;
}
