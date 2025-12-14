"""Example integration: fetch PiAware aircraft data and capture images
when aircraft match criteria.

Usage:
  python3 aircraft_logger.py           # run aircraft watcher
  python3 aircraft_logger.py motion    # run motion-triggered capture
"""

import argparse
import json
import os
import time
from datetime import datetime
from math import radians, sin, cos, sqrt, atan2

import requests

from pico4ml_controller import Pico4MLController


PAWARE_URL = "http://127.0.0.1:8080/data/aircraft.json"


def haversine(lat1, lon1, lat2, lon2):
    # km
    R = 6371.0
    dlat = radians(lat2 - lat1)
    dlon = radians(lon2 - lon1)
    a = sin(dlat / 2) ** 2 + cos(radians(lat1)) * cos(radians(lat2)) * sin(dlon / 2) ** 2
    c = 2 * atan2(sqrt(a), sqrt(1 - a))
    return R * c


def fetch_aircraft(url=PAWARE_URL):
    r = requests.get(url, timeout=5)
    r.raise_for_status()
    return r.json()


def run_watcher(save_dir: str, pico: Pico4MLController, interval: float = 10.0,
                altitude_min: int = 1000, max_distance_km: float = 200.0,
                home_lat: float = None, home_lon: float = None):
    os.makedirs(save_dir, exist_ok=True)
    while True:
        try:
            data = fetch_aircraft()
            now = time.time()
            for ac in data.get('aircraft', []):
                alt = ac.get('alt_baro') or ac.get('alt_geom') or 0
                lat = ac.get('lat')
                lon = ac.get('lon')
                if alt < altitude_min:
                    continue
                if home_lat is not None and lat is not None and lon is not None:
                    d = haversine(home_lat, home_lon, lat, lon)
                    if d > max_distance_km:
                        continue
                ts = datetime.utcnow().strftime('%Y%m%d_%H%M%S')
                fname = f"{ts}_{ac.get('hex','unknown')}.jpg"
                path = os.path.join(save_dir, fname)
                pico.capture_image(path)
                meta = {
                    'timestamp': ts,
                    'aircraft': ac,
                    'image': fname
                }
                with open(os.path.join(save_dir, fname + '.json'), 'w') as f:
                    json.dump(meta, f)
                print('Captured', fname)
        except Exception as e:
            print('Error fetching/capturing:', e)
        time.sleep(interval)


def run_motion(save_dir: str, pico: Pico4MLController, threshold: float = 1.5, poll: float = 0.1):
    os.makedirs(save_dir, exist_ok=True)
    print('Motion-triggered capture. Threshold:', threshold)
    while True:
        imu = pico.read_imu()
        ax, ay, az = imu['ax'], imu['ay'], imu['az']
        mag = (ax * ax + ay * ay + az * az) ** 0.5
        if mag > threshold:
            ts = datetime.utcnow().strftime('%Y%m%d_%H%M%S')
            fname = f"motion_{ts}.jpg"
            pico.capture_image(os.path.join(save_dir, fname))
            print('Motion capture', fname)
        time.sleep(poll)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', help='Serial port for Pico')
    parser.add_argument('--dir', default='./captures', help='Save dir')
    parser.add_argument('--mode', choices=['aircraft', 'motion'], default='aircraft')
    parser.add_argument('--lat', type=float, help='Home latitude')
    parser.add_argument('--lon', type=float, help='Home longitude')
    args = parser.parse_args()
    pico = Pico4MLController(port=args.port)
    try:
        if args.mode == 'aircraft':
            run_watcher(args.dir, pico, home_lat=args.lat, home_lon=args.lon)
        else:
            run_motion(args.dir, pico)
    finally:
        pico.close()


if __name__ == '__main__':
    main()
