//! Fonts in place and the factory artwork in flash (m5_pocket_aot_build).
//! `apps/atoms3-factory/images.json` declares every picture as RGB565
//! (`psm: 0`), so each takes 32 KiB of flash and, while drawn, 32 KiB of
//! heap under the texture budget set in `mount`.
fn main() {
    m5_pocket_aot_build::emit();
    m5_pocket_aot_build::emit_images(
        concat!(env!("CARGO_MANIFEST_DIR"), "/../../../apps/atoms3-factory"),
        1,
    );
}
