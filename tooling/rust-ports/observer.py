"""Exact native observer composition; unrelated MessagePort behavior remains byte-reversible."""

import hashlib

PINS = {
    "html/message_port.rs": "f69cec9147e28c0b3410eefb323d5288ee1df9c184749cc88c987998cb4ef82e",
    "html/mod.rs": "794449fb55e801464f1decbb965dc323ac9718b54c147d72af87ec7e56c615d0",
    "ffi_test_stubs.rs": "7ffb70a758530761ceb7561fcf2cffc26c7fa5856706b9dfaee7f6393ded171e",
}
FIELDS = """    // Native-only observation follows legitimate transfers, never JS properties.
    firefly_observer_id: Cell<u64>,
    firefly_observer_high: Cell<u64>,
    firefly_observer_low: Cell<u64>,
    firefly_observer_consume: Cell<bool>,
    firefly_observer_peer_closed: Cell<bool>,

"""
INIT = """            firefly_observer_id: Cell::new(0),
            firefly_observer_high: Cell::new(0),
            firefly_observer_low: Cell::new(0),
            firefly_observer_consume: Cell::new(false),
            firefly_observer_peer_closed: Cell::new(false),
"""
METHODS = """    // Only real close/finalize/disposal enters this path. Transfers and
    // transport conversions never report a spurious native close.
    fn firefly_observer_closing(&self) {
        if self.firefly_observer_id.get() != 0 {
            if let Some(peer) = self.remote_port.get() {
                if peer.firefly_observer_id.get() != 0 && !peer.firefly_observer_consume.get() {
                    peer.firefly_observer_peer_closed.set(true);
                    peer.firefly_observer_finish_peer_close();
                }
            }
        }
        self.firefly_observer_closed();
    }

    fn firefly_observer_finish_peer_close(&self) {
        if self.firefly_observer_peer_closed.get() && !self.firefly_observer_consume.get()
            && self.port_message_queue.borrow().is_empty() {
            self.firefly_observer_closed();
        }
    }

    fn firefly_observer_closed(&self) {
        let id = self.firefly_observer_id.replace(0);
        let high = self.firefly_observer_high.replace(0);
        let low = self.firefly_observer_low.replace(0);
        self.firefly_observer_consume.set(false);
        self.firefly_observer_peer_closed.set(false);
        if id != 0 {
            // Native atomic state only; safe even during a GC finalizer.
            unsafe { firefly_port_observer_notify(high, low, id, 2) };
        }
    }

    fn firefly_observer_dequeued(&self) {
        let id = self.firefly_observer_id.get();
        if id != 0 && self.firefly_observer_consume.get() {
            unsafe { firefly_port_observer_notify(self.firefly_observer_high.get(), self.firefly_observer_low.get(), id, 1) };
        }
    }

"""
WRITE = """        // Move observer ownership only after successful native transfer steps.
        // Clearing the old cell prevents its later GC/disposal from closing the
        // observer that belongs to the receiving endpoint.
        data_holder.write(&this.firefly_observer_high.replace(0));
        data_holder.write(&this.firefly_observer_low.replace(0));
        data_holder.write(&this.firefly_observer_id.replace(0));
        data_holder.write_bool(this.firefly_observer_consume.replace(false));
        data_holder.write_bool(this.firefly_observer_peer_closed.replace(false));

"""
RECEIVE_ORIGINAL = """        if data_holder.read_bool()? {
            let mut transport = data_holder.read_transport()?.create_transport();"""
RECEIVE = """        let firefly_transport = if data_holder.read_bool()? {
            Some(data_holder.read_transport()?)
        } else {
            None
        };
        let firefly_high: u64 = data_holder.read()?;
        let firefly_low: u64 = data_holder.read()?;
        let firefly_id: u64 = data_holder.read()?;
        let firefly_consume = data_holder.read_bool()?;
        let firefly_peer_closed = data_holder.read_bool()?;
        if firefly_id == 0 {
            if firefly_high != 0 || firefly_low != 0 || firefly_consume || firefly_peer_closed {
                value.close_steps();
                return Err(Exception::Dom("DataCloneError", Utf16String::from_ascii(b"Invalid native port observation")));
            }
        } else {
            // Destination realm is the actual port realm, never a JS label or
            // caller-selected wrapper realm. Foreign agent/VM binding fails closed.
            let admitted = unsafe { firefly_port_observer_bind(
                (vm as *const Vm).cast_mut().cast(), value.realm.as_ptr().cast(), value.as_ptr().cast(),
                firefly_high, firefly_low, firefly_id, firefly_consume,
            ) };
            if !admitted {
                value.close_steps();
                return Err(Exception::Dom("DataCloneError", Utf16String::from_ascii(b"Native port destination was revoked")));
            }
            value.firefly_observer_high.set(firefly_high);
            value.firefly_observer_low.set(firefly_low);
            value.firefly_observer_id.set(firefly_id);
            value.firefly_observer_consume.set(firefly_consume);
            value.firefly_observer_peer_closed.set(firefly_peer_closed);
            value.firefly_observer_finish_peer_close();
        }
        if let Some(firefly_transport) = firefly_transport {
            let mut transport = firefly_transport.create_transport();"""
