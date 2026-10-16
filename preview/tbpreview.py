#!/usr/bin/env python3
"""TinyBuddy preview tool.

All pixels come from build/tbsim, which compiles the *same* C++ core the ESP32 runs.
This script only lays those exact frames out (contact sheets, OLED mock-ups, GIFs).

  python3 preview/tbpreview.py round 1          # render faces + expressions into previews/round_1/
  python3 preview/tbpreview.py compare A 1 2 3  # face A across rounds -> previews/compare_A.png
  python3 preview/tbpreview.py anim 80 F        # key poses, contact sheet and GIF of an animation
  python3 preview/tbpreview.py serve            # local web previewer on http://127.0.0.1:8765
"""
from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SIM = ROOT / "build" / "tbsim"
PREVIEWS = ROOT / "previews"

W, H = 128, 64
LIT = (226, 240, 255)        # cool white OLED pixel
LIT_DIM = (150, 190, 230)
GLASS = (6, 8, 12)


def need_pil():
    try:
        from PIL import Image  # noqa: F401
    except ImportError:
        sys.exit("Pillow is required for image export:  python3 -m pip install pillow")


def build_sim() -> None:
    r = subprocess.run(["make", "-s", "build/tbsim"], cwd=ROOT)
    if r.returncode != 0:
        sys.exit("tbsim build failed")


def sim(*args: str, capture: bool = False) -> str:
    r = subprocess.run([str(SIM), *args], cwd=ROOT, capture_output=capture, text=capture)
    if r.returncode != 0:
        sys.exit(f"tbsim {' '.join(args)} failed")
    return r.stdout if capture else ""


# ---------------------------------------------------------------------------
# Image helpers
# ---------------------------------------------------------------------------
def load_frame(path: Path):
    from PIL import Image
    im = Image.open(path).convert("L")
    assert im.size == (W, H), f"{path} is {im.size}, expected 128x64"
    return im


def nearest(im, scale: int, grid: bool = False):
    """Exact pixels, nearest-neighbour; optional faint grid for pixel counting."""
    from PIL import Image, ImageDraw
    big = im.resize((W * scale, H * scale), Image.NEAREST).convert("RGB")
    if grid and scale >= 4:
        d = ImageDraw.Draw(big)
        for x in range(0, W * scale, scale):
            d.line([(x, 0), (x, H * scale)], fill=(40, 40, 40) if x % (8 * scale) else (70, 70, 90))
        for y in range(0, H * scale, scale):
            d.line([(0, y), (W * scale, y)], fill=(40, 40, 40) if y % (8 * scale) else (70, 70, 90))
    return big


def oled(im, scale: int = 4, color=LIT, glow: bool = True):
    """How it looks on a real panel: dark glass, square pixels with gaps, soft bloom."""
    from PIL import Image, ImageDraw, ImageFilter
    gap = 1 if scale >= 3 else 0
    w, h = W * scale, H * scale
    lit = Image.new("RGB", (w, h), (0, 0, 0))
    d = ImageDraw.Draw(lit)
    px = im.load()
    for y in range(H):
        for x in range(W):
            if px[x, y] > 127:
                d.rectangle([x * scale, y * scale, x * scale + scale - 1 - gap, y * scale + scale - 1 - gap], fill=color)
    base = Image.new("RGB", (w, h), GLASS)
    if glow:
        bloom = lit.filter(ImageFilter.GaussianBlur(scale * 1.6))
        base = Image.blend(base, bloom, 0.0)
        from PIL import ImageChops
        base = ImageChops.add(base, bloom.point(lambda v: int(v * 0.55)))
    from PIL import ImageChops
    return ImageChops.lighter(base, lit)


def bezel(img, pad: int = 14, radius: int = 14):
    """Put a panel inside a dark module bezel."""
    from PIL import Image, ImageDraw
    w, h = img.size
    out = Image.new("RGB", (w + pad * 2, h + pad * 2), (24, 26, 32))
    d = ImageDraw.Draw(out)
    d.rounded_rectangle([0, 0, w + pad * 2 - 1, h + pad * 2 - 1], radius=radius, fill=(18, 20, 26), outline=(52, 56, 66), width=2)
    out.paste(img, (pad, pad))
    return out


