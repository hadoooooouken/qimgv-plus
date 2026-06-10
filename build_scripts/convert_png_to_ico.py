import os
import glob
import zipfile
import io
from PIL import Image, IcoImagePlugin, BmpImagePlugin, ImageFile
from PIL._binary import o8, o16le as o16, o32le as o32

# Monkeypatch Pillow's ICO writer to save frames < 256x256 as BMP (DIB) and 256x256 as PNG.
# This ensures maximum compatibility with Windows Explorer's legacy scaling logic for small icons.
def custom_ico_save(im, fp, filename):
    fp.write(IcoImagePlugin._MAGIC)
    bmp = im.encoderinfo.get("bitmap_format") == "bmp"
    sizes = im.encoderinfo.get(
        "sizes",
        [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)],
    )
    frames = []
    provided_ims = [im] + im.encoderinfo.get("append_images", [])
    width, height = im.size
    for size in sorted(set(sizes)):
        if size[0] > width or size[1] > height or size[0] > 256 or size[1] > 256:
            continue

        for provided_im in provided_ims:
            if provided_im.size != size:
                continue
            frames.append(provided_im)
            break
        else:
            frame = provided_im.copy()
            frame.thumbnail(size, Image.Resampling.LANCZOS, reducing_gap=None)
            frames.append(frame)
            
    fp.write(o16(len(frames)))  # idCount(2)
    offset = fp.tell() + len(frames) * 16
    for frame in frames:
        w, h = frame.size
        # 0 means 256
        fp.write(o8(w if w < 256 else 0))  # bWidth(1)
        fp.write(o8(h if h < 256 else 0))  # bHeight(1)

        # Force BMP (DIB) for frames < 256x256, and PNG for 256x256
        frame_bmp = bmp or (w < 256)

        bits, colors = BmpImagePlugin.SAVE[frame.mode][1:] if frame_bmp else (32, 0)
        fp.write(o8(colors))  # bColorCount(1)
        fp.write(b"\0")  # bReserved(1)
        fp.write(b"\0\0")  # wPlanes(2)
        fp.write(o16(bits))  # wBitCount(2)

        image_io = io.BytesIO()
        if frame_bmp:
            frame.save(image_io, "dib")

            # Extract AND mask from alpha channel to ensure proper transparency fallback
            # and prevent truncated DIB data bugs in Windows decoders.
            if frame.mode == "RGBA":
                alpha = frame.getchannel("A")
                and_mask = alpha.point(lambda a: 255 if a < 128 else 0).convert("1")
            else:
                and_mask = Image.new("1", (w, h))

            ImageFile._save(
                and_mask,
                image_io,
                [ImageFile._Tile("raw", (0, 0) + (w, h), 0, ("1", 0, -1))],
            )
        else:
            frame.save(image_io, "png")
            
        image_io.seek(0)
        image_bytes = image_io.read()
        if frame_bmp:
            image_bytes = image_bytes[:8] + o32(h * 2) + image_bytes[12:]
        bytes_len = len(image_bytes)
        fp.write(o32(bytes_len))  # dwBytesInRes(4)
        fp.write(o32(offset))  # dwImageOffset(4)
        
        current = fp.tell()
        fp.seek(offset)
        fp.write(image_bytes)
        offset = offset + bytes_len
        fp.seek(current)

# Apply the save patch to Pillow
Image.register_save("ICO", custom_ico_save)


def convert_zips_to_icos(directory):
    zip_files = glob.glob(os.path.join(directory, "*.zip"))
    if not zip_files:
        print("No .zip files found in directory:", directory)
        return

    print(f"Found {len(zip_files)} zip files to process.\n")

    for zip_path in zip_files:
        base_name = os.path.splitext(os.path.basename(zip_path))[0]
        ico_path = os.path.join(directory, f"{base_name}.ico")
        
        print(f"Processing {os.path.basename(zip_path)}...")
        
        try:
            with zipfile.ZipFile(zip_path, 'r') as z:
                # Load all image files from the zip
                images_dict = {}
                for name in z.namelist():
                    if name.lower().endswith(('.png', '.jpg', '.jpeg', '.bmp')):
                        img_data = z.read(name)
                        img = Image.open(io.BytesIO(img_data))
                        
                        # Verify it's a square image of a standard size
                        w, h = img.size
                        if w == h and w in {16, 24, 32, 48, 64, 96, 128, 256}:
                            # Convert to RGBA for standard compatibility and transparency support
                            img_rgba = img.convert("RGBA")
                            images_dict[w] = img_rgba
                            print(f"  Loaded {name} as {w}x{h}")
                
                if not images_dict:
                    print(f"  WARNING: No suitable square PNGs found in {zip_path}. Skipping.")
                    continue
                
                # Automatically generate standard sizes if they are missing
                standard_sizes = {16, 24, 32, 48, 64, 96, 128, 256}
                largest_size = max(images_dict.keys())
                loaded_sizes = list(images_dict.keys())
                
                for size in sorted(standard_sizes, reverse=True):
                    if size not in images_dict and size < largest_size:
                        # Find the smallest originally loaded image that is larger than the target size
                        possible_sources = [s for s in loaded_sizes if s >= size]
                        if possible_sources:
                            source_size = min(possible_sources)
                            source_image = images_dict[source_size]
                            # Resize using high-quality Lanczos filter
                            resized_img = source_image.resize((size, size), Image.Resampling.LANCZOS)
                            images_dict[size] = resized_img
                            print(f"  Auto-generated {size}x{size} from {source_size}x{source_size}")
                
                # Sort images by size descending so the largest (e.g. 256x256) is the primary image,
                # which is a standard convention.
                sorted_sizes = sorted(images_dict.keys(), reverse=True)
                sorted_images = [images_dict[size] for size in sorted_sizes]
                
                primary_image = sorted_images[0]
                other_images = sorted_images[1:]
                ico_sizes = [(size, size) for size in sorted_sizes]
                
                # Save as ICO
                primary_image.save(
                    ico_path,
                    format='ICO',
                    append_images=other_images,
                    sizes=ico_sizes
                )
                print(f"  Successfully created -> {os.path.basename(ico_path)} with sizes: {sorted_sizes}\n")
                
        except Exception as e:
            print(f"  ERROR processing {zip_path}: {e}\n")

if __name__ == "__main__":
    target_dir = r"c:\Users\hadouken\Desktop\filetypes"
    convert_zips_to_icos(target_dir)
