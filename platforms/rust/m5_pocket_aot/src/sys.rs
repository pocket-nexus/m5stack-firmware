//! The host hooks of `pocket_runtime/src/hooks.c` and, on the device, the
//! global allocator and panic handler over them.
//!
//! `pocket_host_alloc` takes PSRAM first and internal RAM otherwise, so the
//! same library runs on boards with and without PSRAM.

#[cfg(target_os = "none")]
mod device {
    use core::alloc::{GlobalAlloc, Layout};

    unsafe extern "C" {
        pub fn pocket_host_alloc(size: usize, align: usize) -> *mut u8;
        pub fn pocket_host_realloc(
            pointer: *mut u8,
            old_size: usize,
            align: usize,
            new_size: usize,
        ) -> *mut u8;
        pub fn pocket_host_free(pointer: *mut u8, size: usize, align: usize);
        pub fn pocket_host_panic(message: *const u8, length: usize) -> !;
        pub fn pocket_host_log(level: i32, message: *const u8, length: usize);
        pub fn m5_pocket_profile_stage(stage: u32);
    }

    struct IdfHeap;

    unsafe impl GlobalAlloc for IdfHeap {
        unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
            unsafe { pocket_host_alloc(layout.size(), layout.align()) }
        }
        unsafe fn dealloc(&self, pointer: *mut u8, layout: Layout) {
            unsafe { pocket_host_free(pointer, layout.size(), layout.align()) }
        }
        unsafe fn realloc(&self, pointer: *mut u8, layout: Layout, new_size: usize) -> *mut u8 {
            unsafe { pocket_host_realloc(pointer, layout.size(), layout.align(), new_size) }
        }
    }

    #[global_allocator]
    static HEAP: IdfHeap = IdfHeap;

    /// Formats into a fixed stack buffer: a panic must not allocate.
    struct Message {
        bytes: [u8; 192],
        length: usize,
    }
    impl core::fmt::Write for Message {
        fn write_str(&mut self, text: &str) -> core::fmt::Result {
            let room = self.bytes.len() - self.length;
            let take = text.len().min(room);
            self.bytes[self.length..self.length + take].copy_from_slice(&text.as_bytes()[..take]);
            self.length += take;
            Ok(())
        }
    }

    #[panic_handler]
    fn panic(info: &core::panic::PanicInfo<'_>) -> ! {
        let mut message = Message {
            bytes: [0; 192],
            length: 0,
        };
        let _ = core::fmt::write(&mut message, format_args!("{info}"));
        unsafe { pocket_host_panic(message.bytes.as_ptr(), message.length) }
    }
}

/// Log a line through `pocket_host_log` (ESP_LOGI, tag `pocket_runtime`).
pub fn log(message: &str) {
    #[cfg(target_os = "none")]
    unsafe {
        device::pocket_host_log(2, message.as_ptr(), message.len())
    }
    #[cfg(not(target_os = "none"))]
    let _ = message;
}

/// Frame profiler stages (`m5_pocket_app` src/profile.h) the library enters.
pub mod stage {
    pub const BRIDGE: u32 = 2;
    pub const TRANSLATE: u32 = 3;
    pub const APP_INPUT: u32 = 4;
    pub const MODEL: u32 = 5;
    pub const VIEW: u32 = 6;
    pub const REQUESTS: u32 = 7;
    pub const DRAW: u32 = 8;
}

/// Enter a frame profiler stage: `m5_pocket_profile_stage`, a no-op without
/// `CONFIG_M5_POCKET_APP_PROFILE`.
pub fn enter(stage: u32) {
    #[cfg(target_os = "none")]
    unsafe {
        device::m5_pocket_profile_stage(stage)
    }
    #[cfg(not(target_os = "none"))]
    let _ = stage;
}
