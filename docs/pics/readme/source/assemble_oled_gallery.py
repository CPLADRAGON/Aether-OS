from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
FRAMES = ROOT / "generated"
OUTPUT = ROOT / "oled-ui-gallery.png"

ORDER = [
    ("menu", "MAIN MENU"),
    ("measure", "MEASURE"),
    ("time", "TIME"),
    ("time-detail", "TIME DETAIL"),
    ("weather", "WEATHER"),
    ("weather-detail", "WEATHER DETAIL"),
    ("room", "ROOM"),
    ("room-detail", "ROOM STATUS"),
    ("timer", "TIMER"),
    ("alert", "TIMER ALERT"),
]

SCALE = 4
PAD = 28
LABEL_H = 24
COLS = 5
ROWS = 2
CELL_W = 64 * SCALE
CELL_H = 48 * SCALE

canvas = Image.new(
    "RGB",
    (COLS * (CELL_W + PAD) + PAD, ROWS * (CELL_H + LABEL_H + PAD) + PAD),
    "#0d0d0f",
)
draw = ImageDraw.Draw(canvas)
font = ImageFont.truetype(r"C:\Windows\Fonts\arialbd.ttf", 11)

for index, (filename, label) in enumerate(ORDER):
    frame = Image.open(FRAMES / f"{filename}.pgm").convert("RGB")
    # No resampling: one source framebuffer pixel maps to a crisp 4×4 block.
    frame = frame.resize((CELL_W, CELL_H), Image.Resampling.NEAREST)
    row, col = divmod(index, COLS)
    x = PAD + col * (CELL_W + PAD)
    y = PAD + row * (CELL_H + LABEL_H + PAD)
    canvas.paste(frame, (x, y))
    draw.text((x, y + CELL_H + 5), label, fill="#a1a1aa", font=font)

canvas.save(OUTPUT)
