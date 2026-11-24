//! The exported host profile (m5_pocket_aot_build::emit). The fonts are read
//! in place by the generated `load_fonts_in_place`; the pictures by the
//! generated `load_images`, at the CoreP4X host contract's raster density 1.
fn main() {
    m5_pocket_aot_build::emit();
}
