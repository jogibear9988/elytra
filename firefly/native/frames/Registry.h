/** Native canonical frame authority. No renderer-supplied identity or URL is admitted. */
#pragma once
#include <AK/ByteString.h>
#include <AK/Error.h>
#include <AK/Optional.h>
#include <AK/WeakPtr.h>
#include <LibWebCommon/HTML/CrossProcessId.h>
#include <vector>

namespace WebView {
class CanonicalDocument;
class CanonicalNavigable;
class CanonicalTraversable;
class WebContentPage;
}

namespace Firefly::Frames {
class Registry {
public:
    struct Authority {
        int contents;
        int token;
        int tree_node;
        int process;
        int os_process;
        Web::HTML::CrossProcessId document;
        Web::HTML::CrossProcessId navigable;
        WeakPtr<WebView::CanonicalDocument> owner;
    };

    ErrorOr<ByteString> snapshot(int contents, WebView::CanonicalTraversable*);

    Optional<Authority> resolve(int token) const;

    void invalidate(int contents, bool forget_tree = false);

    void clear();

private:
    struct Record {
        Authority authority;
        WeakPtr<WebView::CanonicalNavigable> navigable;
    };

    struct Tree {
        int contents;
        int id;
        WeakPtr<WebView::CanonicalNavigable> navigable;
    };

    std::vector<Record> m_records;
    std::vector<Tree> m_trees;
    int m_next_token {};
    int m_next_tree {};
};
}
