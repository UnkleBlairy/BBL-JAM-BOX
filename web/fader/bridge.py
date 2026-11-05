#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# The phone link for the crossfader. The phone opens / (phone.html) over Wi-Fi and posts its CCs here; desktop.html, open in
# Chrome on this computer, hears them and sends them to the FM-1 by USB (only a page on this computer can use Web MIDI over plain
# http). Python's standard library only. Only MIDI CC messages pass through. Port: first argument (default 8765).
import json, pathlib, queue, socket, sys, threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HERE = pathlib.Path(__file__).resolve().parent
PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8765
listeners, lock = set(), threading.Lock()


def lan_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("10.255.255.255", 1))     # (sends nothing: asks which address faces the network)
        return s.getsockname()[0]
    except OSError:
        return "127.0.0.1"
    finally:
        s.close()


def share(msg):
    with lock:
        for q in listeners:
            q.put(msg)


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    disable_nagle_algorithm = True           # (no waiting to fill a packet: the fader must feel direct)

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
        path = self.path.split("?")[0]
        if path == "/":
            self.reply((HERE / "phone.html").read_bytes(), "text/html; charset=utf-8")
        elif path == "/desktop":
            page = (HERE / "desktop.html").read_text(encoding="utf-8").replace("PHONE_URL", f"http://{lan_ip()}:{PORT}/")
            self.reply(page.encode("utf-8"), "text/html; charset=utf-8")
        elif path == "/font.ttf":
            self.reply((HERE / "BLBPixel7-Chunky.ttf").read_bytes(), "font/ttf")
        elif path == "/events":
            self.events()
        elif path == "/ping":                 # (the phone asks: is a desktop page listening?)
            with lock:
                heard = len(listeners)
            self.reply(str(heard).encode())
        else:
            self.reply(b"not here", code=404)

    def do_POST(self):
        n = int(self.headers.get("Content-Length", 0) or 0)
        if self.path != "/cc" or n > 1000:
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
        share(json.dumps(data))
        with lock:
            heard = len(listeners)
        self.reply(str(heard).encode())       # how many desktop pages it reached

    def events(self):
        q = queue.Queue()
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Connection", "close")
        self.end_headers()
        self.close_connection = True
        with lock:
            listeners.add(q)
        try:
            self.wfile.write(b": hello\n\n")
            self.wfile.flush()
            while True:
                try:
                    self.wfile.write(f"data: {q.get(timeout=10)}\n\n".encode())
                except queue.Empty:
                    self.wfile.write(b": still here\n\n")
                self.wfile.flush()
        except OSError:
            pass
        finally:
            with lock:
                listeners.discard(q)


print(f"\n  BBL macro fader link is running.\n\n  On this PC, in Chrome:  http://localhost:{PORT}/desktop\n  On your phone, in Chrome:  http://{lan_ip()}:{PORT}/\n\n  Close this window to stop it.\n", flush=True)
ThreadingHTTPServer(("0.0.0.0", PORT), Handler).serve_forever()
