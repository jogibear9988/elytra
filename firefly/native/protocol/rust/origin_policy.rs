//! Borrow the canonical process policy from LibURL; never duplicate it in Rust crate copies.
use crate::origin::Origin;
use crate::FfiOrigin;
use std::ffi::c_void;

#[cfg(not(test))]
unsafe extern "C" {
    fn firefly_url_custom_origin(
        data: *const u8,
        length: usize,
        context: *mut c_void,
        callback: unsafe extern "C" fn(*mut c_void, *const FfiOrigin),
    );
}

// Upstream Rust unit tests do not link C++. Their default empty policy is
// unchanged; actual configured policy is tested in native/helper integration.
#[cfg(test)]
unsafe fn firefly_url_custom_origin(
    _data: *const u8,
    _length: usize,
    _context: *mut c_void,
    _callback: unsafe extern "C" fn(*mut c_void, *const FfiOrigin),
) {
}

pub(crate) fn custom_origin(serialized: &str) -> Option<Origin> {
    unsafe extern "C" fn receive(context: *mut c_void, value: *const FfiOrigin) {
        // SAFETY: C++ calls synchronously and lends a complete existing FfiOrigin.
        // to_origin copies its strings before C++ releases the borrowed storage.
        unsafe { *context.cast::<Option<Origin>>() = Some((*value).to_origin()) };
    }

    let mut result = None;
    // SAFETY: input/result live through this synchronous call. The callback is
    // invoked at most once, only when the native policy has a tuple override.
    unsafe {
        firefly_url_custom_origin(
            serialized.as_ptr(),
            serialized.len(),
            (&raw mut result).cast(),
            receive,
        );
    }
    result
}
