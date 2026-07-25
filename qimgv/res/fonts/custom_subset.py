from pathlib import Path
from fontTools import subset

# Get script's directory (res/fonts/)
SCRIPT_DIR = Path(__file__).resolve().parent

INPUT_FONT = SCRIPT_DIR / "FluentSystemIcons-Regular-Full.ttf"
OUTPUT_FONT = SCRIPT_DIR / "FluentSystemIcons-Custom.ttf"

UNICODES = (
    "U+E00B,U+F0799,U+E07C,U+E0C5,U+F8FA,U+F27C,U+E2FD,U+F290,U+E41F,U+F34C,"
    "U+F368,U+F369,U+F418,U+F462,U+F4A3,U+E422,U+F153,U+F660,U+F0A39,U+F6A9,"
    "U+F42E,U+F70A,U+E077,U+F15B,U+F185,U+F187,U+F2A3,U+F2B6,U+E42B,U+F02A1,"
    "U+E084,U+F582,U+F08F5,U+F62A,U+F66C,U+F359,U+F8C4,U+F8C6,U+E0C4,U+F1AB,"
    "U+F018D,U+F018E,U+F1E8,U+E305,U+F2B5,U+F488,U+E643,U+F41C,U+F480,U+F697,"
    "U+E8B0,U+F16A,U+F16C,U+F298,U+F3F1,U+F869,U+F2A1,U+F2AD,U+F2B3,U+F02F,"
    "U+F601,U+F493,U+F0B3A,U+F0BCB,U+E74A,U+EFC9,U+F0059,U+F339,U+EA94,U+F0461,"
    "U+F2DE,U+E373"
)

def build_subset() -> None:
    if not INPUT_FONT.exists():
        raise FileNotFoundError(f"Source font not found at {INPUT_FONT}")

    options = subset.Options()
    font = subset.load_font(str(INPUT_FONT), options)

    subsetter = subset.Subsetter(options=options)
    subsetter.populate(unicodes=subset.parse_unicodes(UNICODES))
    subsetter.subset(font)

    subset.save_font(font, str(OUTPUT_FONT), options)
    print(f"Generated subset: {OUTPUT_FONT}")

if __name__ == "__main__":
    build_subset()
