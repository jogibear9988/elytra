/** Dispatch browser-owned filesystem capabilities for the currently active document. */
#include "Impl.h"
#include <AK/CharacterTypes.h>
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/NeverDestroyed.h>
#include <LibIPC/FireflyBootstrap.h>
#include <LibWebCommon/MimeSniff/MimeType.h>
#include <LibWebCommon/SecureContexts/AbstractOperations.h>
#include <LibWebView/Application.h>
#include <LibWebView/BrowsingSession.h>
#include <LibWebView/CanonicalDocument.h>
#include <LibWebView/CanonicalEnvironmentSettingsObject.h>
#include <LibWebView/CanonicalTraversable.h>
#include <LibWebView/CanonicalWindow.h>
#include <LibWebView/Profile.h>
#include <LibWebView/WebContentClient.h>
#include <LibWebView/WebContentPage.h>
#include <LibWebView/WebWorkerClient.h>
#include <QDir>
#include <QFileInfo>
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

    StringView file_access_error(Error const& error)
    {
        if (!error.is_errno())
            return "UnknownError"sv;

        switch (error.code()) {
        case EBUSY:
        case EAGAIN:
            return "InvalidStateError"sv;
        case ENOENT:
            return "NotFoundError"sv;
        case EISDIR:
        case ENOTDIR:
            return "TypeMismatchError"sv;
        case ENOTEMPTY:
        case EEXIST:
        case EXDEV:
            return "InvalidModificationError"sv;
        case EBADF:
            return "InvalidStateError"sv;
        case ENOLCK:
        case EROFS:
            return "NoModificationAllowedError"sv;
        case EACCES:
        case EPERM:
            return "NotAllowedError"sv;
        case ENOTSUP:
            return "NotSupportedError"sv;
        case EINVAL:
            return "TypeError"sv;
        case E2BIG:
        case ENOSPC:
            return "QuotaExceededError"sv;
        default:
            return "UnknownError"sv;
        }
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
            fail(file_access_error(error));
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

    bool consume_picker_activation(WebView::WebContentPage& page)
    {
        if (!page.consume_transient_user_activation())
            return false;

        page.traversable().top_level_traversable().for_each_hosting_page([](auto& host) {
            host.async_consume_user_activation(Web::HTML::UserActivationConsumption::Transient);
        });
        return true;
    }

    template <typename Responder>
    void dispatch_handle_operation(Responder const& reply, FileAccessGrants& grants,
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

            if (result.is_error()) {
                if (operation.is_one_of("write"sv, "truncate"sv))
                    (void)grants.finish_write(reply.document, *token, false);
                reply.fail(result.error());
            } else {
                reply.send({});
            }

            return;
        }

        if (operation == "release"sv) {
            grants.release(reply.document, *token);
            reply.send({});
            return;
        }

        if (operation == "queryPermission"sv) {
            auto result = grants.permission(
                reply.document, *token, options.get_bool("writable"sv).value_or(false));
            if (result.is_error()) {
                reply.fail(result.error());
                return;
            }

            JsonObject metadata;
            metadata.set("permission"sv, result.value());
            reply.send(move(metadata));
            return;
        }

        if (operation.is_one_of("sameEntry"sv, "resolve"sv)) {
            auto other = options.get_string("other"sv);
            if (!other.has_value() || other->byte_count() != 36) {
                reply.fail("TypeError"sv);
                return;
            }

            JsonObject metadata;
            if (operation == "sameEntry"sv) {
                auto result = grants.is_same_entry(reply.document, *token, *other);
                if (result.is_error()) {
                    reply.fail(result.error());
                    return;
                }

                metadata.set("same"sv, result.value());
            } else {
                auto result = grants.resolve(reply.document, *token, *other);
                if (result.is_error()) {
                    reply.fail(result.error());
                    return;
                }

                if (result.value().has_value()) {
                    JsonArray path;
                    for (auto const& component : *result.value())
                        MUST(path.append(component));
                    metadata.set("path"sv, move(path));
                } else {
                    metadata.set("path"sv, JsonValue {});
                }
            }

            reply.send(move(metadata));
            return;
        }

        if (operation == "remove"sv) {
            auto name = options.get_string("name"sv);
            if (!name.has_value()) {
                reply.fail("TypeError"sv);
                return;
            }

            auto result = grants.remove(reply.document, *token, name->bytes_as_string_view(),
                options.get_bool("recursive"sv).value_or(false));
            if (result.is_error())
                reply.fail(result.error());
            else
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
                *kind == "file"sv ? Kind::File : Kind::Directory,
                options.get_bool("create"sv).value_or(false));
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

            auto before = grants.file_version(reply.document, *token);
            if (before.is_error()) {
                reply.fail(before.error());
                return;
            }

            auto result = grants.read(reply.document, *token, *offset, static_cast<size_t>(*count));
            if (result.is_error()) {
                reply.fail(result.error());
                return;
            }

            auto after = grants.file_version(reply.document, *token);
            if (after.is_error() || after.value().fingerprint != before.value().fingerprint) {
                reply.fail("NotReadableError"sv);
                return;
            }

            JsonObject metadata;
            auto revision = WebView::Profile::routing_identifier(
                after.value().fingerprint.bytes_as_string_view());
            metadata.set("revision"sv, revision.view());
            metadata.set("lastModified"sv, after.value().last_modified);
            if (*offset == 0) {
                auto name = grants.name(reply.document, *token);
                if (!name.is_error()) {
                    auto mime = QMimeDatabase().mimeTypeForFile(
                        qt_string(name.value()), QMimeDatabase::MatchExtension);
                    if (mime.isValid() && !mime.isDefault()) {
                        auto type = mime.name().toUtf8();
                        metadata.set("type"sv, StringView(type.constData(), type.size()));
                    }
                }
            }

            reply.send(move(metadata), result.release_value());
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

    struct WorkerReply {
        static constexpr Document document { 1, 1 };
        mutable JsonObject metadata;
        mutable ByteBuffer data;

        void send(JsonObject value, ByteBuffer bytes = {}) const
        {
            metadata = move(value);
            data = move(bytes);
        }

        void fail(StringView error) const
        {
            metadata.set("error"sv, error);
        }

        void fail(Error const& error) const
        {
            fail(file_access_error(error));
        }

        Messages::WebWorkerClient::RequestOriginPrivateFileAccessResponse take()
        {
            return { move(metadata), move(data) };
        }
    };

    // The worker connection has its own authenticated environment and token namespace.
    // Its registry never accepts browser picker grants or renderer-supplied paths.
    NeverDestroyed<std::unordered_map<WebView::WebWorkerClient*, std::unique_ptr<FileAccessGrants>>>
        worker_grants;

    void dispatch_sync_operation(WorkerReply const& reply, FileAccessGrants& grants,
        StringView operation, JsonObject const& options, ReadonlyBytes bytes)
    {
        auto token = options.get_string("token"sv);
        if (!token.has_value() || token->byte_count() != 36) {
            reply.fail("TypeError"sv);
            return;
        }

        if (operation == "syncOpen"sv) {
            auto mode = options.get_string("mode"sv);
            if (!mode.has_value()
                || !mode->bytes_as_string_view().is_one_of(
                    "read-only"sv, "readwrite"sv, "readwrite-unsafe"sv)) {
                reply.fail("TypeError"sv);
                return;
            }

            auto opened = grants.begin_sync(
                reply.document, *token, *mode != "read-only"sv, *mode == "readwrite"sv);
            if (opened.is_error()) {
                reply.fail(opened.error());
                return;
            }

            JsonObject result;
            result.set("sync"sv, opened.release_value());
            reply.send(move(result));
            return;
        }

        if (operation == "syncSize"sv) {
            auto size = grants.sync_size(reply.document, *token);
            if (size.is_error()) {
                reply.fail(size.error());
                return;
            }

            JsonObject result;
            result.set("size"sv, size.value());
            reply.send(move(result));
            return;
        }

        if (operation == "syncRead"sv) {
            auto offset = options.get_u64("offset"sv);
            auto count = options.get_u64("count"sv);
            if (!offset.has_value() || !count.has_value()
                || *count > FileAccessGrants::max_read_size) {
                reply.fail("TypeError"sv);
                return;
            }

            auto result = grants.sync_read(reply.document, *token, *offset, *count);
            if (result.is_error())
                reply.fail(result.error());
            else
                reply.send({}, result.release_value());
            return;
        }

        auto mutate = [&]() -> ErrorOr<void> {
            if (operation == "syncWrite"sv) {
                auto offset = options.get_u64("offset"sv);
                if (!offset.has_value())
                    return Error::from_errno(EINVAL);
                return grants.sync_write(reply.document, *token, *offset, bytes);
            }

            if (operation == "syncTruncate"sv) {
                auto size = options.get_u64("size"sv);
                if (!size.has_value())
                    return Error::from_errno(EINVAL);
                return grants.sync_truncate(reply.document, *token, *size);
            }

            if (operation == "syncFlush"sv)
                return grants.sync_flush(reply.document, *token);
            if (operation == "syncClose"sv) {
                grants.close_sync(reply.document, *token);
                return {};
            }

            return Error::from_errno(ENOTSUP);
        };

        auto result = mutate();
        if (result.is_error())
            reply.fail(result.error());
        else
            reply.send({});
    }

    Messages::WebWorkerClient::RequestOriginPrivateFileAccessResponse receive_worker_request(
        WebView::WebWorkerClient& worker, Web::HTML::EnvironmentId const& environment,
        JsonValue metadata, ByteBuffer bytes)
    {
        WorkerReply reply;
        auto settings = worker.hosted_environment(environment);
        auto top = settings.has_value() ? settings->top_level_origin() : Optional<URL::Origin> {};
        if (!worker.session() || !settings.has_value()
            || (top.has_value() && !settings->origin().is_same_origin(*top))
            || settings->origin().is_opaque()
            || Web::SecureContexts::is_origin_potentially_trustworthy(settings->origin())
                != Web::SecureContexts::Trustworthiness::PotentiallyTrustworthy) {
            reply.fail("SecurityError"sv);
            return reply.take();
        }

        if (!FileAccessGrants::supported() || worker.is_private() == WebView::IsPrivate::Yes) {
            reply.fail("NotSupportedError"sv);
            return reply.take();
        }

        if (!metadata.is_object() || bytes.size() > FileAccessGrants::max_read_size) {
            reply.fail("TypeError"sv);
            return reply.take();
        }

        auto options = move(metadata.as_object());
        auto operation = options.get_string("operation"sv);
        if (!operation.has_value()
            || (!operation->bytes_as_string_view().is_one_of("write"sv, "syncWrite"sv)
                && !bytes.is_empty())) {
            reply.fail("TypeError"sv);
            return reply.take();
        }

        auto position = worker_grants->find(&worker);
        if (*operation == "getPrivateDirectory"sv) {
            auto storage_key = WebView::obtain_a_storage_key(*settings);
            if (!storage_key.has_value()) {
                reply.fail("SecurityError"sv);
                return reply.take();
            }

            auto const& profile = WebView::Application::profile();
            auto const& path = profile.paths().data;
            auto canonical_data = QFileInfo(QString::fromUtf8(path.characters(), path.length()))
                                      .canonicalFilePath()
                                      .toUtf8();
            auto canonical_key = storage_key->to_string();
            auto key = WebView::Profile::routing_identifier(canonical_key.bytes_as_string_view());
            if (position == worker_grants->end()) {
                if (worker_grants->size() >= 128) {
                    reply.fail("QuotaExceededError"sv);
                    return reply.take();
                }

                position
                    = worker_grants->emplace(&worker, std::make_unique<FileAccessGrants>()).first;
            }

            auto directory = position->second->admit_private(WorkerReply::document,
                StringView(canonical_data.constData(), canonical_data.size()), key.view());
            if (directory.is_error())
                reply.fail(directory.error());
            else
                reply.send(handle_info(directory.value()));
            return reply.take();
        }

        if (position == worker_grants->end()) {
            reply.fail("NotAllowedError"sv);
            return reply.take();
        }

        auto name = *operation == "requestPermission"sv ? "queryPermission"sv
                                                        : operation->bytes_as_string_view();
        if (name.starts_with("sync"sv))
            dispatch_sync_operation(reply, *position->second, name, options, bytes.bytes());
        else
            dispatch_handle_operation(reply, *position->second, name, options, bytes.bytes());
        return reply.take();
    }

}

