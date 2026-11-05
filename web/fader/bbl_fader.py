#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# BBL Fader: the phone's fader to the FM-1, one Windows app (web/fader/build_exe.sh makes the .exe). The phone opens phone.html
# from here over Wi-Fi (the window's QR code) and posts its CCs; this sends them to the FM-1 by USB through Windows' own MIDI
# (winmm). Only MIDI CC messages pass. The MIDI port is only held while the fader moves, so the install and Samples pages can use it.
import ctypes
import json
import os
import pathlib
import re
import socket
import sys
import threading
import time
import tkinter as tk
from ctypes import wintypes
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from tkinter import font as tkfont, messagebox

import segno

HERE = pathlib.Path(getattr(sys, "_MEIPASS", pathlib.Path(__file__).resolve().parent))
PORT = 8765
BIND = os.environ.get("BBL_FADER_BIND") or "0.0.0.0"   # (the env: for testing with no firewall question)
MATCH = re.compile(os.environ.get("BBL_FADER_MATCH") or r"fm-?1|felucca|m-?vave|bbl", re.I)   # (the env: for testing without an FM-1)
HOLD_S = 5.0      # let go of the MIDI port this long after the last move
PHONE_S = 6.0     # the phone asks every 2 s: this long without a word = gone
FONT_FILE = "BLBPixel7-Chunky.ttf"

BLACK, WHITE, GREY, GREEN, AMBER = "#000000", "#ffffff", "#9a9a9a", "#35d07f", "#ffb020"


class MidiOutCaps(ctypes.Structure):
    _fields_ = [("wMid", wintypes.WORD), ("wPid", wintypes.WORD), ("vDriverVersion", wintypes.UINT),
                ("szPname", wintypes.WCHAR * 32), ("wTechnology", wintypes.WORD), ("wVoices", wintypes.WORD),
                ("wNotes", wintypes.WORD), ("wChannelMask", wintypes.WORD), ("dwSupport", wintypes.DWORD)]


winmm = ctypes.windll.winmm
winmm.midiOutGetNumDevs.restype = wintypes.UINT
winmm.midiOutGetDevCapsW.argtypes = [ctypes.c_size_t, ctypes.POINTER(MidiOutCaps), wintypes.UINT]
winmm.midiOutOpen.argtypes = [ctypes.POINTER(ctypes.c_void_p), wintypes.UINT, ctypes.c_size_t, ctypes.c_size_t, wintypes.DWORD]
winmm.midiOutShortMsg.argtypes = [ctypes.c_void_p, wintypes.DWORD]
winmm.midiOutClose.argtypes = [ctypes.c_void_p]


class Fm1:
    """The FM-1's MIDI port. state: 'gone' (not plugged in), 'ready', or 'busy' (another program has it)."""

    def __init__(self):
        self.lock = threading.Lock()
        self.handle = None
        self.state = "gone"
        self.last = 0.0

    def find(self):
        caps = MidiOutCaps()
        for i in range(winmm.midiOutGetNumDevs()):
            if winmm.midiOutGetDevCapsW(i, ctypes.byref(caps), ctypes.sizeof(caps)) == 0 and MATCH.search(caps.szPname):
                return i
        return None

    def close(self):
        if self.handle:
            winmm.midiOutClose(self.handle)      # (no midiOutReset: that would cut whatever the FM-1 is playing)
            self.handle = None

    def send(self, messages):
        """1 = sent, 0 = not (the window says why)."""
        with self.lock:
            if not self.handle:
                dev = self.find()
                if dev is None:
                    self.state = "gone"
                    return 0
                handle = ctypes.c_void_p()
                if winmm.midiOutOpen(ctypes.byref(handle), dev, 0, 0, 0) != 0:
                    self.state = "busy"
                    return 0
                self.handle = handle
            for st, cc, val in messages:
                if winmm.midiOutShortMsg(self.handle, st | cc << 8 | val << 16) != 0:
                    self.close()                 # (unplugged: tick looks for it again)
                    self.state = "gone"
                    return 0
            self.state = "ready"
            self.last = time.monotonic()
            return 1

    def tick(self):
        """Once a second: let go when idle, and notice the FM-1 coming and going."""
        with self.lock:
            if self.handle and time.monotonic() - self.last > HOLD_S:
                self.close()
            if not self.handle:
                dev = self.find()
                if dev is None:
                    self.state = "gone"
                elif self.state == "busy":       # (has the other program let go? then the phone sends again)
                    handle = ctypes.c_void_p()
                    if winmm.midiOutOpen(ctypes.byref(handle), dev, 0, 0, 0) == 0:
                        winmm.midiOutClose(handle)
                        self.state = "ready"
                else:
                    self.state = "ready"

    def heard(self):
        return 1 if self.state == "ready" else 0


