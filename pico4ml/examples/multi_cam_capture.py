"""Example: cycle through multiple cameras and capture frames.

Requires `pico4ml_controller.py` and a connected Pico4ML running the
multi-camera firmware.
"""

import os
import time
from pico4ml_controller import Pico4MLController


def main():
    pico = Pico4MLController()
    save_dir = './multi_captures'
    os.makedirs(save_dir, exist_ok=True)
    try:
        for cam in range(4):
            out = os.path.join(save_dir, f'cam{cam}_{int(time.time())}.jpg')
            print('Capturing from camera', cam)
            pico.capture_image_from(out, camera_index=cam)
            time.sleep(0.2)
    finally:
        pico.close()


if __name__ == '__main__':
    main()
