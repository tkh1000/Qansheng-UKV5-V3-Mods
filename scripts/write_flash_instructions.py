#!/usr/bin/env python3
"""write_flash_instructions.py — Write FLASH_INSTRUCTIONS.txt into the artifact dir."""
import sys
import os

out_path = sys.argv[1] if len(sys.argv) > 1 else "artifacts/FLASH_INSTRUCTIONS.txt"
os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)

text = (
    "UV-K5 V3 / UV-K1 -- Firmware Flashing Instructions\n"
    "===================================================\n"
    "\n"
    "STEP 0 -- BACKUP CALIBRATION (do this every time before flashing)\n"
    "  1. Open https://armel.github.io/uvtools2/ in Chrome or Edge (desktop)\n"
    "  2. Dump Calib tab -> connect radio via Kenwood cable (3.5mm + 2.5mm)\n"
    "     WARNING: USB-C is power-only and cannot flash firmware\n"
    "  3. Save the calibration .bin file somewhere safe\n"
    "\n"
    "STEP 1 -- ENTER DFU MODE\n"
    "  1. Power the radio OFF\n"
    "  2. Hold PTT, then press Power -- screen stays blank = DFU mode\n"
    "\n"
    "STEP 2 -- FLASH\n"
    "  1. https://armel.github.io/uvtools2/ -> Flash Firmware tab\n"
    "  2. Select the .bin file from this artifact\n"
    "  3. Choose your serial/COM port -> Flash -> wait for green bar\n"
    "  4. Power-cycle the radio\n"
    "\n"
    "FIRST BOOT\n"
    "  Settings reset to defaults (EEPROM version bumped). Reconfigure channels.\n"
    "\n"
    "MESSENGER  (F + MENU to open)\n"
    "  * (star)      cycle ABC / abc / 123 input modes\n"
    "  0-9           multi-tap character input\n"
    "  MENU          transmit on active VFO\n"
    "  F             backspace\n"
    "  F (long)      clear all messages\n"
    "  UP            recall last sent message\n"
    "  EXIT          close messenger\n"
    "\n"
    "NEW MENU ITEMS\n"
    "  MsgMod   FSK modulation  (FSK450 / FSK700 / AFSK1200)\n"
    "  MsgRx    Background FSK listener  (ON / OFF)\n"
    "  MsgAck   Auto-ACK on receive  (ON / OFF)\n"
    "  MsgEnc   ChaCha20 encryption  (ON / OFF)\n"
    "  EncKey   Key preview (first 4 bytes as hex)\n"
    "  Scrambl  Voice scrambler  (OFF / 2000 / 2500 / 2700 / 3000 Hz)\n"
    "\n"
    "SCRAMBLER / DE-SCRAMBLER\n"
    "  Same Scrambl setting de-scrambles received audio automatically.\n"
    "  Frequency inversion is self-inverse -- no separate de-scrambler setting.\n"
    "  Both radios must use the same Scrambl inversion frequency.\n"
    "\n"
    "INTEROPERABILITY\n"
    "  FSK-compatible with kamilsss655 V1/V2 firmware.\n"
    "  Both sides must use the same MsgMod and EncKey settings.\n"
)

with open(out_path, "w") as f:
    f.write(text)

print(f"Flash instructions written to {out_path}")