fm1 = Fm1()
phone_seen = 0.0
port_taken = False


def lan_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("10.255.255.255", 1))         # (sends nothing: asks which address faces the network)
        return s.getsockname()[0]
    except OSError:
        return None
    finally:
        s.close()


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    disable_nagle_algorithm = True               # (no waiting to fill a packet: the fader must feel direct)

    def log_message(self, *args):
        pass

    def reply(self, body, kind="text/plain; charset=utf-8", code=200):
        self.send_response(code)
        self.send_header("Content-Type", kind)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        global phone_seen
        path = self.path.split("?")[0]
        if path == "/":
            self.reply((HERE / "phone.html").read_bytes(), "text/html; charset=utf-8")
        elif path == "/font.ttf":
            self.reply((HERE / FONT_FILE).read_bytes(), "font/ttf")
        elif path == "/ping":
            phone_seen = time.monotonic()
            self.reply(str(fm1.heard()).encode())
        else:
            self.reply(b"not here", code=404)

    def do_POST(self):
        global phone_seen
        n = int(self.headers.get("Content-Length", 0) or 0)
        # (the JSON type: a web page elsewhere can't send it here without asking first, and nothing here says yes)
        if self.path != "/cc" or n > 1000 or self.headers.get("Content-Type") != "application/json":
            if n <= 1000:
                self.rfile.read(n)
            self.close_connection = True
            self.reply(b"no", code=400)
            return
        try:
            data = json.loads(self.rfile.read(n))
            for st, cc, val in data:
                if not (0xB0 <= st <= 0xBF and 0 <= cc <= 127 and 0 <= val <= 127):
                    raise ValueError
        except (ValueError, TypeError):
            self.reply(b"no", code=400)
            return
        phone_seen = time.monotonic()
        self.reply(str(fm1.send(data)).encode())


