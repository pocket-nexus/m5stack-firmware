//! Build script of the StopWatch AOT crate: the shared profile and in-place
//! font glue, and the in-place image loader (docs/APP-HOST.md "Images"):
//! the pictures MicroTS bakes in the formats apps/stopwatch-factory/
//! images.json selects stay in flash until a frame draws them, instead of
//! the heap copies the generated `load_images` makes.
fn main() {
    m5_pocket_aot_build::emit();
    m5_pocket_aot_build::emit_images();
}
