/** Dispatch browser-owned filesystem capabilities for the currently active document. */
#include "Impl.h"
#include <AK/CharacterTypes.h>
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <LibIPC/FireflyBootstrap.h>
#include <LibWebCommon/MimeSniff/MimeType.h>
#include <LibWebCommon/SecureContexts/AbstractOperations.h>
#include <LibWebView/CanonicalDocument.h>
#include <LibWebView/CanonicalTraversable.h>
#include <LibWebView/WebContentClient.h>
#include <LibWebView/WebContentPage.h>
#include <QMimeDatabase>
#include <QStandardPaths>

namespace Firefly {
namespace {
    using Kind = FileAccessGrants::Kind;
    using Document = FileAccessGrants::Document;

    JsonObject handle_info(FileAccessGrants::Handle const& handle)
    {
        JsonObject result;
        result.set("token"sv, handle.token);
        result.set("name"sv, handle.name);
        result.set("kind"sv, handle.kind == Kind::File ? "file"sv : "directory"sv);
        return result;
    }

    struct Reply {
        RefPtr<WebView::WebContentPage> page;
        Document document;
        u64 request;

        void send(JsonObject metadata, ByteBuffer bytes = {}) const
        {
            if (page->is_open())
                page->client().async_firefly_file_access_reply(page->id().value(),
                    document.namespace_id, document.local_id, request, move(metadata),
                    bytes.bytes());
        }

        void fail(StringView name) const
        {
            JsonObject metadata;
            metadata.set("error"sv, name);
            send(move(metadata));
        }

        void fail(Error const& error) const
        {
            if (!error.is_errno()) {
                fail("UnknownError"sv);
                return;
            }

            switch (error.code()) {
            case EBUSY:
            case EAGAIN:
                fail("InvalidStateError"sv);
                break;
            case ENOENT:
                fail("NotFoundError"sv);
                break;
            case EISDIR:
            case ENOTDIR:
                fail("TypeMismatchError"sv);
                break;
            case EACCES:
            case EPERM:
                fail("NotAllowedError"sv);
                break;
            case ENOTSUP:
                fail("NotSupportedError"sv);
                break;
            case EINVAL:
                fail("TypeError"sv);
                break;
            case E2BIG:
            case ENOSPC:
                fail("QuotaExceededError"sv);
                break;
            default:
                fail("UnknownError"sv);
                break;
            }
        }
    };

    bool accessible_origin(WebView::WebContentPage& page, Document id)
    {
        auto document = page.hosted_document_with_id(id);
        if (!document)
            return false;
        auto const& origin = document->origin();
        auto top = document->top_level_origin();
        return !origin.is_opaque() && top.has_value() && origin.is_same_origin(*top)
            && Web::SecureContexts::is_origin_potentially_trustworthy(origin)
            == Web::SecureContexts::Trustworthiness::PotentiallyTrustworthy;
    }

    QString qt_string(String const& value)
    {
        return QString::fromUtf8(
            value.bytes_as_string_view().characters_without_null_termination(), value.byte_count());
    }

