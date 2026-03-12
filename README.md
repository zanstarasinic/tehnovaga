# Tehnovaga

ESP32-based onboard weight scale for truck/tractor trailers. Four load cells measure cargo weight in real time, displayed on a 7" Nextion touchscreen.

## Quick start

Requires [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/).

```bash
source $IDF_PATH/export.sh    # load ESP-IDF environment
idf.py build                  # compile
idf.py flash monitor          # flash + serial log (Ctrl+] to exit)
```

### Run tests (no hardware needed)

```bash
cd test && make test
```

## How it works

```
HX711 load cells (×4)  →  weight processing  →  Nextion display
       20 Hz               10-sample avg            5 Hz
```

The system reads 4 load cells at 20 Hz, applies a moving average filter, and pushes the result to the display at 5 Hz. Calibration (zero offsets + scale factors) is saved to NVS flash and survives reboots.

If a sensor fails 10 consecutive reads (~0.5s), it's marked offline and the display shows `ERR` for that sensor.

## Code structure

```
main/main.c              app entry, FreeRTOS tasks, calibration wizard, touch handlers
components/
  hx711/                  HX711 bit-bang ADC driver (4 sensors)
  nextion/                Nextion UART driver + touch event listener
  weight/                 moving average, stability detection, capacity %
test/                     host-based unit tests (runs on macOS/Linux, no ESP32 needed)
```

## Wiring

### Load cells (HX711)

| Sensor | DOUT | SCK |
|--------|------|-----|
| #1 | GPIO 32 | GPIO 33 |
| #2 | GPIO 25 | GPIO 26 |
| #3 | GPIO 27 | GPIO 14 |
| #4 | GPIO 12 | GPIO 13 |

### Nextion display (UART2, 9600 baud)

| | GPIO |
|-|------|
| ESP32 TX → Nextion RX | 17 |
| ESP32 RX ← Nextion TX | 16 |

### Load cell wire colors (YZC-516C)

RED = E+, BLACK = E-, WHITE = S+, GREEN = S-

## Nextion HMI setup

Design the HMI in [Nextion Editor](https://nextion.tech/nextion-editor/), flash to display via microSD.

**Component IDs must match exactly** — the ESP32 identifies buttons by their numeric ID in touch events. Every button must have `Send Component ID` enabled in Nextion Editor.

### Page 0 — Main screen

| Name | Type | ID | What it shows / does |
|------|------|----|----------------------|
| tTotal | Text | — | Total weight (kg), prefixed with `!` if any sensor is down |
| tS0–tS3 | Text | — | Individual sensor weight (kg), or `ERR` if offline |
| jBar | Progress bar | — | Capacity 0–100% |
| pStable | Dual-state | — | Stability indicator (1 = stable) |
| bTara | Button | **1** | Zero all sensors (tare) |
| bSave | Button | **2** | Log weight (not yet implemented) |
| bCal | Button | **3** | Open calibration wizard |

### Page 1 — Calibration wizard

| Name | Type | ID | What it shows / does |
|------|------|----|----------------------|
| tStep | Text | — | Step instructions ("1/3 Remove all weight") |
| tKg | Text | — | Known weight value being entered |
| tUnit | Text | — | Static "kg" label |
| bNext | Button | **1** | Advance step (NEXT → NEXT → SAVE) |
| bPlus | Button | **2** | Increase weight value |
| bMinus | Button | **3** | Decrease weight value |
| bStep | Button | **4** | Toggle step size (100 → 10 → 1 → 100) |
| bBack | Button | **5** | Cancel, return to main screen |

### Calibration flow

1. **Remove all weight** → press NEXT → system zeros all sensors (20-sample average)
2. **Place known weight** → press NEXT → system captures raw readings
3. **Enter weight** → use +/- to set kg value, press SAVE → computes scale factors, saves to NVS

Calibration is active immediately after saving.

## Hardware

| Component | Model | Specs |
|-----------|-------|-------|
| MCU | ESP32 | Wi-Fi, dual-core, 240 MHz |
| Load cells (×4) | YZC-516C | 10t each, 2 mV/V |
| ADC (×4) | HX711 | 24-bit, 80 Hz |
| Display | Nextion NX8048P070 | 7", 800×480, resistive touch |
| Power | LM2596 | 12/24V → 5V, 3A |

## Technical details

| Parameter | Value |
|-----------|-------|
| Sensor sample rate | 20 Hz per sensor |
| Moving average window | 10 samples |
| Stability | weight change < 5 kg for 2s (40 updates) |
| Max capacity | 40,000 kg |
| Display refresh | 5 Hz |
| Sensor failure threshold | 10 consecutive read failures |
| FreeRTOS weight task | priority 10, 4096 B stack |
| FreeRTOS nextion listener | priority 5, 2048 B stack |
| NVS scale factor storage | integer × 1000 (avoids float NVS limitation) |
