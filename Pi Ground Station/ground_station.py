#!/usr/bin/env python3
"""LSwarm ground station for a Raspberry Pi Zero 2 W (WiFi AP).

Does four jobs in one process:
  1. UDP: receive UWB ranges + health from drones/anchors, trilaterate, reply.
  2. HTTP: serve the setup + live dashboard web apps (open them from any device
     on the LSwarm AP -> http://192.168.4.1:8080 ).
  3. SSE: push live fleet state to the dashboard (/events).
  4. Tile proxy/cache: serve satellite imagery, caching each tile to disk so the
     field (no internet) still shows the map after one online setup.

Config lives in config.json (written by the setup page):
  {"geo": {"origin_lat":.., "origin_lon":..},
   "anchors": {"1": {"lat":.., "lon":.., "z":..}, ...}}

Local frame: metres, origin at geo.origin, x=east y=north (equirectangular).
The drones range in metres and we trilaterate in metres; positions are also
converted to lat/lon for the map.

Wire protocol (UDP, CSV, port 9000):
  drone -> pi   R,<id>,<aid>,<dist>,<aid>,<dist>,...   ranges (metres)
  drone -> pi   D,<id>,<vbat>,<armed>,<uwbok>          health
  anchor-> pi   A,<aid>,<uwbok>,<vbat>                 health
  pi -> drone   P,<x>,<y>,<z>,<q>   solved position    (to drone :9001)

Run:  python3 ground_station.py         (serve + solve)
      python3 ground_station.py --demo  (self-check the math)
"""
import json, math, os, socket, sys, threading, time, urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import numpy as np

HERE      = os.path.dirname(os.path.abspath(__file__))
CONFIG    = os.path.join(HERE, "config.json")
TILE_DIR  = os.path.join(HERE, "tiles")
UDP_PORT  = 9000
HTTP_PORT = 8080
DRONE_RX  = 9001
# Esri World Imagery (free, no key). Tile order is z/y/x.
ESRI = "https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}"

# ---- geo <-> local metres (equirectangular around origin) -----------------
def ll_to_xy(lat, lon, o_lat, o_lon):
    x = math.radians(lon - o_lon) * math.cos(math.radians(o_lat)) * 6371000.0
    y = math.radians(lat - o_lat) * 6371000.0
    return x, y

def xy_to_ll(x, y, o_lat, o_lon):
    lat = o_lat + math.degrees(y / 6371000.0)
    lon = o_lon + math.degrees(x / (6371000.0 * math.cos(math.radians(o_lat))))
    return lat, lon

# ---- shared fleet state ---------------------------------------------------
LOCK   = threading.Lock()
CONF   = {"geo": {"origin_lat": 0.0, "origin_lon": 0.0}, "anchors": {}}
STATE  = {"anchors": {}, "drones": {}}   # live health/positions keyed by id str

def load_config():
    if os.path.exists(CONFIG):
        with open(CONFIG) as f:
            data = json.load(f)
        CONF.clear(); CONF.update(data)   # mutate in place so imports stay valid

def anchors_xyz():
    """Config anchors -> {id:int -> np.array([x,y,z])} in local metres."""
    g = CONF["geo"]
    out = {}
    for k, a in CONF["anchors"].items():
        x, y = ll_to_xy(a["lat"], a["lon"], g["origin_lat"], g["origin_lon"])
        out[int(k)] = np.array([x, y, a.get("z", 0.0)])
    return out

# ---- positioning ----------------------------------------------------------
def trilaterate(anchors, ranges):
    """Least-squares 3D position from {aid: dist}. >=4 anchors for a full 3D
    fix; 3 still solves but z is weak. Subtract anchor-0's sphere eqn -> Ax=b."""
    ids = [a for a in ranges if a in anchors]
    if len(ids) < 3:
        return None
    p = np.array([anchors[a] for a in ids])
    d = np.array([ranges[a] for a in ids])
    p0, d0 = p[0], d[0]
    A = 2.0 * (p[1:] - p0)
    b = (d0**2 - d[1:]**2) + np.sum(p[1:]**2, axis=1) - np.sum(p0**2)
    sol, *_ = np.linalg.lstsq(A, b, rcond=None)
    return sol, len(ids)

# ---- UDP receiver ---------------------------------------------------------
def udp_loop():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("0.0.0.0", UDP_PORT))
    print(f"UDP up on :{UDP_PORT}")
    while True:
        data, addr = sock.recvfrom(512)
        f = data.decode(errors="ignore").strip().split(",")
        now = time.time()
        try:
            if f[0] == "R":
                did = f[1]
                nums = f[2:]
                ranges = {int(nums[i]): float(nums[i+1]) for i in range(0, len(nums)-1, 2)}
                with LOCK:
                    res = trilaterate(anchors_xyz(), ranges)
                    g = CONF["geo"]
                    d = STATE["drones"].setdefault(did, {})
                    d["ranges"] = ranges; d["seen"] = now
                    if res is not None:
                        (x, y, z), q = res
                        lat, lon = xy_to_ll(x, y, g["origin_lat"], g["origin_lon"])
                        d.update(x=float(x), y=float(y), z=float(z), q=int(q),
                                 lat=lat, lon=lon)
                        sock.sendto(f"P,{x:.3f},{y:.3f},{z:.3f},{q}".encode(),
                                    (addr[0], DRONE_RX))
            elif f[0] == "D":
                with LOCK:
                    d = STATE["drones"].setdefault(f[1], {})
                    d.update(vbat=float(f[2]), armed=int(f[3]), uwbok=int(f[4]),
                             ip=addr[0], seen=now)
            elif f[0] == "A":
                with LOCK:
                    a = STATE["anchors"].setdefault(f[1], {})
                    a.update(uwbok=int(f[2]), vbat=float(f[3]), ip=addr[0], seen=now)
        except (ValueError, IndexError):
            pass

