#include <LibJS/Runtime/ExecutionContext.h>
#include <LibJS/Runtime/Realm.h>
#include <LibJS/Runtime/VM.h>
#include <cstdio>

int main()
{
    // A moved-from owner is null. Optimizers may still invoke class deallocation
    // for its trivial-destructor delete expression, as Apple Clang does with -Oz.
    JS::ExecutionContext::operator delete(nullptr);
    auto vm = JS::VM::create();
    auto result = JS::Realm::initialize_host_defined_realm(*vm, {}, {});
    if (result.is_error())
        return 1;
    auto owner = result.release_value();
    auto* address = owner.ptr();
    auto moved = move(owner);
    if (moved.ptr() != address || !moved->realm)
        return 1;
    vm->pop_execution_context();
    puts("PASS null deallocation, real realm/context ownership and moved-from cleanup");
    return 0;
}
