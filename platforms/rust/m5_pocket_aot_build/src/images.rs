//! [`emit_images`]: the application's pictures at the raster density, in the
//! pixel formats `images.json` declares.
//!
//! MicroTS's generated `load_images` embeds each image's 1x file as
//! RGBA8888: it ignores the `@<density>x` variants and `images.json`, so on a
//! density-2 host the AOT artwork is half resolution, and every picture takes
//! four bytes per pixel. The JS build packs the density variant in the format
//! `images.json` names.
use std::{
    env, fs,
    path::{Path, PathBuf},
};

/// PSP pixel storage modes of the DrawList texture contract.
const PSM_5650: u32 = 0;
const PSM_4444: u32 = 2;
const PSM_8888: u32 = 3;
/// `spec::img::FLAG_LINEAR`: bilinear sampling.
const FLAG_LINEAR: u8 = 2;

/// One image the generated `load_images` registers: its name, its RGBA8888
/// stream in the AOT directory and that stream's (power-of-two) size.
struct Generated {
    name: String,
    stream: String,
    width: u32,
    height: u32,
}

/// The `upload_texture(include_bytes!("<stream>"), w, h, ..)` and
/// `register_image("<name>", id)` pairs of the generated `include.rs`.
fn generated_images(include: &str) -> Vec<Generated> {
    let mut images = Vec::new();
    for line in include.lines() {
        let Some((_, rest)) = line.split_once("include_bytes!(\"") else {
            continue;
        };
        let Some((stream, rest)) = rest.split_once("\")") else {
            continue;
        };
        let numbers: Vec<u32> = rest
            .split(',')
            .skip(1)
            .take(2)
            .filter_map(|value| value.trim().parse().ok())
            .collect();
        let name = line
            .split_once("register_image(\"")
            .and_then(|(_, rest)| rest.split_once('"'))
            .map(|(name, _)| name);
        if let (Some(name), &[width, height]) = (name, numbers.as_slice()) {
            images.push(Generated {
                name: name.to_string(),
                stream: stream.to_string(),
                width,
                height,
            });
        }
    }
    images
}

/// The `"psm"` and `"linear"` of `name` in `images.json`
/// (`{ "<name>": { "psm": N, "linear": true }, ... }`); RGBA8888 and point
/// sampling when the name is absent.
fn declared(manifest: &str, name: &str) -> (u32, bool) {
    let key = format!("\"{name}\"");
    let Some(at) = manifest.find(&key) else {
        return (PSM_8888, false);
    };
    let entry = &manifest[at + key.len()..];
    let entry = &entry[..entry.find('}').unwrap_or(entry.len())];
    let psm = entry
        .find("\"psm\"")
        .map(|at| &entry[at + 5..])
        .map(|value| {
            value
                .trim_start_matches(|c: char| c == ':' || c.is_whitespace())
                .chars()
                .take_while(char::is_ascii_digit)
                .collect::<String>()
        })
        .and_then(|digits| digits.parse().ok())
        .unwrap_or(PSM_8888);
    let linear = entry
        .find("\"linear\"")
        .map(|at| entry[at + 8..].trim_start_matches(|c: char| c == ':' || c.is_whitespace()))
        .is_some_and(|value| value.starts_with("true"));
    (psm, linear)
}

/// `<stem>@<density>x.png` beside `path`.
fn density_variant(path: &Path, density: u32) -> PathBuf {
    let stem = path.file_stem().unwrap_or_default().to_string_lossy();
    path.with_file_name(format!("{stem}@{density}x.png"))
}

/// RGBA8 pixels of an 8-bit RGB or RGBA PNG.
fn decode(path: &Path) -> (u32, u32, Vec<u8>) {
    let file = fs::File::open(path).unwrap_or_else(|error| panic!("{}: {error}", path.display()));
    let mut decoder = png::Decoder::new(file);
    decoder.set_transformations(png::Transformations::EXPAND);
    let mut reader = decoder
        .read_info()
        .unwrap_or_else(|error| panic!("{}: {error}", path.display()));
    let mut buffer = vec![0; reader.output_buffer_size()];
    let info = reader
        .next_frame(&mut buffer)
        .unwrap_or_else(|error| panic!("{}: {error}", path.display()));
    let pixels = (info.width * info.height) as usize;
    let rgba = match info.color_type {
        png::ColorType::Rgba => buffer[..pixels * 4].to_vec(),
        png::ColorType::Rgb => buffer[..pixels * 3]
            .chunks(3)
            .flat_map(|p| [p[0], p[1], p[2], 255])
            .collect(),
        other => panic!("{}: unsupported PNG color type {other:?}", path.display()),
    };
    (info.width, info.height, rgba)
}