def snapshot():
    """Merge config anchor coords + live health into one JSON-able dict."""
    now = time.time()
    with LOCK:
        g = CONF["geo"]
        anchors = {}
        for k, a in CONF["anchors"].items():
            live = STATE["anchors"].get(k, {})
            anchors[k] = {"lat": a["lat"], "lon": a["lon"], "z": a.get("z", 0.0),
                          "uwbok": live.get("uwbok"), "vbat": live.get("vbat"),
                          "age": round(now - live["seen"], 1) if "seen" in live else None}
        drones = {}
        for k, d in STATE["drones"].items():
            drones[k] = {**{kk: d.get(kk) for kk in
                            ("lat","lon","x","y","z","q","vbat","armed","uwbok")},
                         "age": round(now - d["seen"], 1) if "seen" in d else None}
        return {"geo": g, "anchors": anchors, "drones": drones}

# ---- tile cache -----------------------------------------------------------
def get_tile(z, x, y):
    path = os.path.join(TILE_DIR, z, x, f"{y}.png")
    if os.path.exists(path):
        with open(path, "rb") as fp:
            return fp.read()
    try:
        url = ESRI.format(z=z, y=y, x=x)
        req = urllib.request.Request(url, headers={"User-Agent": "LSwarm"})
        blob = urllib.request.urlopen(req, timeout=8).read()
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as fp:
            fp.write(blob)
        return blob
    except Exception:
        return None      # offline + not cached -> blank tile

# ---- HTTP server ----------------------------------------------------------
class Handler(BaseHTTPRequestHandler):
    def log_message(self, *a):  # quiet
        pass

    def _send(self, code, body, ctype="application/json"):
        if isinstance(body, str):
            body = body.encode()
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _file(self, name, ctype):
        p = os.path.join(HERE, name)
        if not os.path.exists(p):
            return self._send(404, "not found", "text/plain")
        with open(p, "rb") as fp:
            self._send(200, fp.read(), ctype)

    def do_GET(self):
        path = self.path.split("?")[0]
        if path in ("/", "/dashboard", "/dashboard.html"):
            return self._file("dashboard.html", "text/html")
        if path in ("/setup", "/setup.html"):
            return self._file("setup.html", "text/html")
        if path == "/leaflet.js":
            return self._file("leaflet.js", "application/javascript")
        if path == "/leaflet.css":
            return self._file("leaflet.css", "text/css")
        if path == "/config":
            return self._send(200, json.dumps(CONF))
        if path == "/state":
            return self._send(200, json.dumps(snapshot()))
        if path.startswith("/tiles/"):
            try:
                _, _, z, x, y = path.split("/")
                blob = get_tile(z, x, y.replace(".png", ""))
            except ValueError:
                blob = None
            if blob is None:
                return self._send(204, b"", "image/png")
            return self._send(200, blob, "image/png")
        if path == "/events":
            return self._sse()
        return self._send(404, "not found", "text/plain")

    def _sse(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.end_headers()
        try:
            while True:
                self.wfile.write(f"data: {json.dumps(snapshot())}\n\n".encode())
                self.wfile.flush()
                time.sleep(0.25)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def do_POST(self):
        if self.path != "/config":
            return self._send(404, "not found", "text/plain")
        n = int(self.headers.get("Content-Length", 0))
        try:
            body = json.loads(self.rfile.read(n))
            assert "geo" in body and "anchors" in body
        except Exception as e:
            return self._send(400, json.dumps({"err": str(e)}))
        with LOCK:
            CONF.clear(); CONF.update(body)
            with open(CONFIG, "w") as f:
                json.dump(CONF, f, indent=2)
        self._send(200, json.dumps({"ok": True}))

# ---- entry ----------------------------------------------------------------
def serve():
    load_config()
    threading.Thread(target=udp_loop, daemon=True).start()
    httpd = ThreadingHTTPServer(("0.0.0.0", HTTP_PORT), Handler)
    print(f"HTTP up on :{HTTP_PORT}  ->  http://192.168.4.1:{HTTP_PORT}/setup  (setup)")
    print(f"                              http://192.168.4.1:{HTTP_PORT}/       (live)")
    httpd.serve_forever()

def _demo():
    anchors = {1: np.array([0,0,0.]), 2: np.array([4,0,0.]),
               3: np.array([0,4,0.]), 4: np.array([2,2,2.5])}
    truth = np.array([1.5, 2.0, 1.0])
    ranges = {a: float(np.linalg.norm(p - truth)) for a, p in anchors.items()}
    est, q = trilaterate(anchors, ranges)
    assert q == 4 and np.allclose(est, truth, atol=1e-6), est
    # geo round-trip
    la, lo = xy_to_ll(*ll_to_xy(37.5, -122.1, 37.4, -122.0), 37.4, -122.0)
    assert abs(la-37.5) < 1e-6 and abs(lo+122.1) < 1e-6
    print("demo ok:", est)

if __name__ == "__main__":
    if len(sys.argv) == 2 and sys.argv[1] == "--demo":
        _demo()
    else:
        serve()
