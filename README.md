# Tehnovaga

ESP32-based onboard weight scale for truck and tractor trailers. Four load cells under the trailer floor measure cargo weight in real time, displayed on a Nextion touchscreen mounted in the cab.

**Technical lead:** Žan Starašinič

## Hardware

| Component | Model | Specs |
|-----------|-------|-------|
| MCU | ESP32 | Wi-Fi, dual-core, 240MHz |
| Load cells (x4) | YZC-516C | 10t each, 2mV/V |
| ADC (x4) | HX711 | 24-bit, 80Hz sample rate |
| Display | Nextion NX8048P070 | 7", 800x480, resistive touch |
| Power regulator | LM2596 | 12/24V DC input → 5V 3A output |

### Wiring

#### ESP32 → HX711 (load cells)

| Sensor | DOUT pin | SCK pin |
|--------|----------|---------|
| HX711 #1 | GPIO 32 | GPIO 33 |
| HX711 #2 | GPIO 25 | GPIO 26 |
| HX711 #3 | GPIO 27 | GPIO 14 |
| HX711 #4 | GPIO 12 | GPIO 13 |

#### ESP32 → Nextion display

| Function | GPIO |
|----------|------|
| ESP32 TX → Nextion RX | GPIO 17 |
| ESP32 RX ← Nextion TX | GPIO 16 |

UART2, 9600 baud, 8N1.

#### Reserved for future use

| Function | GPIO |
|----------|------|
| I2C SDA | GPIO 21 |
| I2C SCL | GPIO 22 |

#### Load cell wire colors (YZC-516C)

| Wire | Connection |
|------|------------|
| RED | Excitation+ (E+) |
| BLACK | Excitation- (E-) |
| WHITE | Signal+ (S+) |
| GREEN | Signal- (S-) |

## Firmware

### Prerequisites

- [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/)
- USB cable to ESP32

After installing ESP-IDF, source the environment:

```bash
source $IDF_PATH/export.sh
```

### Build & flash

```bash
idf.py build                # compile
idf.py flash                # flash via USB
idf.py monitor              # serial log output (Ctrl+] to exit)
idf.py flash monitor        # flash then monitor in one step
idf.py menuconfig           # edit sdkconfig
idf.py fullclean            # wipe build dir for clean rebuild
```

### Code structure

```
├── main/
│   └── main.c              # Entry point, FreeRTOS tasks, touch handlers, calibration wizard
├── components/
│   ├── hx711/              # HX711 bit-bang driver (read, tare, calibrate)
│   ├── nextion/            # Nextion UART driver (commands, touch event listener)
│   └── weight/             # Signal processing (moving avg, stability, capacity %)
├── sdkconfig.defaults      # Default ESP-IDF settings
└── CMakeLists.txt          # Root build file
```

### Data flow

```
HX711 sensors (20Hz per sensor)
    → weight component (10-sample moving average, stability detection)
    → Nextion display (updated at 5Hz)
```

### Calibration persistence

Calibration data (zero offsets + scale factors) is stored in ESP32 NVS flash. Survives power cycles and reboots. Scale factors are stored as integer × 1000 to work around NVS not supporting float types.

## Nextion display setup

The HMI file must be designed in [Nextion Editor](https://nextion.tech/nextion-editor/) and flashed to the display via microSD card.

The firmware expects two pages with specific component names and IDs. **Component IDs must match exactly** — the ESP32 identifies buttons by their numeric ID in touch events.

### Page 0 — Main screen

| Component name | Type | Component ID | Description |
|----------------|------|:------------:|-------------|
| tTotal | Text | — | Total weight across all sensors (kg) |
| tS0 | Text | — | Sensor 1 individual weight (kg) |
| tS1 | Text | — | Sensor 2 individual weight (kg) |
| tS2 | Text | — | Sensor 3 individual weight (kg) |
| tS3 | Text | — | Sensor 4 individual weight (kg) |
| jBar | Progress bar | — | Capacity bar (0–100%) |
| pStable | Variable/Dual-state | — | Stability indicator (0 = unstable, 1 = stable) |
| bTara | Button | **1** | Zero all sensors (tare) |
| bSave | Button | **2** | Log current weight (not yet implemented) |
| bCal | Button | **3** | Enter calibration wizard |

### Page 1 — Calibration wizard

| Component name | Type | Component ID | Description |
|----------------|------|:------------:|-------------|
| tStep | Text | — | Current step instructions (e.g. "1/3  Remove all weight") |
| tKg | Text | — | Known weight value being entered |
| tUnit | Text | — | Static "kg" label |
| bNext | Button | **1** | Advances wizard (label changes: NEXT → NEXT → SAVE) |
| bPlus | Button | **2** | Increase known weight value |
| bMinus | Button | **3** | Decrease known weight value |
| bStep | Button | **4** | Toggle +/- step size (cycles: 100 → 10 → 1 → 100) |
| bBack | Button | **5** | Cancel calibration, return to main screen |

> **Important:** In Nextion Editor, each button must have `Send Component ID` enabled in its touch event settings, otherwise the ESP32 won't receive the press events.

### Calibration wizard flow

1. **Step 1/3 — Zero:** Remove all weight from the platform, press NEXT. The system averages 20 readings per sensor to establish the zero point.
2. **Step 2/3 — Load:** Place a known weight on the platform, press NEXT. The system captures 20 raw readings per sensor.
3. **Step 3/3 — Enter weight:** Use +/- buttons to dial in the exact known weight in kg. Press the step-size button to switch between 100/10/1 kg increments. Press SAVE.

The system computes a scale factor per sensor, saves everything to NVS, and returns to the main screen. The calibration is immediately active.

## Signal processing

| Parameter | Value |
|-----------|-------|
| Sample rate | 20 Hz per sensor (80 Hz total across 4 sensors) |
| ADC resolution | 24-bit |
| Moving average window | 10 samples |
| Stability detection | Weight change < 5 kg for 40 consecutive updates (2 seconds) |
| Max capacity | 40,000 kg (configurable in code) |
| Display refresh | 5 Hz |

## Target specs

| Metric | Target |
|--------|--------|
| Accuracy | ±0.1% full scale (±10 kg at 10t per sensor) |
| Response time | < 200 ms |
| Boot time | < 3 seconds |
| Power consumption | < 2W operational |
| Operating temperature | -20°C to +60°C |

## FreeRTOS tasks

| Task | Stack | Priority | Rate | Purpose |
|------|-------|----------|------|---------|
| weight | 4096 B | 10 | 20 Hz | Read sensors, filter, update display |
| nextion_rx | 2048 B | 5 | continuous | Listen for touch events from display |