/// RGBA8 pixels in `psm`, as the JS build's pak encoder stores them:
/// PSM_5650 with red in the low five bits, PSM_4444 with red in the low
/// nibble, both little endian; RGBA8888 unchanged.
fn convert(rgba: Vec<u8>, psm: u32) -> Vec<u8> {
    match psm {
        PSM_5650 => rgba
            .chunks(4)
            .flat_map(|p| {
                let value =
                    ((p[2] as u16 >> 3) << 11) | ((p[1] as u16 >> 2) << 5) | (p[0] as u16 >> 3);
                value.to_le_bytes()
            })
            .collect(),
        PSM_4444 => rgba
            .chunks(4)
            .flat_map(|p| {
                let value = ((p[3] as u16 >> 4) << 12)
                    | ((p[2] as u16 >> 4) << 8)
                    | ((p[1] as u16 >> 4) << 4)
                    | (p[0] as u16 >> 4);
                value.to_le_bytes()
            })
            .collect(),
        _ => rgba,
    }
}

/// Write `OUT_DIR/m5_images.rs` with
///
/// - `fn load_images_at_density(ui: &mut microts::Ui, storage: m5_pocket_aot::ImageStorage)`,
///   which registers every image the generated `include.rs` registers, under
///   the same names;
/// - `const M5_IMAGE_BYTES: usize`, the pixel bytes of all of them.
///
/// Each picture's pixels come from `<stem>@<density>x.png` beside the image
/// in `app_dir` when `density` is above 1 and that file exists, otherwise
/// from the RGBA8888 stream MicroTS generated (the 1x file, already on a
/// power-of-two canvas). They are stored in the format `images.json` in
/// `app_dir` declares: `"psm"` 0 RGB565, 2 RGBA4444, 3 or absent RGBA8888;
/// `"linear": true` selects bilinear sampling. The pixels are written to
/// `OUT_DIR` and embedded with `include_bytes!`, so they cost their size in
/// flash. A density variant must have power-of-two sides, as textures
/// require; another size fails the build. Call after [`crate::emit`].
pub fn emit_images(app_dir: impl AsRef<Path>, density: u32) {
    let app_dir = app_dir.as_ref();
    assert!(density >= 1, "raster density must be at least 1");
    let out = PathBuf::from(env::var_os("OUT_DIR").expect("OUT_DIR"));
    let aot = PathBuf::from(env::var_os("POCKETJS_AOT_DIR").expect("POCKETJS_AOT_DIR"));
    let include = aot.join("include.rs");
    let manifest_path = app_dir.join("images.json");
    println!("cargo:rerun-if-changed={}", include.display());
    println!("cargo:rerun-if-changed={}", manifest_path.display());
    let images = generated_images(
        &fs::read_to_string(&include)
            .unwrap_or_else(|error| panic!("{}: {error}", include.display())),
    );
    let manifest = fs::read_to_string(&manifest_path).unwrap_or_default();
    let mut total = 0usize;
    let mut code = format!(
        "// Generated by m5_pocket_aot_build::emit_images; do not edit.\n\
         /// The application's pictures at raster density {density}.\n\
         #[allow(dead_code)]\n\
         fn load_images_at_density(ui: &mut microts::Ui, storage: m5_pocket_aot::ImageStorage) {{\n"
    );
    for (index, image) in images.iter().enumerate() {
        let variant = density_variant(&app_dir.join(&image.name), density);
        let (width, height, rgba) = if density > 1 && variant.exists() {
            println!("cargo:rerun-if-changed={}", variant.display());
            let (width, height, rgba) = decode(&variant);
            assert!(
                width.is_power_of_two() && height.is_power_of_two(),
                "{}: {width}x{height}; textures need power-of-two sides",
                variant.display()
            );
            (width, height, rgba)
        } else {
            let stream = aot.join(&image.stream);
            let rgba =
                fs::read(&stream).unwrap_or_else(|error| panic!("{}: {error}", stream.display()));
            assert_eq!(
                rgba.len(),
                (image.width * image.height * 4) as usize,
                "{}",
                stream.display()
            );
            (image.width, image.height, rgba)
        };
        let (psm, linear) = declared(&manifest, &image.name);
        let psm = match psm {
            PSM_5650 | PSM_4444 | PSM_8888 => psm,
            other => {
                println!(
                    "cargo:warning={}: images.json psm {other} is stored as RGBA8888",
                    image.name
                );
                PSM_8888
            }
        };
        let bytes = convert(rgba, psm);
        total += bytes.len();
        let file = out.join(format!("m5-image-{index}.raw"));
        fs::write(&file, &bytes).expect("write image pixels");
        let flags = if linear { FLAG_LINEAR } else { 0 };
        code.push_str(&format!(
            "    m5_pocket_aot::upload_image(ui, {:?}, include_bytes!({:?}), {width}, {height}, {psm}, {flags}, storage);\n",
            image.name,
            file.display().to_string()
        ));
    }
    code.push_str("}\n");
    code.push_str(&format!(
        "/// Pixel bytes of the pictures load_images_at_density registers.\n\
         #[allow(dead_code)]\n\
         const M5_IMAGE_BYTES: usize = {total};\n"
    ));
    fs::write(out.join("m5_images.rs"), code).expect("write m5_images.rs");
}