    ErrorOr<void> apply_picker_options(JsonObject const& options, FilePicker::Selection& selection)
    {
        if (auto location = options.get_string("startIn"sv); location.has_value()) {
            using Location = QStandardPaths::StandardLocation;
            Location folder;
            if (*location == "desktop"sv)
                folder = Location::DesktopLocation;
            else if (*location == "documents"sv)
                folder = Location::DocumentsLocation;
            else if (*location == "downloads"sv)
                folder = Location::DownloadLocation;
            else if (*location == "music"sv)
                folder = Location::MusicLocation;
            else if (*location == "pictures"sv)
                folder = Location::PicturesLocation;
            else if (*location == "videos"sv)
                folder = Location::MoviesLocation;
            else
                return Error::from_errno(EINVAL);

            selection.initial_directory = QStandardPaths::writableLocation(folder);
        }

        auto groups = options.get_array("filterGroups"sv);
        auto exclude_all = options.get_bool("excludeAcceptAllOption"sv).value_or(false);
        if ((!groups.has_value() || groups->is_empty()) && !exclude_all)
            return {};
        if (!groups.has_value() || groups->is_empty() || groups->size() > 16
            || selection.mode == FilePicker::Mode::Directory)
            return Error::from_errno(EINVAL);

        QMimeDatabase database;
        for (size_t i = 0; i < groups->size(); ++i) {
            auto const& value = groups->at(i);
            if (!value.is_object())
                return Error::from_errno(EINVAL);

            auto const& group = value.as_object();
            auto extensions = group.get_array("extensions"sv);
            auto description = group.get_string("description"sv);
            auto mimes = group.get_array("mimes"sv);
            if (!extensions.has_value() || extensions->size() > 32 || !mimes.has_value()
                || mimes->size() > 16 || !description.has_value()
                || description->byte_count() > 256)
                return Error::from_errno(EINVAL);

            QStringList patterns;
            for (size_t index = 0; index < extensions->size(); ++index) {
                auto const& suffix = extensions->at(index);
                if (!suffix.is_string())
                    return Error::from_errno(EINVAL);
                auto extension = suffix.as_string().bytes_as_string_view();
                if (extension.length() < 2 || extension.length() > 16 || !extension.starts_with('.')
                    || extension.ends_with('.'))
                    return Error::from_errno(EINVAL);
                for (auto character : extension) {
                    if (!is_ascii_alphanumeric(character) && character != '.' && character != '+'
                        && character != '-' && character != '_')
                        return Error::from_errno(EINVAL);
                }

                patterns.append("*" + qt_string(suffix.as_string()));
            }

            for (size_t index = 0; index < mimes->size(); ++index) {
                auto const& mime = mimes->at(index);
                if (!mime.is_string() || mime.as_string().byte_count() > 127)
                    return Error::from_errno(EINVAL);
                auto parsed
                    = Web::MimeSniff::MimeType::parse(mime.as_string().bytes_as_string_view());
                if (!parsed.has_value() || !parsed->parameters().is_empty())
                    return Error::from_errno(EINVAL);
                patterns.append(database.mimeTypeForName(qt_string(parsed->essence()))
                        .globPatterns()
                        .mid(0, 16));
            }

            if (patterns.isEmpty())
                continue;

            patterns.removeDuplicates();
            auto label = qt_string(*description).simplified();
            label.replace('(', '[').replace(')', ']').replace(';', ',').replace(
                QChar::Null, QChar('?'));
            if (label.isEmpty())
                label = "Accepted files";
            selection.name_filters.append(label + " (" + patterns.join(' ') + ')');
        }

        if (!exclude_all)
            selection.name_filters.append("All files (*)");
        if (selection.name_filters.isEmpty())
            return Error::from_errno(EINVAL);
        return {};
    }

