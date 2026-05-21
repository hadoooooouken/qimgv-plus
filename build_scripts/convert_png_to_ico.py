import os
import glob
import zipfile
import io
from PIL import Image

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
                        if w == h and w in {16, 32, 48, 64, 128, 256}:
                            # Convert to RGBA for standard compatibility and transparency support
                            img_rgba = img.convert("RGBA")
                            images_dict[w] = img_rgba
                            print(f"  Loaded {name} as {w}x{h}")
                
                if not images_dict:
                    print(f"  WARNING: No suitable square PNGs found in {zip_path}. Skipping.")
                    continue
                
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
