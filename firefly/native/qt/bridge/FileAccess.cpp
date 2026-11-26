/** Dispatch browser-owned filesystem capabilities for the currently active document. */
#include "Impl.h"
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <LibIPC/FireflyBootstrap.h>
#include <LibWebCommon/SecureContexts/AbstractOperations.h>
#include <LibWebView/CanonicalDocument.h>
#include <LibWebView/WebContentClient.h>
#include <LibWebView/WebContentPage.h>

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

            if (!page->consume_transient_user_activation()) {
                reply.fail("SecurityError"sv);
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
                    selection.suggested_name = QString::fromUtf8(
                        suggested->bytes_as_string_view().characters_without_null_termination(),
                        suggested->byte_count());
                }
            } else if (options.get_bool("multiple"sv).value_or(false)) {
                selection.mode = FilePicker::Mode::OpenFiles;
            }

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
