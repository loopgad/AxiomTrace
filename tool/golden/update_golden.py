#!/usr/bin/env python3
"""Generate golden frames and expected decoder output."""

import subprocess
import json
import sys
from pathlib import Path

GOLDEN_DIR = Path(__file__).parent / "frames"
EXPECTED_DIR = Path(__file__).parent / "expected"


def run():
    GOLDEN_DIR.mkdir(exist_ok=True)
    EXPECTED_DIR.mkdir(exist_ok=True)

    # For now, generate simple synthetic golden frames
    # In a full setup, this would compile and run a C encoder program.

    frames = [
        ("frame_01_minimal", bytes([
            0xA5, 0x10, 0x01, 0x03, 0x01, 0x00, 0x00, 0x00,  # header: INFO, mod=3, evt=1, seq=0
            0x00,                                            # payload len = 0
            0x00, 0x00,                                      # CRC placeholder (computed below)
        ])),
        ("frame_02_u16", bytes([
            0xA5, 0x10, 0x01, 0x03, 0x01, 0x00, 0x01, 0x00,  # header: INFO, mod=3, evt=1, seq=1
            0x03,                                            # payload len = 3
            0x03, 0xE8, 0x03,                                # u16(1000)
            0x00, 0x00,                                      # CRC placeholder
        ])),
    ]

    sys.path.insert(0, str(Path(__file__).parent.parent / "decoder"))
    from axiom_decoder import crc16_ccitt_false

    for name, data in frames:
        # Compute CRC over header + payload_len + payload
        crc_data = data[:-2]
        crc = crc16_ccitt_false(crc_data)
        data = crc_data + bytes([crc & 0xFF, crc >> 8])
        (GOLDEN_DIR / f"{name}.bin").write_bytes(data)

        # Decode to get expected JSON
        from axiom_decoder import decode_stream
        decoded = decode_stream(data)
        (EXPECTED_DIR / f"{name}.json").write_text(json.dumps(decoded, indent=2))
        print(f"Generated {name}.bin + {name}.json")


if __name__ == '__main__':
    run()
