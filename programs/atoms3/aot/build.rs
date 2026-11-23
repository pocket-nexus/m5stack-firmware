//! The exported host profile (m5_pocket_aot_build). The fonts and the factory
//! artwork are read in place from flash by the generated
//! `load_fonts_in_place` and `load_images_in_place`.
//! `apps/atoms3-factory/images.json` declares every picture as RGB565
//! (`psm: 0`), so each takes 32 KiB of flash and, while drawn, 32 KiB of
//! heap under the texture budget set in `mount`.
fn main() {
    m5_pocket_aot_build::emit();
}