def font(size: int):
    from PIL import ImageFont
    for p in ("/System/Library/Fonts/SFNSRounded.ttf", "/System/Library/Fonts/SFNS.ttf",
              "/System/Library/Fonts/Helvetica.ttc", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"):
        try:
            return ImageFont.truetype(p, size)
        except OSError:
            continue
    return ImageFont.load_default(size=size)


def faces_info() -> dict:
    return json.loads(sim("info", capture=True))


# ---------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------
def cmd_round(n: int, note: str | None) -> Path:
    need_pil()
    from PIL import Image, ImageDraw
    build_sim()
    out = PREVIEWS / f"round_{n}"
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    raw = out / "raw"
    raw.mkdir()
    sim("faces", str(raw))
    sim("expr", str(raw))
    info = faces_info()
    faces = info["faces"]
    exprs = info["expressions"]

    # individual faces: native PNG, exact 6x, OLED mock, raw bitmap + ascii
    for f in faces:
        fid = f["id"]
        im = load_frame(raw / f"face_{fid}.pgm")
        im.convert("1").save(out / f"face_{fid}_native.png")
        nearest(im, 6, grid=True).save(out / f"face_{fid}_x6_grid.png")
        bezel(oled(im, 5)).save(out / f"face_{fid}_oled.png")
        shutil.copy(raw / f"face_{fid}.bin", out / f"face_{fid}.bin")
        shutil.copy(raw / f"face_{fid}.txt", out / f"face_{fid}.txt")

    # contact sheet: 3 x 2 OLED panels, labels, then true-size (1x) and 2x thumbnails
    scale, cols = 4, 3
    panel_w, panel_h = W * scale + 28, H * scale + 28
    cell_w, cell_h = panel_w + 36, panel_h + 92 + H * 2 + 24
    sheet = Image.new("RGB", (cell_w * cols + 40, cell_h * 2 + 120), (12, 13, 17))
    d = ImageDraw.Draw(sheet)
    d.text((40, 30), f"TinyBuddy — face candidates · round {n}", font=font(34), fill=(235, 238, 245))
    if note:
        d.text((40, 76), note, font=font(18), fill=(150, 156, 170))
    for i, f in enumerate(faces):
        cx, cy = 40 + (i % cols) * cell_w, 120 + (i // cols) * cell_h
        im = load_frame(raw / f"face_{f['id']}.pgm")
        sheet.paste(bezel(oled(im, scale)), (cx, cy))
        ty = cy + panel_h + 10
        d.text((cx + 2, ty), f"{f['id']} · {f['name']}", font=font(26), fill=(235, 238, 245))
        d.text((cx + 2, ty + 34), f["motto"], font=font(17), fill=(140, 146, 160))
        thumbs_y = ty + 66
        sheet.paste(nearest(im, 2), (cx + 2, thumbs_y))
        sheet.paste(nearest(im, 1), (cx + 2 + W * 2 + 16, thumbs_y + H))
        d.text((cx + W * 2 + 18 + W + 12, thumbs_y + H + 20), "2x · 1x (true size)", font=font(14), fill=(110, 116, 128))
    sheet.save(out / "contact_sheet.png")

    # native-size check strip: all six at 1x and 2x, nothing else
    strip = Image.new("RGB", (W * 3 * 6 + 70, H * 2 + 20 + H + 20), (0, 0, 0))
    for i, f in enumerate(faces):
        im = load_frame(raw / f"face_{f['id']}.pgm")
        strip.paste(nearest(im, 2), (10 + i * (W * 3 + 10), 10))
        strip.paste(nearest(im, 1), (10 + i * (W * 3 + 10), H * 2 + 30))
    strip.save(out / "native_check.png")

    # expression sheets (one per face) — the face's emotional range
    escale, ecols = 3, 5
    ew, eh = W * escale + 16, H * escale + 44
    for f in faces:
        rows = (len(exprs) + ecols - 1) // ecols
        es = Image.new("RGB", (ew * ecols + 24, eh * rows + 70), (12, 13, 17))
        ed = ImageDraw.Draw(es)
        ed.text((16, 18), f"{f['id']} · {f['name']} — expressions (round {n})", font=font(24), fill=(235, 238, 245))
        for j, e in enumerate(exprs):
            x, y = 12 + (j % ecols) * ew, 64 + (j // ecols) * eh
            im = load_frame(raw / f"expr_{f['id']}_{e}.pgm")
            es.paste(oled(im, escale, glow=False), (x, y))
            ed.text((x + 2, y + H * escale + 6), e.replace("_", " "), font=font(17), fill=(160, 166, 180))
        es.save(out / f"expressions_{f['id']}.png")

    # snapshot the design source so every round can be compared or restored
    shutil.copy(ROOT / "core" / "src" / "tb_style.cpp", out / "tb_style.snapshot.cpp")
    shutil.copy(ROOT / "core" / "src" / "tb_render.cpp", out / "tb_render.snapshot.cpp")
    shutil.rmtree(raw)
    print(f"round {n} -> {out.relative_to(ROOT)}")
    return out


def cmd_compare(face: str, rounds: list[int]) -> None:
    need_pil()
    from PIL import Image, ImageDraw
    panels = []
    for r in rounds:
        p = PREVIEWS / f"round_{r}" / f"face_{face}_native.png"
        if not p.exists():
            sys.exit(f"missing {p}")
        panels.append((r, Image.open(p).convert("L")))
    scale = 4
    pw = W * scale + 28
    out = Image.new("RGB", (pw * len(panels) + 40, H * scale + 28 + 90), (12, 13, 17))
    d = ImageDraw.Draw(out)
    for i, (r, im) in enumerate(panels):
        out.paste(bezel(oled(im, scale)), (20 + i * pw, 20))
        d.text((24 + i * pw, H * scale + 28 + 34), f"face {face} · round {r}", font=font(22), fill=(220, 224, 232))
    dest = PREVIEWS / f"compare_{face}.png"
    out.save(dest)
    print(dest.relative_to(ROOT))


def cmd_lab(name: str, face: str, exprs: list[str], variants: list[str], scale: int = 3) -> Path:
    """Design lab: rows = parameter variants, columns = expressions. Variant syntax:
    'label|key=val key=val'  (keys are FaceStyle params or rest.<channel>)."""
    need_pil()
    from PIL import Image, ImageDraw
    build_sim()
    rows = []
    for v in variants:
        label, _, kvs = v.partition("|")
        rows.append((label.strip(), kvs.split()))
    lab_dir = PREVIEWS / "lab"
    lab_dir.mkdir(parents=True, exist_ok=True)
    cw, ch = W * scale + 12, H * scale + 12
    label_w = 230
    sheet = Image.new("RGB", (label_w + cw * len(exprs) + 12, 56 + ch * len(rows) + 10), (12, 13, 17))
    d = ImageDraw.Draw(sheet)
    for j, e in enumerate(exprs):
        d.text((label_w + j * cw + 4, 18), e.replace("_", " "), font=font(18), fill=(170, 176, 190))
    with tempfile.TemporaryDirectory() as tmp:
        for i, (label, kvs) in enumerate(rows):
            d.text((12, 56 + i * ch + ch // 2 - 12), label, font=font(18), fill=(230, 233, 240))
            for j, e in enumerate(exprs):
                out = Path(tmp) / f"{i}_{j}.pgm"
                args = ["lab", str(out), face, e]
                for kv in kvs:
                    args += ["--set", kv]
                sim(*args)
                sheet.paste(oled(load_frame(out), scale, glow=False), (label_w + j * cw, 56 + i * ch))
    dest = lab_dir / f"{name}.png"
    sheet.save(dest)
    print(dest.relative_to(ROOT))
    return dest


def read_seq(d: Path) -> tuple[dict, list]:
    """frames.bin (page layout, exactly what the OLED receives) -> list of 128x64 'L' images."""
    from PIL import Image
    meta = json.loads((d / "frames.json").read_text())
    raw = (d / "frames.bin").read_bytes()
    frames = []
    for i in range(meta["count"]):
        page = raw[i * 1024:(i + 1) * 1024]
        px = bytearray(W * H)
        for p in range(8):
            for x in range(W):
                b = page[p * W + x]
                for bit in range(8):
                    if b >> bit & 1:
                        px[(p * 8 + bit) * W + x] = 255
        frames.append(Image.frombytes("L", (W, H), bytes(px)))
    return meta, frames


def save_gif(frames, fps: int, dest: Path, scale: int, style: str = "oled") -> None:
    from PIL import Image
    if style == "oled":
        imgs = [oled(f, scale, glow=False) for f in frames]
    else:
        imgs = [nearest(f, scale) for f in frames]
    pal = [im.convert("P", palette=Image.ADAPTIVE, colors=4) for im in imgs]
    pal[0].save(dest, save_all=True, append_images=pal[1:], duration=round(1000 / fps), loop=0, optimize=True,
                disposal=1)


def cmd_anim(anim: str, faces: list[str], fps: int, mirror: int, outdir: Path | None = None) -> Path:
    need_pil()
    from PIL import Image, ImageDraw
    build_sim()
    out = outdir or (PREVIEWS / "animations" / f"anim_{anim}")
    out.mkdir(parents=True, exist_ok=True)
    seqs = {}
    with tempfile.TemporaryDirectory() as tmp:
        for f in faces:
            d = Path(tmp) / f
            d.mkdir()
            sim("anim", anim, f, str(d), "--fps", str(fps), "--mirror", str(mirror))
            seqs[f] = read_seq(d)
    meta0 = seqs[faces[0]][0]
    name = meta0["name"]
    for f, (meta, frames) in seqs.items():
        save_gif(frames, fps, out / f"anim_{anim}_{f}.gif", 4, "oled")
        # key poses: the frame where each phase reaches its peak (35% into the phase)
        marks = meta["markers"]
        picks = []
        for i, m in enumerate(marks):
            t_end = marks[i + 1]["t"] if i + 1 < len(marks) else meta["duration"]
            t = m["t"] + 0.35 * (t_end - m["t"])
            idx = min(range(len(meta["t"])), key=lambda k: abs(meta["t"][k] - t))
            picks.append((m["label"], meta["t"][idx], frames[idx]))
        sc, cols = 3, 5
        cw, ch = W * sc + 14, H * sc + 44
        rows = (len(picks) + cols - 1) // cols
        sheet = Image.new("RGB", (cw * cols + 20, ch * rows + 70), (12, 13, 17))
        d = ImageDraw.Draw(sheet)
        d.text((14, 18), f"#{anim} {name} — key poses, face {f}", font=font(24), fill=(235, 238, 245))
        for j, (label, t, im) in enumerate(picks):
            x, y = 10 + (j % cols) * cw, 62 + (j // cols) * ch
            sheet.paste(oled(im, sc, glow=False), (x, y))
            d.text((x + 2, y + H * sc + 6), f"{t:>5} ms · {label}", font=font(16), fill=(170, 176, 190))
        sheet.save(out / f"keyposes_{f}.png")
        # filmstrip: one frame every 100 ms, native 2x, for spotting pops and artefacts
        step = max(1, round(fps / 10))
        strip = frames[::step]
        cols2 = 10
        rows2 = (len(strip) + cols2 - 1) // cols2
        fs = Image.new("RGB", ((W * 2 + 6) * cols2 + 6, (H * 2 + 22) * rows2 + 6), (12, 13, 17))
        dd = ImageDraw.Draw(fs)
        for j, im in enumerate(strip):
            x, y = 6 + (j % cols2) * (W * 2 + 6), 6 + (j // cols2) * (H * 2 + 22)
            fs.paste(nearest(im, 2), (x, y))
            dd.text((x, y + H * 2 + 2), f"{meta['t'][j * step]} ms", font=font(13), fill=(120, 126, 140))
        fs.save(out / f"filmstrip_{f}.png")
    if len(faces) > 1:
        # all candidates performing in sync: the best way to choose a face is to watch it act
        n = len(seqs[faces[0]][1])
        cols3 = 3
        rows3 = (len(faces) + cols3 - 1) // cols3
        sc = 3
        pw, ph = W * sc + 16, H * sc + 34
        grid = []
        for i in range(n):
            g = Image.new("RGB", (pw * cols3 + 8, ph * rows3 + 8), (12, 13, 17))
            gd = ImageDraw.Draw(g)
            for j, f in enumerate(faces):
                fr = seqs[f][1][min(i, len(seqs[f][1]) - 1)]
                x, y = 8 + (j % cols3) * pw, 8 + (j // cols3) * ph
                g.paste(oled(fr, sc, glow=False), (x, y))
                gd.text((x + 2, y + H * sc + 4), f"{f}", font=font(18), fill=(200, 204, 214))
            grid.append(g)
        pal = [im.convert("P", palette=Image.ADAPTIVE, colors=8) for im in grid]
        pal[0].save(out / f"anim_{anim}_all_faces.gif", save_all=True, append_images=pal[1:],
                    duration=round(1000 / fps), loop=0, optimize=True)
    print(f"animation {anim} -> {out.relative_to(ROOT)}")
    return out


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("round", help="render a design round")
    r.add_argument("n", type=int)
    r.add_argument("--note", default=None)
    c = sub.add_parser("compare", help="one face across rounds")
    c.add_argument("face")
    c.add_argument("rounds", type=int, nargs="+")
    lb = sub.add_parser("lab", help="compare parameter variants")
    lb.add_argument("name")
    lb.add_argument("--face", default="F")
    lb.add_argument("--exprs", default="rest,blink,happy,sleepy,look_left,angry")
    lb.add_argument("--var", action="append", default=[], help="'label|key=val key=val'")
    lb.add_argument("--scale", type=int, default=3)
    an = sub.add_parser("anim", help="key poses, filmstrip and GIFs of an animation")
    an.add_argument("anim", help="animation id or name")
    an.add_argument("faces", nargs="?", default="ABCDEF", help="faces to render, e.g. F or ABCDEF")
    an.add_argument("--fps", type=int, default=30)
    an.add_argument("--mirror", type=int, default=0)
    a = ap.parse_args()
    if a.cmd == "anim":
        cmd_anim(a.anim, list(a.faces.upper()), a.fps, a.mirror)
    elif a.cmd == "lab":
        cmd_lab(a.name, a.face.upper(), a.exprs.split(","), a.var or ["base|"], a.scale)
    elif a.cmd == "round":
        cmd_round(a.n, a.note)
    elif a.cmd == "compare":
        cmd_compare(a.face.upper(), a.rounds)


if __name__ == "__main__":
    main()
