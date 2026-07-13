"""PolyHome web dashboard — static file server + API proxy.

The browser cannot call the PolyHome API directly (CORS), so this serves
index.html and forwards every /api/* request to the real server.

Run:  python server.py   ->  http://localhost:8765
"""
import http.server
import json
import ssl
import urllib.error
import urllib.request
from pathlib import Path

UPSTREAM = "https://polyhome.lesmoulinsdudev.com"
PORT = 8765

class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **kw):
        super().__init__(*a, directory=str(Path(__file__).parent), **kw)

    def do_GET(self):
        if self.path.startswith("/api/"):
            self.proxy("GET")
        else:
            super().do_GET()

    def do_POST(self):
        self.proxy("POST")

    def do_DELETE(self):
        self.proxy("DELETE")

    def proxy(self, method):
        length = int(self.headers.get("Content-Length") or 0)
        body = self.rfile.read(length) if length else None
        req = urllib.request.Request(UPSTREAM + self.path, data=body, method=method)
        for h in ("Content-Type", "Authorization"):
            if self.headers.get(h):
                req.add_header(h, self.headers[h])
        ctx = ssl.create_default_context()
        try:
            with urllib.request.urlopen(req, context=ctx, timeout=15) as r:
                data, code = r.read(), r.status
                ctype = r.headers.get("Content-Type", "application/json")
        except urllib.error.HTTPError as e:
            data, code = e.read(), e.code
            ctype = e.headers.get("Content-Type", "application/json")
        except Exception as e:
            data = json.dumps({"error": str(e)}).encode()
            code, ctype = 502, "application/json"
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def log_message(self, fmt, *args):
        print(f"{self.command} {self.path} -> {args[1] if len(args) > 1 else ''}")

if __name__ == "__main__":
    print(f"PolyHome dashboard on http://localhost:{PORT}  (proxy -> {UPSTREAM})")
    http.server.ThreadingHTTPServer(("127.0.0.1", PORT), Handler).serve_forever()
