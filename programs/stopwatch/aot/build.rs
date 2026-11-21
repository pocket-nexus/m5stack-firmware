//! Build script of the StopWatch AOT crate: the shared profile/font glue and
//! the shared image loader (docs/APP-HOST.md "Images"), which stores every
//! picture in the format apps/stopwatch-factory/images.json selects
//! (RGB565, RGBA4444 or RGBA8888, as the JS bundle does) instead of the
//! generated RGBA8888 `load_images`.
fn main() {
    m5_pocket_aot_build::emit();
    m5_pocket_aot_build::emit_images(
        concat!(env!("CARGO_MANIFEST_DIR"), "/../../../apps/stopwatch-factory"),
        1,
    );
}
