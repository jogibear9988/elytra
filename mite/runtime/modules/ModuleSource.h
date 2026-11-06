/**
 * Shared source/path conversion helpers for the module loader. Native errors are translated at
 * the boundary rather than evaluated as JavaScript source.
 */

#pragma once
#include <LibGC/Cell.h>
#include <LibGC/CellAllocator.h>
#include <string>

namespace Mite {
class ModuleSource final : public GC::Cell {
    GC_CELL(ModuleSource, GC::Cell);

    GC_DECLARE_ALLOCATOR(ModuleSource);

public:
    explicit ModuleSource(std::string identity)
        : url(std::move(identity))
    {
    }

    std::string const url;
};

}
