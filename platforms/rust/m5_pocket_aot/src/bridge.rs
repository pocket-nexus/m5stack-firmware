//! The consumer side of `m5_pocket/bridge.h`.

use core::ffi::c_char;

const KIND_TEXT: u32 = 2;
const KIND_ARRAY: u32 = 3;

/// `m5_pocket_value`.
#[repr(C)]
struct RawValue {
    struct_size: usize,
    name: *const c_char,
    name_length: usize,
    kind: u32,
    number: f64,
    text: *const c_char,
    text_length: usize,
    array: *const f32,
    array_length: usize,
}

impl RawValue {
    const fn empty() -> Self {
        Self {
            struct_size: core::mem::size_of::<Self>(),
            name: core::ptr::null(),
            name_length: 0,
            kind: 0,
            number: 0.0,
            text: core::ptr::null(),
            text_length: 0,
            array: core::ptr::null(),
            array_length: 0,
        }
    }
}

unsafe extern "C" {
    fn m5_pocket_bridge_next_value(out: *mut RawValue) -> bool;
    fn m5_pocket_bridge_next_event(out: *mut RawValue) -> bool;
    fn m5_pocket_bridge_dispatch_request(text: *const c_char, length: usize) -> i32;
}

/// A board value's content.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Data<'a> {
    Number(f64),
    Text(&'a str),
    Array(&'a [f32]),
}

/// One board value or event, borrowed until the next one is taken.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Value<'a> {
    /// The `device.ts` field name the board published under.
    pub name: &'a str,
    pub data: Data<'a>,
}

impl Value<'_> {
    /// The number; 0 for text and arrays.
    pub fn number(&self) -> f64 {
        match self.data {
            Data::Number(value) => value,
            _ => 0.0,
        }
    }
    pub fn boolean(&self) -> bool {
        self.number() != 0.0
    }
    /// The text; empty for numbers and arrays.
    pub fn text(&self) -> &str {
        match self.data {
            Data::Text(text) => text,
            _ => "",
        }
    }
    /// The array; empty for numbers and text.
    pub fn array(&self) -> &[f32] {
        match self.data {
            Data::Array(values) => values,
            _ => &[],
        }
    }
}

/// # Safety
/// `raw` was filled by the bridge; its pointers live until the next take.
unsafe fn view(raw: &RawValue) -> Option<Value<'_>> {
    let bytes = |pointer: *const c_char, length: usize| -> &[u8] {
        if pointer.is_null() || length == 0 {
            &[]
        } else {
            unsafe { core::slice::from_raw_parts(pointer.cast::<u8>(), length) }
        }
    };
    let name = core::str::from_utf8(bytes(raw.name, raw.name_length)).ok()?;
    let data = match raw.kind {
        KIND_TEXT => Data::Text(core::str::from_utf8(bytes(raw.text, raw.text_length)).ok()?),
        KIND_ARRAY => Data::Array(if raw.array.is_null() || raw.array_length == 0 {
            &[]
        } else {
            unsafe { core::slice::from_raw_parts(raw.array, raw.array_length) }
        }),
        _ => Data::Number(raw.number),
    };
    Some(Value { name, data })
}

fn drain(next: unsafe extern "C" fn(*mut RawValue) -> bool, mut each: impl FnMut(&Value<'_>)) {
    let mut raw = RawValue::empty();
    while unsafe { next(&mut raw) } {
        if let Some(value) = unsafe { view(&raw) } {
            each(&value);
        }
        raw = RawValue::empty();
    }
}

/// Every board value changed since the previous call, once each.
pub fn for_each_value(each: impl FnMut(&Value<'_>)) {
    drain(m5_pocket_bridge_next_value, each);
}

/// Every queued board event, in order.
pub fn for_each_event(each: impl FnMut(&Value<'_>)) {
    drain(m5_pocket_bridge_next_event, each);
}

/// Pass one request string to the program's handler; false without one.
pub fn request(text: &str) -> bool {
    if text.is_empty() {
        return false;
    }
    unsafe { m5_pocket_bridge_dispatch_request(text.as_ptr().cast(), text.len()) == 0 }
}
