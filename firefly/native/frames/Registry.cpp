/** Snapshot only active hosted canonical documents; stale tokens never resolve to a successor. */
#include "Registry.h"
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/ScopeGuard.h>
#include <LibWebView/CanonicalDocument.h>
#include <LibWebView/CanonicalNavigable.h>
#include <LibWebView/CanonicalTraversable.h>
#include <LibWebView/WebContentClient.h>
#include <LibWebView/WebContentPage.h>
#include <algorithm>
#include <limits>

namespace Firefly::Frames {
namespace {
    enum class Lineage { ActiveHosted, Inactive, Unhosted, Limit };

    Lineage lineage(WebView::CanonicalNavigable const& node)
    {
        bool active = true;
        bool hosted = true;
        size_t depth = 0;
        auto* child = &node;
        for (;;) {
            // Check depth even under an unhosted/inactive ancestor, so the
            // canonical tree's recursive walker cannot descend without bound.
            if (++depth > 128)
                return Lineage::Limit;
            auto page = child->active_document().host();
            if (!page || !page->is_open() || !page->hosts_active_document_of(*child))
                hosted = false;
            auto* parent = child->parent();
            if (!parent)
                break;
            if (child->container_document() != &parent->active_document())
                active = false;
            child = parent;
        }

        if (!active)
            return Lineage::Inactive;
        return hosted ? Lineage::ActiveHosted : Lineage::Unhosted;
    }

