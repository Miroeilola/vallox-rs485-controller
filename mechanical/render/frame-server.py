# SPDX-License-Identifier: CERN-OHL-S-2.0
# SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
import http.server, os, sys
WWW = sys.argv[1]; FRAMES = sys.argv[2]
class H(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **k): super().__init__(*a, directory=WWW, **k)
    def do_POST(self):
        n = int(self.headers['Content-Length'])
        name = os.path.basename(self.path)
        with open(os.path.join(FRAMES, name), 'wb') as f:
            f.write(self.rfile.read(n))
        self.send_response(200); self.end_headers()
    def log_message(self, *a): pass
http.server.ThreadingHTTPServer(('127.0.0.1', 8932), H).serve_forever()
