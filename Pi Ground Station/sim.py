#!/usr/bin/env python3
"""Fake LSwarm fleet -> exercises the ground station + dashboard with no hardware.

Reads config.json (make it in the setup page first), then flies N virtual drones
on circular paths and streams the exact UDP packets real hardware would send:
anchor health, drone ranges (with noise) and drone health.

Run the ground station in one terminal, this in another:
    python3 ground_station.py
    python3 sim.py 3          # 3 drones
Then open http://localhost:8080/  (or the Pi AP address).
"""
import json, math, os, random, socket, sys, time
from ground_station import CONFIG, ll_to_xy, load_config, CONF

def anchors_xy():
    load_config()
    g = CONF["geo"]
    return {int(k): (*ll_to_xy(a["lat"], a["lon"], g["origin_lat"], g["origin_lon"]),
                     a.get("z", 0.0)) for k, a in CONF["anchors"].items()}

def main(n_drones):
    if not os.path.exists(CONFIG):
        sys.exit("no config.json — open /setup and place anchors first")
    ax = anchors_xy()
    if not ax:
        sys.exit("config has no anchors")
    cx = sum(p[0] for p in ax.values()) / len(ax)
    cy = sum(p[1] for p in ax.values()) / len(ax)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    tx = ("127.0.0.1", 9000)
    print(f"sim: {len(ax)} anchors, {n_drones} drones -> {tx}")

    t0 = time.time()
    while True:
        t = time.time() - t0
        # anchors report health at 1 Hz-ish
        for aid in ax:
            sock.sendto(f"A,{aid},1,{3.9+0.05*math.sin(t+aid):.2f}".encode(), tx)
        # each drone: circle around the anchor centroid at its own radius/phase
        for d in range(1, n_drones + 1):
            r = 2.0 + 0.6 * d
            ang = 0.5 * t + d * 2.1
            px, py, pz = cx + r*math.cos(ang), cy + r*math.sin(ang), 1.5 + 0.3*d
            parts = [f"R", str(d)]
            for aid, (ax_, ay_, az_) in ax.items():
                dist = math.dist((px, py, pz), (ax_, ay_, az_)) + random.gauss(0, 0.05)
                parts += [str(aid), f"{dist:.3f}"]
            sock.sendto(",".join(parts).encode(), tx)
            sock.sendto(f"D,{d},{3.9-0.1*d:.2f},1,1".encode(), tx)
        time.sleep(0.1)   # 10 Hz

if __name__ == "__main__":
    main(int(sys.argv[1]) if len(sys.argv) > 1 else 3)