EXTRA = """
// Owned native observation ABI. No JavaScript method or property is installed.
unsafe extern "C" {
    fn firefly_port_observer_bind(vm: *mut c_void, realm: *mut c_void, port: *mut c_void,
        high: u64, low: u64, id: u64, consume: bool) -> bool;
    fn firefly_port_observer_notify(high: u64, low: u64, id: u64, kind: u8);
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn firefly_message_port_observe(vm: *mut c_void, port: *mut c_void,
    high: u64, low: u64, id: u64, consume: bool) -> bool {
    // SAFETY: The owning native registry checks thread and VM before invoking
    // this entry; the caller lends a live MessagePort cell for the call.
    let this = unsafe { crate::bindings::interface::cell_from_abi::<MessagePort>(port) };
    if id == 0 || this.detached.get() {
        return false;
    }
    if this.firefly_observer_id.get() != 0 && !(this.firefly_observer_id.get() == id
        && this.firefly_observer_high.get() == high && this.firefly_observer_low.get() == low
        && this.firefly_observer_consume.get() == consume) {
        return false;
    }
    if !unsafe { firefly_port_observer_bind(vm, this.realm.as_ptr().cast(), port, high, low, id, consume) } {
        return false;
    }
    this.firefly_observer_high.set(high);
    this.firefly_observer_low.set(low);
    this.firefly_observer_id.set(id);
    this.firefly_observer_consume.set(consume);
    true
}
"""
STUBS = """
// Owned test-only ABI stubs: Rust unit tests do not link a native VM/observer registry.
#[unsafe(no_mangle)]
extern "C" fn firefly_port_observer_bind(_: *mut core::ffi::c_void, _: *mut core::ffi::c_void,
    _: *mut core::ffi::c_void, _: u64, _: u64, _: u64, _: bool) -> bool { false }
#[unsafe(no_mangle)]
extern "C" fn firefly_port_observer_notify(_: u64, _: u64, _: u64, _: u8) {}
"""


def replacements():
    return [
        (
            "    // https://html.spec.whatwg.org/multipage/structured-data.html#detached\n",
            FIELDS
            + "    // https://html.spec.whatwg.org/multipage/structured-data.html#detached\n",
        ),
        (
            "            detached: Cell::new(false),",
            INIT + "            detached: Cell::new(false),",
        ),
        ("impl MessagePort {\n", "impl MessagePort {\n" + METHODS),
        (
            "    fn finalize(port: Ref<Self>) {\n        port.disentangle();",
            "    fn finalize(port: Ref<Self>) {\n        port.firefly_observer_closing();\n        port.disentangle();",
        ),
        # Realm and whole-agent disposal share these exact actual teardown loops.
        (
            "        for port in ports.values().iter() {\n            port.disentangle();",
            "        for port in ports.values().iter() {\n            port.firefly_observer_closing();\n            port.disentangle();",
        ),
        (
            "                Self::post_message_task_steps(this, vm, serialize_with_transfer_result);",
            "                this.firefly_observer_dequeued();\n                Self::post_message_task_steps(this, vm, serialize_with_transfer_result);",
        ),
        (
            "    pub(crate) fn close_steps(&self) {",
            "    pub(crate) fn close_steps(&self) {\n        self.firefly_observer_closing();",
        ),
        (
            "        if !sent {\n            this.disentangle();",
            "        if !sent {\n            this.firefly_observer_closed();\n            this.disentangle();",
        ),
        (
            "        this.clear_message_queues();\n\n        Ok(())\n    }\n\n    // https://html.spec.whatwg.org/multipage/web-messaging.html#message-ports:transfer-receiving-steps",
            "        this.clear_message_queues();\n\n"
            + WRITE
            + "        Ok(())\n    }\n\n    // https://html.spec.whatwg.org/multipage/web-messaging.html#message-ports:transfer-receiving-steps",
        ),
        (
            "                Self::post_message_task_steps(this, vm, serialize_with_transfer_result);",
            "                Self::post_message_task_steps(this, vm, serialize_with_transfer_result);\n                this.firefly_observer_finish_peer_close();",
        ),
        (RECEIVE_ORIGINAL, RECEIVE),
    ]


def compose(name, original):
    if hashlib.sha256(original.encode()).hexdigest() != PINS[name]:
        raise ValueError(f"Unreviewed native port source: {name}")
    if name == "ffi_test_stubs.rs":
        return original + STUBS
    if name != "html/message_port.rs":
        raise ValueError(f"Unknown native port leaf: {name}")
    result = original
    changes = []
    for before, after in replacements():
        expected = 2 if before.startswith("        for port in ports.values().iter()") else 1
        if result.count(before) != expected:
            raise ValueError(f"Native port source seam changed: {before}")
        result = result.replace(before, after)
        changes.append((before, after, expected))
    result += EXTRA
    restored = result.removesuffix(EXTRA)
    for before, after, expected in reversed(changes):
        if restored.count(after) != expected:
            raise ValueError("Native port reversal drift")
        restored = restored.replace(after, before)
    if restored != original:
        raise AssertionError("Native port observer changed unrelated source bytes")
    return result
