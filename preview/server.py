#!/usr/bin/env python3
"""TinyBuddy local previewer — stdlib only, binds to 127.0.0.1.

Every frame shown in the browser is rendered by build/tbsim, i.e. the same C++ core
the ESP32 firmware runs. The page only decodes the 1024-byte page-layout buffers.

    python3 preview/server.py            # then open http://127.0.0.1:8765
"""
from __future__ import annotations

import json
import re
import shutil
import subprocess
import sys
import time
from http import HTTPStatus
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

ROOT = Path(__file__).resolve().parent.parent
SIM = ROOT / "build" / "tbsim"
WEB = ROOT / "preview" / "web"
PREVIEWS = ROOT / "previews"
REFERENCE = ROOT / "reference"
REVIEWS = PREVIEWS / "reviews.json"
PORT = 8765

SET_RE = re.compile(r"^([A-F]:)?[A-Za-z][A-Za-z0-9.]*=[-A-Za-z0-9.]+$")
FACE_RE = re.compile(r"^[A-Fa-f]$")
WORD_RE = re.compile(r"^[A-Za-z0-9_ ]{1,40}$")


def build() -> None:
    r = subprocess.run(["make", "-s", "build/tbsim"], cwd=ROOT)
    if r.returncode != 0:
        sys.exit("could not build build/tbsim (is a C++ compiler installed?)")


def tbsim(*args: str, sets: list[str] | None = None) -> bytes:
    cmd = [str(SIM)]
    for s in sets or []:
        cmd += ["--set", s]
    cmd += list(args)
    r = subprocess.run(cmd, cwd=ROOT, capture_output=True, timeout=60)
    if r.returncode != 0:
        raise RuntimeError(r.stderr.decode(errors="replace") or f"tbsim failed: {args}")
    return r.stdout


def num(q: dict, key: str, default: float, lo: float, hi: float) -> str:
    try:
        v = float(q.get(key, [default])[0])
    except ValueError:
        v = default
    v = max(lo, min(hi, v))
    return str(int(v)) if float(v).is_integer() else str(v)