void Bridge::Impl::initialize_file_access()
{
    WebView::WebWorkerClient::set_file_access_provider(
        receive_worker_request, [](auto& worker) { worker_grants->erase(&worker); });
    Bytes::set_file_access_request_receiver([this](
                                                IPC::Stub& stub, Bytes::FileAccessMessage message) {
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

        if (operation->starts_with_bytes("sync"sv)) {
            reply.fail("NotSupportedError"sv);
            return;
        }

        auto& picker = *state.file_picker;
        auto const& profile = WebView::Application::profile();
        auto const& data_path = profile.paths().data;
        auto canonical_data
            = QFileInfo(QString::fromUtf8(data_path.characters(), data_path.length()))
                  .canonicalFilePath();
        if (canonical_data.isEmpty()) {
            reply.fail("InvalidStateError"sv);
            return;
        }

        auto private_path = QDir(canonical_data).filePath("ElytraOriginFiles").toUtf8();
        auto private_area
            = String::from_utf8(StringView(private_path.constData(), private_path.size()));
        if (private_area.is_error()) {
            reply.fail(private_area.error());
            return;
        }

        picker.grants().set_private_area(private_area.release_value());
        if (*operation == "getPrivateDirectory"sv) {
            if (page->client().session().is_private() == WebView::IsPrivate::Yes) {
                reply.fail("NotSupportedError"sv);
                return;
            }

            auto document = page->hosted_document_with_id(reply.document);
            auto const& settings = document->relevant_global_object().relevant_settings_object();
            auto storage_key = WebView::obtain_a_storage_key(settings);
            if (!storage_key.has_value()) {
                reply.fail("SecurityError"sv);
                return;
            }

            auto canonical_key = storage_key->to_string();
            auto key = WebView::Profile::routing_identifier(canonical_key.bytes_as_string_view());
            auto canonical_bytes = canonical_data.toUtf8();
            auto root = picker.grants().admit_private(reply.document,
                StringView(canonical_bytes.constData(), canonical_bytes.size()), key.view());
            if (root.is_error())
                reply.fail(root.error());
            else
                reply.send(handle_info(root.value()));
            return;
        }

        if (*operation == "requestPermission"sv) {
            auto token = options.get_string("token"sv);
            if (!token.has_value() || token->byte_count() != 36) {
                reply.fail("TypeError"sv);
                return;
            }

            auto status = picker.grants().permission(
                reply.document, *token, options.get_bool("writable"sv).value_or(false));
            if (status.is_error()) {
                reply.fail(status.error());
                return;
            }

            if (status.value() != "prompt"sv) {
                JsonObject result;
                result.set("permission"sv, status.value());
                reply.send(move(result));
                return;
            }

            if (!consume_picker_activation(*page)) {
                reply.fail("SecurityError"sv);
                return;
            }

            auto document = page->hosted_document_with_id(reply.document);
            auto origin = document->origin().serialize();
            auto name = picker.grants().name(reply.document, *token);
            if (name.is_error()) {
                reply.fail(name.error());
                return;
            }

            bool shown = picker.request_write_permission(qt_string(origin), qt_string(name.value()),
                [this, reply, token = *token, picker = QPointer<FilePicker> { &picker }](
                    bool granted) {
                    if (!picker || !permission_contents(*reply.page, reply.document)) {
                        reply.fail("AbortError"sv);
                        return;
                    }

                    auto stored
                        = picker->grants().set_write_permission(reply.document, token, granted);
                    if (stored.is_error()) {
                        reply.fail(stored.error());
                        return;
                    }

                    JsonObject result;
                    result.set("permission"sv, granted ? "granted"sv : "denied"sv);
                    reply.send(move(result));
                });
            if (!shown)
                reply.fail("InvalidStateError"sv);
            return;
        }

        if (!operation->bytes_as_string_view().is_one_of("open"sv, "directory"sv, "save"sv)) {
            dispatch_handle_operation(reply, state.file_picker->grants(),
                operation->bytes_as_string_view(), options, message.bytes.bytes());
            return;
        }

        FilePicker::Selection selection {};
        if (*operation == "directory"sv) {
            selection.mode = FilePicker::Mode::Directory;
            selection.writable_directory = options.get_bool("writable"sv).value_or(false);
        } else if (*operation == "save"sv) {
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

        auto location_id = options.get_string("id"sv);
        if (location_id.has_value()) {
            auto identifier = location_id->bytes_as_string_view();
            if (identifier.length() > 32) {
                reply.fail("TypeError"sv);
                return;
            }

            for (auto character : identifier) {
                if (!is_ascii_alphanumeric(character) && character != '_' && character != '-') {
                    reply.fail("TypeError"sv);
                    return;
                }
            }
        }

        auto origin = page->hosted_document_with_id(reply.document)->origin().serialize();
        selection.reminder_key = qt_string(origin) + '|'
            + (location_id.has_value() ? qt_string(*location_id) : QString {});
        auto remembered = picker.remembered_directory(selection.reminder_key);
        if (!remembered.isEmpty())
            selection.initial_directory = move(remembered);

        if (auto start = options.get_string("startHandle"sv); start.has_value()) {
            auto directory = picker.grants().directory_hint(reply.document, *start);
            if (directory.is_error()) {
                reply.fail(directory.error());
                return;
            }

            selection.initial_directory = qt_string(directory.value());
        }

        if (!consume_picker_activation(*page)) {
            reply.fail("SecurityError"sv);
            return;
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
    WebView::WebWorkerClient::set_file_access_provider({}, {});
    worker_grants->clear();
    Bytes::set_file_access_request_receiver({});
}

}
