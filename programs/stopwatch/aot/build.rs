//! Build script of the StopWatch AOT crate.
//!
//! Besides the shared profile/font glue (`m5_pocket_aot_build::emit`) it
//! writes `OUT_DIR/stopwatch_images.rs` with `load_images_in_place`. The
//! generated `load_images` embeds every picture as RGBA8888 and copies it
//! into the heap at mount: about 10 MB of flash and 10 MB of heap for this
//! application (116 big-number tiles, 9 icons). This loader stores each
//! picture in the format `apps/stopwatch-factory/images.json` selects (as the
//! JS bundle does: RGB565 for opaque artwork, RGBA4444 for the big-number
//! tiles, RGBA8888 otherwise) and registers it with
//! `upload_texture_from`, which reads the flash bytes when the texture is
//! drawn and keeps a decoded copy only under the texture budget.

use std::{env, fs, path::PathBuf};

const PSM_5650: u32 = 0;
const PSM_4444: u32 = 2;
const PSM_8888: u32 = 3;

struct Entry {
    name: String,
    file: String,
    width: u32,
    height: u32,
}

/// Reads the `upload_texture(include_bytes!("<file>"), w, h, ..)` /
/// `register_image("<name>", id)` pairs of the generated include.rs.
fn entries(include: &str) -> Vec<Entry> {
    let mut out = Vec::new();
    for line in include.lines() {
        let Some(rest) = line.split("include_bytes!(\"").nth(1) else { continue };
        let Some((file, rest)) = rest.split_once("\")") else { continue };
        let numbers: Vec<u32> = rest
            .split(',')
            .skip(1)
            .take(2)
            .filter_map(|v| v.trim().parse().ok())
            .collect();
        let Some(name) = line
            .split("register_image(\"")
            .nth(1)
            .and_then(|r| r.split('"').next())
        else {
            continue;
        };
        if numbers.len() == 2 {
            out.push(Entry {
                name: name.to_string(),
                file: file.to_string(),
                width: numbers[0],
                height: numbers[1],
            });
        }
    }
    out
}

/// `"name": { "psm": n, "linear": true }` entries of images.json.
fn meta(json: &str, name: &str) -> (u32, bool) {
    let key = format!("\"{name}\"");
    let Some(start) = json.find(&key) else { return (PSM_8888, false) };
    let body = &json[start + key.len()..];
    let body = &body[..body.find('}').unwrap_or(body.len())];
    let psm = body
        .split("\"psm\":")
        .nth(1)
        .and_then(|v| v.trim().split(|c: char| !c.is_ascii_digit()).next())
        .and_then(|v| v.parse().ok())
        .unwrap_or(PSM_8888);
    (psm, body.contains("\"linear\": true"))
}

fn convert(rgba: &[u8], psm: u32) -> Vec<u8> {
    match psm {
        PSM_5650 => rgba
            .chunks_exact(4)
            .flat_map(|p| {
                let v = ((p[2] as u16 >> 3) << 11) | ((p[1] as u16 >> 2) << 5) | (p[0] as u16 >> 3);
                v.to_le_bytes()
            })
            .collect(),
        PSM_4444 => rgba
            .chunks_exact(4)
            .flat_map(|p| {
                let v = ((p[3] as u16 >> 4) << 12)
                    | ((p[2] as u16 >> 4) << 8)
                    | ((p[1] as u16 >> 4) << 4)
                    | (p[0] as u16 >> 4);
                v.to_le_bytes()
            })
            .collect(),
        _ => rgba.to_vec(),
    }
}

fn main() {
    m5_pocket_aot_build::emit();

    let aot = PathBuf::from(env::var("POCKETJS_AOT_DIR").expect("POCKETJS_AOT_DIR"));
    let out = PathBuf::from(env::var("OUT_DIR").expect("OUT_DIR"));
    let manifest = PathBuf::from(env::var("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR"));
    let json_path = manifest.join("../../../apps/stopwatch-factory/images.json");
    let include_path = aot.join("include.rs");
    println!("cargo:rerun-if-changed={}", include_path.display());
    println!("cargo:rerun-if-changed={}", json_path.display());
    let include = fs::read_to_string(&include_path).expect("include.rs");
    let json = fs::read_to_string(&json_path).unwrap_or_default();

    let mut code = String::from(
        "/// Registers every picture from flash in its images.json format (build.rs).\n\
         fn load_images_in_place(ui: &mut microts::Ui) {\n",
    );
    for (index, entry) in entries(&include).iter().enumerate() {
        let rgba = fs::read(aot.join(&entry.file)).expect("image stream");
        let (psm, linear) = meta(&json, &entry.name);
        let bytes = convert(&rgba, psm);
        let file = out.join(format!("image-{index}.bin"));
        fs::write(&file, &bytes).expect("write image");
        let flags = if linear { "microts::pocketjs_core::spec::img::FLAG_LINEAR" } else { "0" };
        code.push_str(&format!(
            "    {{ static DATA: &[u8] = include_bytes!({file:?}); \
             let storage: alloc::rc::Rc<dyn AsRef<[u8]>> = alloc::rc::Rc::new(DATA); \
             let id = ui.core_mut().upload_texture_from(storage, 0..DATA.len(), {w}, {h}, {psm}, {flags}); \
             if id >= 0 {{ ui.register_image({name:?}, id); }} }}\n",
            w = entry.width,
            h = entry.height,
            name = entry.name,
        ));
    }
    code.push_str("}\n");
    fs::write(out.join("stopwatch_images.rs"), code).expect("write loader");
}