class Window:
    def __init__(self, root):
        self.root = root
        self.url = None
        root.title("BBL Fader")
        root.configure(bg=BLACK)
        root.resizable(False, False)
        family = pixel_family(root)
        big = (family, 28) if family else ("Segoe UI", 20, "bold")
        mid = (family, 16) if family else ("Segoe UI", 12, "bold")
        small = ("Segoe UI", 10)

        pad = tk.Frame(root, bg=BLACK, padx=24, pady=20)
        pad.pack()
        tk.Label(pad, text="BBL FADER", font=big, fg=WHITE, bg=BLACK).pack(anchor="w")
        self.fm1_line = self.status(pad, mid)
        self.phone_line = self.status(pad, mid)
        self.canvas = tk.Canvas(pad, bg=WHITE, highlightthickness=0)
        self.canvas.pack(pady=(16, 8))
        self.address = tk.Label(pad, font=small, fg=GREY, bg=BLACK)
        self.address.pack()
        tk.Label(pad, text="Phone and PC on the same Wi-Fi. Keep this window open while you play.\n"
                           "Close it before installing firmware.",
                 font=small, fg=GREY, bg=BLACK, justify="center").pack(pady=(8, 0))
        root.protocol("WM_DELETE_WINDOW", self.quit)
        self.refresh()

    def status(self, parent, font):
        row = tk.Frame(parent, bg=BLACK)
        row.pack(anchor="w", pady=(10, 0))
        dot = tk.Canvas(row, width=14, height=14, bg=BLACK, highlightthickness=0)
        dot.pack(side="left", padx=(0, 10))
        mark = dot.create_oval(1, 1, 13, 13, fill=AMBER, outline="")
        label = tk.Label(row, font=font, fg=WHITE, bg=BLACK, anchor="w")
        label.pack(side="left")
        return dot, mark, label

    def show(self, line, ok, text):
        dot, mark, label = line
        dot.itemconfigure(mark, fill=GREEN if ok else AMBER)
        if label.cget("text") != text:
            label.configure(text=text)

    def draw_qr(self, url):
        rows = segno.make(url, error="m").matrix
        size, quiet, cell = len(rows), 4, 6      # (the white margin a camera needs)
        side = (size + 2 * quiet) * cell
        self.canvas.delete("all")
        self.canvas.configure(width=side, height=side)
        for y, row in enumerate(rows):
            for x, on in enumerate(row):
                if on:
                    px, py = (x + quiet) * cell, (y + quiet) * cell
                    self.canvas.create_rectangle(px, py, px + cell, py + cell, fill=BLACK, outline="")

    def refresh(self):
        if port_taken:
            messagebox.showerror("BBL Fader", "BBL Fader is already open, or another program is using its port "
                                              f"({PORT}). Close it and try again.")
            self.quit()
        ip = lan_ip()
        url = f"http://{ip}:{PORT}/" if ip else None
        if url != self.url:
            self.url = url
            if url:
                self.draw_qr(url)
                self.address.configure(text="Scan with your phone's camera, or type: " + url)
            else:
                self.canvas.delete("all")
                self.canvas.configure(width=1, height=1)
                self.address.configure(text="This PC isn't on a network. Connect it to your Wi-Fi.")
        self.show(self.fm1_line, fm1.state == "ready",
                  {"gone": "FM-1: plug it in and switch it on", "busy": "FM-1: another app is using it",
                   "ready": "FM-1: connected"}[fm1.state])
        here = time.monotonic() - phone_seen < PHONE_S and phone_seen > 0
        self.show(self.phone_line, here, "Phone: connected" if here else "Phone: scan the code below")
        self.root.after(400, self.refresh)

    def quit(self):
        with fm1.lock:
            fm1.close()
        os._exit(0)


def pixel_family(root):
    """Blair's font for this program only (not installed on the PC). None: it didn't load, use Windows' own."""
    try:
        if not ctypes.windll.gdi32.AddFontResourceExW(str(HERE / FONT_FILE), 0x10, 0):   # (FR_PRIVATE)
            return None
        return next((f for f in tkfont.families(root) if "BLB" in f.upper()), None)
    except (OSError, tk.TclError):
        return None


class OnePort(ThreadingHTTPServer):
    allow_reuse_address = False                  # (on Windows it lets two programs share a port: a second copy must fail)


def serve():
    global port_taken
    try:
        server = OnePort((BIND, PORT), Handler)
    except OSError:
        port_taken = True
        return
    server.daemon_threads = True
    server.serve_forever()


def ticker():
    while True:
        fm1.tick()
        time.sleep(1.0)


def main():
    try:
        ctypes.windll.shcore.SetProcessDpiAwareness(1)   # (sharp on a scaled screen: a blurry QR code is hard to scan)
    except (OSError, AttributeError):
        pass
    root = tk.Tk()
    root.report_callback_exception = lambda kind, e, tb: messagebox.showerror("BBL Fader", f"BBL Fader hit a problem: {e!r}")
    Window(root)
    threading.Thread(target=serve, daemon=True).start()   # (after the window: Windows' firewall question comes with this)
    threading.Thread(target=ticker, daemon=True).start()
    root.mainloop()


if __name__ == "__main__":
    try:
        main()
    except Exception as e:                       # (the .exe has no console: never die with nothing on screen)
        messagebox.showerror("BBL Fader", f"BBL Fader stopped: {e!r}")
