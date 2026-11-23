//! `build.rs` support for the per-app AOT crates of m5_pocket_app.
//!
//! [`emit`] places the exported host profile in `OUT_DIR/pocket_profile.rs`
//! (`pocket_profile_build`, from `POCKETJS_PROFILE_RS`).
//!
//! The generated `include.rs` brings its own loaders: `load_fonts` and
//! `load_images` copy every atlas and picture into the heap at mount,
//! `load_fonts_in_place` and `load_images_in_place` read them from the
//! `include_bytes!` data in flash (docs/APP-HOST.md, "Images").
use std::env;

pub fn emit() {
    pocket_profile_build::emit(pocket_profile_build::Export::Required);
    println!("cargo:rerun-if-env-changed=POCKETJS_AOT_DIR");
    assert!(
        env::var_os("POCKETJS_AOT_DIR").is_some(),
        "POCKETJS_AOT_DIR is unset: build this crate through m5_pocket_app() (idf.py with CONFIG_M5_POCKET_APP_MODE_AOT)",
    );
}