    void dispatch_handle_operation(Reply const& reply, FileAccessGrants& grants,
        StringView operation, JsonObject const& options, ReadonlyBytes bytes)
    {
        auto token = options.get_string("token"sv);
        if (!token.has_value() || token->byte_count() != 36) {
            reply.fail("TypeError"sv);
            return;
        }

        if (operation == "beginWrite"sv) {
            auto writer = grants.begin_write(
                reply.document, *token, options.get_bool("keepExistingData"sv).value_or(false));
            if (writer.is_error()) {
                reply.fail(writer.error());
                return;
            }

            JsonObject metadata;
            metadata.set("writer"sv, writer.release_value());
            reply.send(move(metadata));
            return;
        }

        if (operation.is_one_of("write"sv, "truncate"sv, "closeWriter"sv, "abortWriter"sv)) {
            auto result = [&]() -> ErrorOr<void> {
                if (operation == "write"sv) {
                    auto offset = options.get_u64("offset"sv);
                    if (!offset.has_value())
                        return Error::from_errno(EINVAL);
                    return grants.write(reply.document, *token, *offset, bytes);
                }

                if (operation == "truncate"sv) {
                    auto size = options.get_u64("size"sv);
                    if (!size.has_value())
                        return Error::from_errno(EINVAL);
                    return grants.truncate(reply.document, *token, *size);
                }

                return grants.finish_write(reply.document, *token, operation == "closeWriter"sv);
            }();

            if (result.is_error())
                reply.fail(result.error());
            else
                reply.send({});
            return;
        }

        if (operation == "release"sv) {
            grants.release(reply.document, *token);
            reply.send({});
            return;
        }

        if (operation == "child"sv) {
            auto name = options.get_string("name"sv);
            auto kind = options.get_string("kind"sv);
            if (!name.has_value() || !kind.has_value()
                || !kind->bytes_as_string_view().is_one_of("file"sv, "directory"sv)) {
                reply.fail("TypeError"sv);
                return;
            }

            auto result = grants.child(reply.document, *token, name->bytes_as_string_view(),
                *kind == "file"sv ? Kind::File : Kind::Directory);
            if (result.is_error())
                reply.fail(result.error());
            else
                reply.send(handle_info(result.value()));
            return;
        }

        if (operation == "read"sv) {
            auto offset = options.get_u64("offset"sv);
            auto count = options.get_u64("count"sv);
            if (!offset.has_value() || !count.has_value()
                || *count > FileAccessGrants::max_read_size) {
                reply.fail("TypeError"sv);
                return;
            }

            auto result = grants.read(reply.document, *token, *offset, static_cast<size_t>(*count));
            if (result.is_error())
                reply.fail(result.error());
            else
                reply.send({}, result.release_value());
            return;
        }

        if (operation == "iterate"sv) {
            auto result = grants.begin_iteration(reply.document, *token);
            if (result.is_error()) {
                reply.fail(result.error());
                return;
            }

            JsonObject metadata;
            metadata.set("cursor"sv, result.release_value());
            reply.send(move(metadata));
            return;
        }

        if (operation == "closeIterator"sv) {
            grants.close_iteration(reply.document, *token);
            reply.send({});
            return;
        }

        if (operation == "nextEntries"sv) {
            auto result = grants.next_entries(
                reply.document, *token, FileAccessGrants::max_entries_per_page);
            if (result.is_error()) {
                reply.fail(result.error());
                return;
            }

            JsonArray entries;
            for (auto const& entry : result.value().entries) {
                JsonObject metadata;
                metadata.set("name"sv, entry.name);
                metadata.set("kind"sv, entry.kind == Kind::File ? "file"sv : "directory"sv);
                MUST(entries.append(move(metadata)));
            }

            JsonObject metadata;
            metadata.set("entries"sv, move(entries));
            metadata.set("done"sv, result.value().done);
            reply.send(move(metadata));
            return;
        }

        reply.fail("NotSupportedError"sv);
    }
}

void Bridge::Impl::initialize_file_access()
{
    Bytes::set_file_access_request_receiver(
        [this](IPC::Stub& stub, Bytes::FileAccessMessage message) {
            auto& client
                = static_cast<WebView::WebContentClient&>(static_cast<WebContentClientStub&>(stub));
            auto* page = client.page(Web::PageId { message.page });
            if (!page || !page->is_open())
                return;

            Reply reply { page, { message.document_namespace, message.document_local },
                message.request };
            auto id = permission_contents(*page, reply.document);
            if (!id || !accessible_origin(*page, reply.document)) {
                reply.fail("SecurityError"sv);
                return;
            }

            auto& state = windows.at(id);
            if (!state.file_picker || !FileAccessGrants::supported()) {
                reply.fail("NotSupportedError"sv);
                return;
            }

            if (!message.metadata.is_object()
                || message.bytes.size() > FileAccessGrants::max_read_size) {
                reply.fail("TypeError"sv);
                return;
            }

            auto const& options = message.metadata.as_object();
            auto operation = options.get_string("operation"sv);
            if (!operation.has_value() || (*operation != "write"sv && !message.bytes.is_empty())) {
                reply.fail("TypeError"sv);
                return;
            }

            if (!operation->bytes_as_string_view().is_one_of("open"sv, "directory"sv, "save"sv)) {
                dispatch_handle_operation(reply, state.file_picker->grants(),
                    operation->bytes_as_string_view(), options, message.bytes.bytes());
                return;
            }

            FilePicker::Selection selection {};
            if (*operation == "directory"sv)
                selection.mode = FilePicker::Mode::Directory;
            else if (*operation == "save"sv) {
                selection.mode = FilePicker::Mode::SaveFile;
                if (auto suggested = options.get_string("suggestedName"sv); suggested.has_value()) {
                    if (suggested->byte_count() > 255) {
                        reply.fail("TypeError"sv);
                        return;
                    }

                    selection.suggested_name = qt_string(*suggested);
                }
            } else if (options.get_bool("multiple"sv).value_or(false)) {
                selection.mode = FilePicker::Mode::OpenFiles;
            }

            auto configured = apply_picker_options(options, selection);
            if (configured.is_error()) {
                reply.fail(configured.error());
                return;
            }

            if (!page->consume_transient_user_activation()) {
                reply.fail("SecurityError"sv);
                return;
            }

            // Browser admission consumes the trusted input before any renderer gets a reply.
            page->traversable().top_level_traversable().for_each_hosting_page([](auto& host) {
                host.async_consume_user_activation(Web::HTML::UserActivationConsumption::Transient);
            });

            auto started = state.file_picker->request_grants(reply.document, selection,
                [this, reply](ErrorOr<Vector<FileAccessGrants::Handle>> result) {
                    if (!permission_contents(*reply.page, reply.document)) {
                        reply.fail("AbortError"sv);
                        return;
                    }

                    if (result.is_error()) {
                        reply.fail(result.error());
                        return;
                    }

                    if (result.value().is_empty()) {
                        reply.fail("AbortError"sv);
                        return;
                    }

                    JsonArray handles;
                    for (auto const& handle : result.value())
                        MUST(handles.append(handle_info(handle)));
                    JsonObject metadata;
                    metadata.set("handles"sv, move(handles));
                    reply.send(move(metadata));
                });
            if (started.is_error())
                reply.fail(started.error());
        });
}

void Bridge::Impl::stop_file_access()
{
    Bytes::set_file_access_request_receiver({});
}

}