    bool current(auto const& entry)
    {
        auto document = entry.authority.owner.strong_ref();
        auto page = document ? document->host() : nullptr;
        auto* navigable = entry.navigable.ptr();
        return page && page->is_open() && navigable && lineage(*navigable) == Lineage::ActiveHosted
            && page->hosts_active_document_of(*navigable)
            && navigable->id() == entry.authority.navigable
            && &navigable->active_document() == document.ptr()
            && navigable->active_document().id() == entry.authority.document
            && navigable->active_document().host() == page
            && page->client().child_id() == static_cast<u64>(entry.authority.process)
            && page->client().pid() == entry.authority.os_process;
    }
}

Optional<Registry::Authority> Registry::resolve(int token) const
{
    auto found = std::find_if(m_records.begin(), m_records.end(),
        [&](auto const& record) { return record.authority.token == token; });
    if (found == m_records.end() || !current(*found))
        return {};
    return found->authority;
}

void Registry::invalidate(int contents, bool forget_tree)
{
    std::erase_if(
        m_records, [&](auto const& entry) { return entry.authority.contents == contents; });
    if (forget_tree)
        std::erase_if(m_trees, [&](auto const& entry) { return entry.contents == contents; });
}

void Registry::clear()
{
    m_records.clear();
    m_trees.clear(); // Counters never rewind, including after shutdown.
}

ErrorOr<ByteString> Registry::snapshot(int contents, WebView::CanonicalTraversable* root)
{
    bool committed = false;
    ScopeGuard invalidate_failed_snapshot = [&] {
        if (!committed)
            invalidate(contents);
    };

    std::erase_if(m_records, [&](auto const& record) { return !current(record); });
    std::erase_if(m_trees, [](auto const& tree) { return !tree.navigable; });

    struct Staged {
        WebView::CanonicalNavigable* node;
        RefPtr<WebView::WebContentPage> page;
        String url;
        String origin;
        int token {};
        int tree {};
        int parent {};
    };

    std::vector<Staged> staged;
    bool invalid = false;
    size_t visited = 0;
    if (root)
        root->for_each_in_inclusive_subtree([&](auto const& node) {
            if (++visited > 4096) {
                invalid = true;
                return IterationDecision::Break;
            }

            auto status = lineage(node);
            if (status == Lineage::Limit) {
                invalid = true;
                return IterationDecision::Break;
            }
            // A newly created frame may not have a host yet. It and its
            // descendants are not active hosted frames, never partial records.
            if (status != Lineage::ActiveHosted)
                return IterationDecision::Continue;
            if (staged.size() >= 256) {
                invalid = true;
                return IterationDecision::Break;
            }

            auto const& document = node.active_document();
            auto page = document.host();
            auto child = page->client().child_id();
            auto pid = page->client().pid();
            if (!child || child > 2147483647 || pid <= 0 || static_cast<u64>(pid) > 2147483647) {
                invalid = true;
                return IterationDecision::Break;
            }

            auto url = node.active_document_url().serialize();
            auto origin = document.origin().serialize();
            if (url.byte_count() > 64 * 1024 || origin.byte_count() > 64 * 1024) {
                invalid = true;
                return IterationDecision::Break;
            }

            staged.push_back({ const_cast<WebView::CanonicalNavigable*>(&node), page, move(url),
                move(origin), 0, 0, 0 });
            return IterationDecision::Continue;
        });
    if (invalid)
        return Error::from_string_literal(
            "Canonical active hosted frame tree exceeds limits or has invalid metadata");
    auto other_records = std::count_if(m_records.begin(), m_records.end(),
        [&](auto const& entry) { return entry.authority.contents != contents; });
    auto other_trees = std::count_if(m_trees.begin(), m_trees.end(),
        [&](auto const& entry) { return entry.contents != contents; });
    if (static_cast<size_t>(other_records) + staged.size() > 1024
        || static_cast<size_t>(other_trees) + staged.size() > 1024)
        return Error::from_string_literal("Global native frame budget exceeded");
    for (auto& frame : staged) {
        auto old = std::find_if(m_records.begin(), m_records.end(), [&](auto const& record) {
            return record.authority.contents == contents && record.navigable.ptr() == frame.node
                && record.authority.document == frame.node->active_document().id();
        });
        if (old != m_records.end())
            frame.token = old->authority.token;
        else {
            if (m_next_token == 2147483647)
                return Error::from_string_literal("Native frame token space exhausted");
            frame.token = ++m_next_token;
        }

        auto tree = std::find_if(m_trees.begin(), m_trees.end(), [&](auto const& record) {
            return record.contents == contents && record.navigable.ptr() == frame.node;
        });
        if (tree != m_trees.end())
            frame.tree = tree->id;
        else {
            if (m_next_tree == 2147483647)
                return Error::from_string_literal("Native frame tree identifier space exhausted");
            frame.tree = ++m_next_tree;
        }
    }

    JsonArray frames;
    int top = staged.empty() ? 0 : staged.front().token;
    for (auto& frame : staged) {
        if (auto* parent = frame.node->parent()) {
            auto found = std::find_if(staged.begin(), staged.end(),
                [&](auto const& candidate) { return candidate.node == parent; });
            if (found == staged.end())
                return Error::from_string_literal(
                    "Canonical frame parent is outside owned active tree");
            frame.parent = found->token;
        }

        auto doc = frame.node->active_document().id();
        JsonObject entry;
        entry.set("token"sv, frame.token);
        entry.set("document"sv, TRY(String::formatted("{}:{}", doc.namespace_id, doc.local_id)));
        entry.set("frameTreeNodeId"sv, frame.tree);
        entry.set("routingId"sv, frame.token);
        entry.set("processId"sv, frame.page->client().child_id());
        entry.set("osProcessId"sv, frame.page->client().pid());
        entry.set("url"sv, frame.url);
        entry.set("origin"sv, frame.origin);
        entry.set("parent"sv, frame.parent ? JsonValue(frame.parent) : JsonValue());
        entry.set("top"sv, top);
        TRY(frames.append(move(entry)));
    }

    JsonObject result;
    result.set("contentsId"sv, contents);
    result.set("mainFrame"sv, top ? JsonValue(top) : JsonValue());
    result.set("frames"sv, move(frames));
    auto wire = result.serialized();
    if (wire.byte_count() > 2 * 1024 * 1024)
        return Error::from_string_literal("Native frame snapshot JSON budget exceeded");
    invalidate(contents, true);
    for (auto const& frame : staged) {
        m_records.push_back(
            { { contents, frame.token, frame.tree,
                  static_cast<int>(frame.page->client().child_id()),
                  static_cast<int>(frame.page->client().pid()), frame.node->active_document().id(),
                  frame.node->id(), frame.node->active_document().make_weak_ptr() },
                frame.node->make_weak_ptr() });
        m_trees.push_back({ contents, frame.tree, frame.node->make_weak_ptr() });
    }

    committed = true;
    return wire.to_byte_string();
}
}
