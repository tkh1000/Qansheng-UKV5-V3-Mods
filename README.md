# UV-K5 V3 / UV-K1 — SMS Messenger + Voice Scrambler Port

Adds the FSK SMS Messenger (+ ChaCha20 encryption) and voice scrambler from
[kamilsss655/uv-k5-firmware-custom](https://github.com/kamilsss655/uv-k5-firmware-custom)
to [armel/uv-k1-k5v3-firmware-custom](https://github.com/armel/uv-k1-k5v3-firmware-custom)
(the only working custom firmware for the UV-K5 V3 / UV-K1 with the PY32F071 chip).

---

## Files in This Package

```
uvk5v3-messenger-port/
├── build.sh                  ← ONE-COMMAND BUILD SCRIPT (run this)
├── Helper/
│   ├── fsk.h / fsk.c         ← FSK packet framing + CRC (pure C)
│   ├── crypto.h / crypto.c   ← ChaCha20-256 encryption (pure C)
├── App/
│   ├── messenger.h / .c      ← Messenger UI + keyboard + TX/RX logic
│   ├── bk4819_fsk.c          ← BK4819 FSK modem register sequences
│   └── scrambler.h           ← Voice scrambler (with implementation)
└── patch/
    ├── settings_patch.h      ← EEPROM struct fields to add
    ├── menu_patch.c          ← Menu items to add
    ├── app_patch.c           ← app.c integration hooks
    └── cmake_additions.cmake ← CMakeLists.txt additions
```

---

## Prerequisites

Install on your build machine (Linux recommended, macOS works):

```bash
# Ubuntu / Debian
sudo apt update
sudo apt install git cmake ninja-build python3 docker.io
sudo usermod -aG docker $USER   # then log out and back in

# macOS
brew install git cmake ninja python3
# + install Docker Desktop from https://docker.com
```

---

## Build in 3 Steps

### Step 1 — Run the build script

```bash
chmod +x build.sh
./build.sh
```

The script will:
- Clone `armel/uv-k1-k5v3-firmware-custom` into `./target-firmware/`
- Copy all port source files
- Patch `CMakeLists.txt` and `CMakePresets.json` automatically

### Step 2 — Apply the 4 manual patches

The script will show you the 4 files you need to edit manually in `target-firmware/`.
Open each file in your editor and follow the instructions in the `patch/` files.
This takes about 15 minutes — the patch files have exact copy-paste code.

| File to edit | Patch instructions |
|---|---|
| `App/settings.h` | `patch/settings_patch.h` |
| `App/app.c` | `patch/app_patch.c` |
| `App/menu.c` + `App/menu.h` | `patch/menu_patch.c` |
| `App/ui/status.c` (or similar) | `patch/app_patch.c` section 6 |

### Step 3 — Re-run the script and say Y to build

```bash
./build.sh
# When asked "Have you applied patches? [y/N]" → type y
```

Build takes 2–5 minutes. Output `.bin` files will be printed at the end.

---

## Build Variants

| Command | What it builds |
|---|---|
| `./build.sh` | Messenger + Encryption (no scrambler) |
| `./build.sh Fusion-Messenger-Scrambler` | Messenger + Encryption + Scrambler |
| `./build.sh clean` | Remove build directory |

---

## Flashing

1. Open **https://armel.github.io/uvtools2/** in Chrome or Edge (desktop)
2. Click **Flash Firmware** tab
3. Put your radio in **DFU mode**:
   - Power off the radio
   - Hold PTT + press power — the screen stays blank (DFU mode)
4. Connect via the **Baofeng/Kenwood serial cable** (3.5mm + 2.5mm)
   ⚠️ NOT the USB-C charging cable — that won't work for flashing
5. Select the `.bin` file and click **Flash Firmware**
6. Select the serial port when prompted

> **Always dump calibration data first!**
> Use the **Dump Calib** tab in UVTools2 before flashing any development build.

---

## Using the Messenger

| Key | Action |
|---|---|
| F + MENU | Open/activate messenger |
| \* (star) | Cycle keyboard: ABC → abc → 123 |
| 0–9 | Multi-tap character input |
| MENU | Transmit message on active VFO |
| F | Backspace |
| F (long) | Clear all messages |
| UP | Recall last sent message |
| EXIT | Close messenger |

**Menu items added:**
- `MsgMod` — FSK450 / FSK700 / AFSK1200 modulation
- `MsgRx` — Enable background FSK listener (ON/OFF)
- `MsgAck` — Auto-send ACK on receive (ON/OFF)
- `MsgEnc` — Encryption (ON/OFF) — both radios need same key
- `EncKey` — First 4 bytes of encryption key shown as hex
- `Scrambl` — Voice scrambler OFF / 2000 / 2500 / 2700 / 3000 Hz

**Notification:** When a message is received while the messenger screen is closed,
an `M` indicator appears in the top status bar.

---

## Interoperability

The FSK packet format (preamble, sync word `0x2DD4`, CRC-16/CCITT-FALSE) is
**compatible with kamilsss655 V1/V2 firmware**. A V3 radio running this port
can exchange messages with a V1/V2 radio running kamilsss655 firmware,
provided they use the same modulation and encryption settings.

---

## Flash Budget (estimated)

| | Flash | RAM |
|---|---|---|
| Fusion v5.3.1 baseline | 85,708 B (71%) | 12,480 B (76%) |
| + Messenger + Crypto | +~8 KB | +~1 KB |
| **Estimated total** | **~93 KB (79%)** | **~13.5 KB (82%)** |
| Available | 118 KB | 16 KB |

Plenty of headroom for both features.

---

## License

Apache 2.0 — same as the upstream Fusion firmware.
Please keep your fork open source if you build on this work.
