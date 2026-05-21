import os
import zipfile
import io
from PIL import Image

def convert_raw_to_ico():
    directory = r"c:\Users\hadouken\Desktop\filetypes"
    zip_path = os.path.join(directory, "raw.zip")
    ico_path = os.path.join(directory, "raw.ico")
    
    if not os.path.exists(zip_path):
        print(f"Error: {zip_path} does not exist.")
        return
        
    print(f"Processing {os.path.basename(zip_path)}...")
    try:
        with zipfile.ZipFile(zip_path, 'r') as z:
            images_dict = {}
            for name in z.namelist():
                if name.lower().endswith(('.png', '.jpg', '.jpeg', '.bmp')):
                    img_data = z.read(name)
                    img = Image.open(io.BytesIO(img_data))
                    
                    # Verify it's a square image of standard size
                    w, h = img.size
                    if w == h and w in {16, 32, 48, 64, 128, 256}:
                        # Convert to RGBA for standard compatibility and transparency
                        img_rgba = img.convert("RGBA")
                        images_dict[w] = img_rgba
                        print(f"  Loaded {name} as {w}x{h}")
            
            if not images_dict:
                print("  WARNING: No suitable square images found.")
                return
            
            # Sort descending (largest first)
            sorted_sizes = sorted(images_dict.keys(), reverse=True)
            sorted_images = [images_dict[size] for size in sorted_sizes]
            
            primary_image = sorted_images[0]
            other_images = sorted_images[1:]
            ico_sizes = [(size, size) for size in sorted_sizes]
            
            primary_image.save(
                ico_path,
                format='ICO',
                append_images=other_images,
                sizes=ico_sizes
            )
            print(f"\nSuccessfully created -> {os.path.basename(ico_path)} with sizes: {sorted_sizes}")
            
    except Exception as e:
        print(f"  ERROR: {e}")

if __name__ == "__main__":
    convert_raw_to_ico()
