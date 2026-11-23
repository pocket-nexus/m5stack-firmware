//! The exported host profile (m5_pocket_aot_build::emit). The fonts are read
//! in place by the generated `load_fonts_in_place`. The pictures use the
//! generated `load_images`: MicroTS bakes the `@2x` variants at the Tab5
//! host contract's raster density (2) in the formats `images.json` names.
fn main() {
    m5_pocket_aot_build::emit();
}
