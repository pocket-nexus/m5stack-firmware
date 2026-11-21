//! Profile and fonts, then the factory pictures at the raster density of the
//! Tab5 host contract (2): the `@2x` variants in the formats `images.json`
//! names (m5_pocket_aot_build::emit_images), not the generated 1x RGBA8888
//! `load_images`.
fn main() {
    m5_pocket_aot_build::emit();
    m5_pocket_aot_build::emit_images(
        concat!(env!("CARGO_MANIFEST_DIR"), "/../../../apps/tab5-factory"),
        2,
    );
}
