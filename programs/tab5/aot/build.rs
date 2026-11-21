//! Profile and fonts through m5_pocket_aot_build, then the factory pictures
//! at the raster density of the Tab5 host contract. MicroTS's generated
//! `load_images` embeds each image's 1x file as RGBA8888; this loader embeds
//! the `@2x` variant the JS build uses, in the pixel format `images.json`
//! names (PSP PSM 0 = RGB565, otherwise RGBA8888), under the same names.
use std::{env, fs, path::PathBuf};

const DENSITY: u32 = 2;
const PSM_5650: u32 = 0;
const PSM_8888: u32 = 3;

/// The names the generated `load_images` registers, in its order.
fn image_names(include: &str) -> Vec<String> {
    let mut names = Vec::new();
    let mut rest = include;
    while let Some(at) = rest.find("register_image(\"") {
        rest = &rest[at + "register_image(\"".len()..];
        let end = rest.find('"').expect("unterminated image name");
        names.push(rest[..end].to_string());
        rest = &rest[end..];
    }
    names
}

/// `images.json` maps a name to `{ "psm": N }`; only PSM 0 changes the
/// format here.
fn rgb565_names(json: &str) -> Vec<String> {
    let mut names = Vec::new();
    for entry in json.split('}') {
        if let (Some(start), Some(psm)) = (entry.find('"'), entry.find("\"psm\"")) {
            let name_end = entry[start + 1..].find('"').map(|e| start + 1 + e);
            let value = entry[psm + 5..].trim_start_matches([':', ' ', '\n']);
            if let Some(end) = name_end {
                if value.starts_with('0') {
                    names.push(entry[start + 1..end].to_string());
                }
            }
        }
    }
    names
}

fn density_variant(path: &PathBuf) -> PathBuf {
    let stem = path.file_stem().unwrap().to_string_lossy();
    path.with_file_name(format!("{stem}@{DENSITY}x.png"))
}

/// RGBA8 pixels of an 8-bit RGB or RGBA PNG.
fn decode(path: &PathBuf) -> (u32, u32, Vec<u8>) {
    let mut decoder = png::Decoder::new(fs::File::open(path).unwrap_or_else(|e| panic!("{path:?}: {e}")));
    decoder.set_transformations(png::Transformations::EXPAND);
    let mut reader = decoder.read_info().expect("PNG header");
    let mut buffer = vec![0; reader.output_buffer_size()];
    let info = reader.next_frame(&mut buffer).expect("PNG data");
    let pixels = (info.width * info.height) as usize;
    let rgba = match info.color_type {
        png::ColorType::Rgba => buffer[..pixels * 4].to_vec(),
        png::ColorType::Rgb => buffer[..pixels * 3]
            .chunks(3)
            .flat_map(|p| [p[0], p[1], p[2], 255])
            .collect(),
        other => panic!("{path:?}: unsupported PNG color type {other:?}"),
    };
    (info.width, info.height, rgba)
}

fn main() {
    m5_pocket_aot_build::emit();
    let out = PathBuf::from(env::var("OUT_DIR").unwrap());
    let aot = PathBuf::from(env::var("POCKETJS_AOT_DIR").expect("POCKETJS_AOT_DIR"));
    let app = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap()).join("../../../apps/tab5-factory");
    let include = aot.join("include.rs");
    let manifest = app.join("images.json");
    println!("cargo:rerun-if-changed={}", include.display());
    println!("cargo:rerun-if-changed={}", manifest.display());
    let names = image_names(&fs::read_to_string(&include).expect("include.rs"));
    let rgb565 = rgb565_names(&fs::read_to_string(&manifest).unwrap_or_default());
    let mut code = String::from(
        "/// The factory pictures at raster density 2 (build.rs).\nfn load_images_at_density(ui: &mut microts::Ui) {\n",
    );
    for (index, name) in names.iter().enumerate() {
        let base = app.join(name);
        let variant = density_variant(&base);
        let source = if variant.exists() { variant } else { base };
        println!("cargo:rerun-if-changed={}", source.display());
        let (w, h, rgba) = decode(&source);
        let (psm, bytes) = if rgb565.contains(name) {
            let packed: Vec<u8> = rgba
                .chunks(4)
                .flat_map(|p| {
                    let v = ((p[2] as u16 >> 3) << 11) | ((p[1] as u16 >> 2) << 5) | (p[0] as u16 >> 3);
                    v.to_le_bytes()
                })
                .collect();
            (PSM_5650, packed)
        } else {
            (PSM_8888, rgba)
        };
        let file = out.join(format!("tab5-image-{index}.raw"));
        fs::write(&file, &bytes).unwrap();
        code.push_str(&format!(
            "{{ let id = ui.core_mut().upload_texture(include_bytes!({:?}), {w}, {h}, {psm}); assert!(id >= 0); ui.register_image({:?}, id); }}\n",
            file.display().to_string(),
            name
        ));
    }
    code.push_str("}\n");
    fs::write(out.join("tab5_images.rs"), code).unwrap();
}
