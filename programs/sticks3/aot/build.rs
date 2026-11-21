//! Profile and fonts, then the pictures in the formats `images.json` names
//! (RGB565) at the StickS3's raster density 1 (m5_pocket_aot_build::emit_images),
//! instead of the generated RGBA8888 `load_images`.
fn main() {
    m5_pocket_aot_build::emit();
    m5_pocket_aot_build::emit_images(
        concat!(env!("CARGO_MANIFEST_DIR"), "/../../../apps/sticks3-factory"),
        1,
    );
}
