//! Assigning board values to the storage of `device.ts` fields.

use crate::bridge::Value;
use alloc::{string::String, vec::Vec};

/// A `device.ts` field type a board value can be written into.
pub trait Field {
    fn assign(&mut self, value: &Value<'_>);
}

macro_rules! numbers {
    ($($kind:ty),*) => {
        $(impl Field for $kind {
            fn assign(&mut self, value: &Value<'_>) {
                // `as` saturates float-to-integer, as MicroTS conversions do.
                *self = value.number() as $kind;
            }
        })*
    };
}
numbers!(i8, i16, i32, i64, u8, u16, u32, u64, f32, f64);

impl Field for bool {
    fn assign(&mut self, value: &Value<'_>) {
        *self = value.boolean();
    }
}

impl Field for String {
    fn assign(&mut self, value: &Value<'_>) {
        self.clear();
        self.push_str(value.text());
    }
}

macro_rules! arrays {
    ($($kind:ty),*) => {
        $(impl Field for Vec<$kind> {
            fn assign(&mut self, value: &Value<'_>) {
                self.clear();
                self.extend(value.array().iter().map(|element| *element as $kind));
            }
        })*
    };
}
arrays!(i8, i16, i32, u8, u16, u32, f32, f64);
