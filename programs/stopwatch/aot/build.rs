//! Build script of the StopWatch AOT crate: the exported host profile. The
//! fonts and pictures are read in place by the generated
//! `load_fonts_in_place` and `load_images_in_place` (docs/APP-HOST.md
//! "Images"): the pictures MicroTS bakes in the formats
//! apps/stopwatch-factory/images.json selects stay in flash until a frame
//! draws them, instead of the heap copies the generated `load_images` makes.
fn main() {
    m5_pocket_aot_build::emit();
}
