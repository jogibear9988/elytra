/** Bounded byte snapshots avoid passing descriptors whose later read could grow without limit. */
#include "Picker.h"
#include <AK/LexicalPath.h>
#include <LibCore/File.h>
#include <QDir>
#include <sys/stat.h>

namespace Firefly {
ErrorOr<Vector<Web::HTML::SelectedFile>> FilePicker::snapshot(
    QStringList const& paths, bool multiple)
{
    if (paths.size() > static_cast<qsizetype>(max_files) || (!multiple && paths.size() > 1))
        return Error::from_string_literal("File selection count exceeds policy");
    Vector<Web::HTML::SelectedFile> selected;
    if (paths.isEmpty())
        return selected;
    auto storage = TRY(ByteBuffer::create_uninitialized(max_file_bytes + 1));
    size_t total = 0;
    for (auto const& path : paths) {
        auto encoded = path.toUtf8();
        if (!QDir::isAbsolutePath(path) || path.contains(QChar::Null) || encoded.size() > 4096)
            return Error::from_string_literal("File selection path must be absolute and bounded");
        StringView native(encoded.constData(), encoded.size());
        // Nonblocking open avoids waiting for a FIFO writer. Check the opened handle
        // with fstat so path replacement cannot bypass regular-file validation.
        auto file = TRY(Core::File::open(
            native, Core::File::OpenMode::Read | Core::File::OpenMode::Nonblocking));
        auto metadata = TRY(file->stat());
        if (!S_ISREG(metadata.st_mode) || metadata.st_size < 0
            || static_cast<u64>(metadata.st_size) > max_file_bytes)
            return Error::from_string_literal("Only bounded regular files may be selected");
        size_t size = 0;
        while (size < storage.size()) {
            auto read = TRY(file->read_some(storage.bytes().slice(size)));
            if (read.is_empty())
                break;
            size += read.size();
        }

        if (size > max_file_bytes || size > max_total_bytes - total)
            return Error::from_string_literal("File selection byte budget exceeded");
        total += size;
        // All-or-nothing: a later failure destroys earlier snapshots. File names
        // follow HTML's basename-only rule; this is not getPathForFile provenance.
        selected.append({ Utf16String::from_utf8(LexicalPath::basename(native)),
            TRY(ByteBuffer::copy(storage.bytes().slice(0, size))) });
    }

    return selected;
}
}
