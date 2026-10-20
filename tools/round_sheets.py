#!/usr/bin/env python3
"""Compose README screenshots of the round build from build/shots_round
(run `make -f host.mk sim_round` first). Writes screenshots/stopwatch_*.png."""
import os
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "build", "shots_round")
OUT = os.path.join(ROOT, "screenshots")
S = 320
SHEETS = {
    "stopwatch_den": ["den_whisper_night", "den_day_asleep", "den_nessie_loch", "den_legend"],
    "stopwatch_setup": ["boot_done", "tray_holding", "name", "hatch_reveal"],
    "stopwatch_tabs": ["care_tab", "file_scrolled", "more_tab", "legend_reveal_card"],
    "stopwatch_games": ["game_waltz_mid", "game_forage", "game_hop", "walk_real_steps"],
    "stopwatch_moments": ["hiker_alert", "pants_overlay", "burrow_holding", "power_hold"],
}


def disc(path):
    im = Image.open(path).convert("RGB").resize((S, S), Image.LANCZOS)
    mask = Image.new("L", (S * 4, S * 4), 0)
    ImageDraw.Draw(mask).ellipse((0, 0, S * 4 - 1, S * 4 - 1), fill=255)
    mask = mask.resize((S, S), Image.LANCZOS)
    out = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    out.paste(im, (0, 0), mask)
    return out


def main():
    files = {f[3:-4]: os.path.join(SRC, f) for f in os.listdir(SRC) if f.endswith(".ppm")}
    os.makedirs(OUT, exist_ok=True)
    for name, shots in SHEETS.items():
        sheet = Image.new("RGBA", (len(shots) * (S + 24) + 24, S + 48), (24, 26, 32, 255))
        for i, s in enumerate(shots):
            sheet.alpha_composite(disc(files[s]), (24 + i * (S + 24), 24))
        sheet.save(os.path.join(OUT, name + ".png"))
        print("wrote", name)


if __name__ == "__main__":
    main()
