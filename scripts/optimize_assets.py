from pathlib import Path
from PIL import Image

ASSETS = Path("assets")
TARGETS = [
    "01-dashboard-overview.webp",
    "02-mobile-responsive-view.webp",
    "03-wifi-scanner.webp",
    "04-client-manager.webp",
    "05-speed-data-limits.webp",
    "06-access-control.webp",
    "07-wifi-failover.webp",
    "08-esp32u-hardware.webp",
    "rangelink32-hero.webp",
]

MAX_DIMENSION = 1920
QUALITY = 82

for name in TARGETS:
    path = ASSETS / name
    if not path.exists():
        continue

    original_size = path.stat().st_size
    temp = path.with_suffix(".optimized.webp")

    with Image.open(path) as image:
        image.load()

        width, height = image.size
        largest = max(width, height)

        if largest > MAX_DIMENSION:
            scale = MAX_DIMENSION / largest
            image = image.resize(
                (
                    max(1, round(width * scale)),
                    max(1, round(height * scale)),
                ),
                Image.Resampling.LANCZOS,
            )

        if image.mode not in ("RGB", "RGBA"):
            image = image.convert("RGBA" if "A" in image.getbands() else "RGB")

        image.save(
            temp,
            "WEBP",
            quality=QUALITY,
            method=6,
        )

    optimized_size = temp.stat().st_size

    if optimized_size < original_size:
        temp.replace(path)
        print(
            f"{name}: {original_size:,} -> "
            f"{optimized_size:,} bytes"
        )
    else:
        temp.unlink()
        print(f"{name}: kept original ({original_size:,} bytes)")