def load_reviews() -> dict:
    if REVIEWS.exists():
        return json.loads(REVIEWS.read_text())
    return {"current": {}, "history": []}


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *a, **kw):
        super().__init__(*a, directory=str(ROOT), **kw)

    def log_message(self, fmt, *args):  # quieter console
        if "/api/" in (args[0] if args else ""):
            return
        super().log_message(fmt, *args)

    # ---- helpers ----
    def send_json(self, obj, status=HTTPStatus.OK):
        body = obj if isinstance(obj, bytes) else json.dumps(obj).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def fail(self, msg, status=HTTPStatus.BAD_REQUEST):
        self.send_json({"error": msg}, status)

    def sets(self, q) -> list[str]:
        out = []
        for s in q.get("set", []):
            if not SET_RE.match(s):
                raise ValueError(f"bad override {s!r}")
            out.append(s)
        return out

    # ---- routes ----
    def do_GET(self):
        u = urlparse(self.path)
        q = parse_qs(u.query)
        try:
            if u.path in ("/", "/index.html"):
                self.path = "/preview/web/index.html"
                return super().do_GET()
            if u.path.startswith("/web/"):
                self.path = "/preview" + u.path
                return super().do_GET()
            if u.path.startswith("/previews/") or u.path.startswith("/reference/"):
                return super().do_GET()
            if u.path == "/api/info":
                return self.send_json(tbsim("info"))
            if u.path == "/api/params":
                return self.send_json(tbsim("params"))
            if u.path == "/api/styles":
                return self.send_json(tbsim("styles", sets=self.sets(q)))
            if u.path == "/api/pose":
                face, expr = q.get("face", ["F"])[0], q.get("expr", ["rest"])[0]
                if not FACE_RE.match(face) or not WORD_RE.match(expr):
                    return self.fail("bad face/expr")
                return self.send_json(tbsim("json", "pose", face, expr, sets=self.sets(q)))
            if u.path == "/api/anim":
                face, anim = q.get("face", ["F"])[0], q.get("id", ["80"])[0]
                if not FACE_RE.match(face) or not WORD_RE.match(anim):
                    return self.fail("bad face/id")
                return self.send_json(tbsim("json", "anim", anim, face, "--fps", num(q, "fps", 30, 5, 60),
                                            "--mirror", num(q, "mirror", 0, 0, 1),
                                            "--tscale", num(q, "tscale", 1, 0.5, 2), sets=self.sets(q)))
            if u.path == "/api/reel":
                face = q.get("face", ["F"])[0]
                if not FACE_RE.match(face):
                    return self.fail("bad face")
                return self.send_json(tbsim("json", "reel", face, num(q, "seconds", 30, 1, 120),
                                            "--fps", num(q, "fps", 30, 5, 60), "--seed", num(q, "seed", 7, 0, 1e9),
                                            "--first", num(q, "first", 2500, -1, 120000), sets=self.sets(q)))
            if u.path == "/api/rounds":
                rounds = []
                for d in sorted(PREVIEWS.glob("round_*"), key=lambda p: int(p.name.split("_")[1])):
                    notes = (d / "notes.md").read_text() if (d / "notes.md").exists() else ""
                    rounds.append({"n": int(d.name.split("_")[1]), "sheet": f"/previews/{d.name}/contact_sheet.png",
                                   "notes": notes,
                                   "expressions": [f"/previews/{d.name}/{p.name}" for p in sorted(d.glob("expressions_*.png"))]})
                return self.send_json(rounds)
            if u.path == "/api/reviews":
                data = load_reviews()
                ref = REFERENCE / "approved_face.json"
                data["approved"] = json.loads(ref.read_text()) if ref.exists() else None
                return self.send_json(data)
            return self.fail("not found", HTTPStatus.NOT_FOUND)
        except ValueError as e:
            return self.fail(str(e))
        except Exception as e:  # tbsim errors etc.
            return self.fail(str(e), HTTPStatus.INTERNAL_SERVER_ERROR)

    def do_POST(self):
        u = urlparse(self.path)
        n = int(self.headers.get("Content-Length", "0") or 0)
        if n > 100_000:
            return self.fail("too large")
        try:
            body = json.loads(self.rfile.read(n) or b"{}")
        except json.JSONDecodeError:
            return self.fail("bad json")
        face = str(body.get("face", ""))
        if not FACE_RE.match(face):
            return self.fail("bad face")
        face = face.upper()
        note = str(body.get("note", ""))[:2000]
        sets = [s for s in body.get("set", []) if isinstance(s, str) and SET_RE.match(s)]
        stamp = time.strftime("%Y-%m-%dT%H:%M:%S")
        if u.path == "/api/review":
            verdict = body.get("verdict")
            if verdict not in ("approved", "maybe", "rejected", "clear"):
                return self.fail("bad verdict")
            data = load_reviews()
            if verdict == "clear":
                data["current"].pop(face, None)
            else:
                data["current"][face] = {"verdict": verdict, "note": note, "at": stamp}
            data["history"].append({"face": face, "verdict": verdict, "note": note, "at": stamp})
            REVIEWS.write_text(json.dumps(data, indent=2))
            return self.send_json(data)
        if u.path == "/api/approve":
            # Freeze the chosen design as the canonical reference for all later work.
            REFERENCE.mkdir(exist_ok=True)
            styles = json.loads(tbsim("styles", sets=sets))
            ref = {"face": face, "name": styles[face]["name"], "approved_at": stamp, "note": note,
                   "overrides": sets, "style": styles[face],
                   "source": "core/src/tb_style.cpp (+ overrides)"}
            (REFERENCE / "approved_face.json").write_text(json.dumps(ref, indent=2))
            out = REFERENCE / "renders"
            if out.exists():
                shutil.rmtree(out)
            out.mkdir()
            info = json.loads(tbsim("info"))
            for e in ["rest"] + info["expressions"]:
                tbsim("lab", str(out / f"{e}.pgm"), face, e, sets=sets)
            data = load_reviews()
            data["current"][face] = {"verdict": "approved", "note": note, "at": stamp, "canonical": True}
            data["history"].append({"face": face, "verdict": "canonical", "note": note, "at": stamp})
            REVIEWS.write_text(json.dumps(data, indent=2))
            return self.send_json({"ok": True, "reference": ref})
        return self.fail("not found", HTTPStatus.NOT_FOUND)


def main() -> None:
    build()
    port = PORT
    if len(sys.argv) > 1:
        port = int(sys.argv[1])
    srv = ThreadingHTTPServer(("127.0.0.1", port), Handler)
    print(f"TinyBuddy previewer: http://127.0.0.1:{port}   (Ctrl+C to stop)")
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
