"""Reversible permission-request integration in the pinned Rust Notifications leaf.

The browser's store/query outcome is not an embedder request decision. Preserve
native activation checks and task/GC ownership; only owned sessions select their
independent request admission and callback outcome.
"""

import hashlib

PINS = {
    "notifications_api/mod.rs": "699c6ab5a9efe83ec70939a91216166f7d99f3deb961b8160c2bb5aa3806862b",
    "notifications_api/notification.rs": "37d6da3e7e63aa7fd28444d4c616c2f8d3c74252a02e52ca77a61d40e26fac92",
}

EXTERN = """unsafe extern "C" {
    fn firefly_renderer_notification_request_state(settings: *mut core::ffi::c_void) -> u8;
    fn firefly_renderer_permissions_active() -> bool;
"""
INITIAL = """        // The native embedder returns 255 when it does not own this session.
        // Its independent request state still goes through every upstream
        // top-level-origin and transient-activation check below.
        // SAFETY: request.settings retains the actual settings cell on this VM.
        let firefly_request_state = unsafe { firefly_renderer_notification_request_state(settings.as_ptr()) };
        let current_state = match firefly_request_state {
            0 => PermissionState::Granted,
            1 => PermissionState::Denied,
            2 => PermissionState::Prompt,
            255 => get_the_notifications_permission_state(&settings),
            _ => PermissionState::Denied,
        };"""
SETTLE = """        let permission_state = if unsafe { firefly_renderer_permissions_active() } {
            // Refusals before the actual request cannot acquire a grant. A
            // request callback supplies its own outcome, never a query reread.
            decision.unwrap_or(PermissionState::Denied)
        } else {
            get_the_notifications_permission_state(&request.settings)
        };"""
STUBS = """
// Owned permission hooks are inactive in standalone upstream Rust unit tests.
#[unsafe(no_mangle)]
extern "C" fn firefly_renderer_notification_request_state(_: *mut core::ffi::c_void) -> u8 {
    255
}
#[unsafe(no_mangle)]
extern "C" fn firefly_renderer_permissions_active() -> bool {
    false
}
"""


def replacements():
    return [
        ('unsafe extern "C" {\n', EXTERN),
        ("        let current_state = get_the_notifications_permission_state(&settings);", INITIAL),
        (
            "                Self::settle_permission_request(vm, request.clone());",
            "                Self::settle_permission_request(vm, request.clone(), None);",
        ),
        (
            "            |vm, request, _| {\n                Self::settle_permission_request(vm, request);",
            "            |vm, request, decision| {\n                Self::settle_permission_request(vm, request, Some(decision));",
        ),
        (
            "    fn settle_permission_request(vm: &Vm, request: PermissionRequest) {",
            "    fn settle_permission_request(vm: &Vm, request: PermissionRequest, decision: Option<PermissionState>) {",
        ),
        (
            "        let permission_state = get_the_notifications_permission_state(&request.settings);",
            SETTLE,
        ),
    ]


def compose(original):
    if hashlib.sha256(original.encode()).hexdigest() != PINS["notifications_api/notification.rs"]:
        raise ValueError("Review pinned Rust notification permission source")
    output = original
    for before, after in replacements():
        if output.count(before) != 1:
            raise ValueError("Missing/ambiguous Rust notification permission seam")
        output = output.replace(before, after, 1)
    restored = output
    for before, after in reversed(replacements()):
        if restored.count(after) != 1:
            raise ValueError("Ambiguous Rust notification permission reversal")
        restored = restored.replace(after, before, 1)
    if restored != original:
        raise ValueError("Rust permission composition changed unrelated upstream bytes")
    return output


def compose_stubs(original):
    if (
        "firefly_renderer_permissions_active" in original
        or "firefly_renderer_notification_request_state" in original
    ):
        raise ValueError("Duplicate permission test ABI")
    return original + STUBS


def verify(source):
    for name, expected in PINS.items():
        if hashlib.sha256((source / name).read_bytes()).hexdigest() != expected:
            raise ValueError(f"Review pinned permission Rust source: {name}")
