# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

FlockSquawk is an ESP32-based **passive RF awareness system** that detects surveillance devices via WiFi promiscuous mode and Bluetooth Low Energy scanning. It monitors wireless activity in the environment by sniffing WiFi management frames and BLE advertisements, matching them against known device signatures (SSIDs, MAC OUIs, BLE names, service UUIDs). When matches are found, it provides audio alerts, visual feedback on displays, and structured JSON telemetry output.

The system is **passive only** - it does not transmit or interfere with wireless networks. It uses event-driven architecture connecting RadioScanner → ThreatAnalyzer → EventBus → Display/Audio/Telemetry components.

**Critical Constraints**:
- Must use **ESP32 board support version 3.0.7 or older**. Newer versions cause IRAM overflow and compilation failure.
- This is an **Arduino IDE project** (not PlatformIO, not standard C++ build system). Each variant is a separate sketch.
- The repository uses **git** for version control. Standard git workflow applies for committing changes to variants, signatures, or documentation.

## Build & Development Commands

### Arduino IDE Workflow

FlockSquawk uses Arduino IDE (not PlatformIO). Each hardware variant is a separate Arduino sketch:

1. **Open a variant in Arduino IDE**:
   - Navigate to the variant folder (e.g., `Mini12864/flocksquawk_mini12864/`)
   - Open the `.ino` file

2. **Configure board settings** (variant-specific):

   **Mini12864, 128x32 OLED, 128x32 Portable:**
   - Board: ESP32 Dev Module
   - Upload Speed: 115200
   - CPU Frequency: 240MHz (WiFi/BT)
   - Partition Scheme: "Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)" or "Huge APP (3MB No OTA/1MB SPIFFS)"

   **M5Stack Fire:**
   - Board: M5Stack Fire
   - Upload Speed: 115200
   - CPU Frequency: 240MHz (WiFi/BT)
   - Partition Scheme: Default

   **M5StickC Plus2:**
   - Board: M5Stick-C
   - Upload Speed: 115200
   - CPU Frequency: 240MHz (WiFi/BT)
   - Partition Scheme: Default

   **Flipper Zero (ESP32-S2):**
   - Board: ESP32S2 Dev Module
   - Upload Speed: 115200
   - CPU Frequency: 240MHz (WiFi)
   - Partition Scheme: "Default 4MB with spiffs" or "Huge APP (3MB No OTA/1MB SPIFFS)"

3. **Compile**:
   - Arduino IDE: Click "Verify" button or Ctrl+R

4. **Upload code**:
   - Arduino IDE: Click "Upload" button or Ctrl+U

5. **Upload filesystem (audio files)**:
   - Install ESP32 LittleFS plugin: https://github.com/lorol/arduino-esp32fs-plugin/releases
   - Place plugin at: `<Arduino Sketchbook>/tools/ESP32FS/tool/esp32fs.jar`
   - In Arduino IDE 2.x: Ctrl+Shift+P → search "upload" → select LittleFS upload
   - Close all serial terminals before uploading filesystem

6. **Monitor serial output**:
   - Set baud rate to 115200
   - System outputs JSON telemetry and debug messages

### Required Arduino Libraries

Core libraries (all variants except Flipper Zero):
- **ArduinoJson** (version 6.x or 7.x)
- **NimBLE-Arduino** by h2zero (not used on ESP32-S2 Flipper variant)

Display-specific libraries:
- **U8g2** by olikraus (for Mini12864 variant)
- **Adafruit GFX** + **Adafruit SSD1306** (for 128x32 OLED variants)
- **M5Unified** by M5Stack (for M5Stack Fire and M5StickC Plus2 variants)
- **Adafruit NeoPixel** (optional, for WS2811 backlight on Mini12864)

### Audio File Requirements

Audio system varies by variant:

**I2S Audio (Mini12864, 128x32 OLED standard)**
- Requires three WAV files in `data/` folder (uploaded via LittleFS)
- Files: `startup.wav`, `ready.wav`, `alert.wav`
- Format: 16-bit PCM WAV, 16000 Hz, mono, 44-byte header
- Hardware: MAX98357A I2S amplifier on GPIO 27/26/25

**M5Stack Audio (M5Fire)**
- Requires three WAV files on SD card root (not LittleFS)
- Files: `/startup.wav`, `/ready.wav`, `/alert.wav`
- Format: 16-bit PCM WAV, 16000 Hz, mono, 44-byte header
- Hardware: Built-in M5Stack FIRE speaker

**Buzzer Only (128x32 Portable, M5StickC Plus2)**
- No audio files needed (no LittleFS upload required)
- Generates tones via GPIO pin (piezo buzzer)
- Default pin: GPIO 23 (configurable)

**Flipper Zero Variant**
- No audio output (relies on Flipper Zero for alerts)
- RGB LED on GPIO 4/5/6 for visual feedback

## Repository Structure

```
FlockSquawk/
├── test_qr_size.cpp        # Standalone test utility for QR data encoding (not part of Arduino build)
├── Mini12864/              # Full-featured LCD + encoder UI
│   └── flocksquawk_mini12864/
│       ├── flocksquawk_mini12864.ino
│       ├── src/            # Core + Mini12864Display.cpp
│       ├── data/           # WAV files (LittleFS upload)
│       └── README.md
├── 128x32_OLED/            # Compact OLED displays
│   ├── flocksquawk_128x32/         # Standard OLED with I2S audio
│   │   ├── data/           # WAV files (LittleFS upload)
│   │   └── src/
│   └── flocksquawk_128x32_portable/ # Buzzer-only (NO LittleFS)
│       └── src/            # No SoundEngine.h
├── m5stack/
│   ├── flocksquawk_m5fire/   # M5Stack FIRE with SD card audio
│   │   ├── data/           # WAV files (copy to SD card root)
│   │   └── src/
│   └── flocksquawk_m5stick/  # M5StickC Plus2 with buzzer
│       └── src/            # No audio files needed
└── flipper-zero/            # ESP32-S2 firmware (WiFi only, no BLE)
    ├── dev-board-firmware/
    │   ├── flocksquawk-flipper/
    │   │   └── flocksquawk-flipper.ino
    │   └── src/            # No SoundEngine.h
    ├── flock_scanner.fap   # Prebuilt Flipper app
    └── README.md
```

**Variant-Specific Notes:**
- **Mini12864**: Full UI with menu, rotary encoder, U8g2 display, I2S audio via MAX98357A
- **128x32 OLED**: I2C OLED (Adafruit_SSD1306), I2S audio
- **128x32 Portable**: I2C OLED, piezo buzzer on GPIO (no I2S, no LittleFS)
- **M5Fire**: Uses M5Unified library, audio on SD card (not LittleFS), built-in speaker
- **M5StickC**: Uses M5Unified library, piezo buzzer, built-in display
- **Flipper Zero**: ESP32-S2 (no BLE support), UART line protocol (not JSON), RGB LED feedback

## Core Architecture

FlockSquawk uses a publish-subscribe event bus connecting these subsystems:

### Core Components (in `src/`)

1. **EventBus.h** - Lightweight pub/sub system
   - Event types: WiFiFrameCaptured, BluetoothDeviceFound, ThreatIdentified, SystemReady, AudioPlaybackRequested
   - Components subscribe to events and publish when relevant data is available
   - Single handler per event type (not multi-subscriber)

2. **RadioScanner.h** - RF scanning manager
   - WiFi: Promiscuous mode, channels 1-13, 500ms hop interval
   - BLE: NimBLE scanning, 1 second scan every 5 seconds
   - `wifiPacketHandler()` callback parses management frames (probe requests, beacons)
   - Publishes WiFiFrameEvent and BluetoothDeviceEvent to EventBus

3. **ThreatAnalyzer.h** - Pattern matching engine
   - Compares captured data against signatures in DeviceSignatures.h
   - Matches: SSID patterns, MAC OUI prefixes, BLE device names, service UUIDs
   - Calculates certainty score (0-100) based on match quality
   - Publishes ThreatEvent when match found

4. **DeviceSignatures.h** - Detection patterns
   - `NetworkNames[]` - SSID patterns to match
   - `MACPrefixes[]` - MAC OUI prefixes (e.g., "58:8e:81")
   - `BLEIdentifiers[]` - Bluetooth device name patterns
   - `RavenServices[]` - BLE service UUIDs for Raven acoustic detectors

5. **SoundEngine.h** - I2S audio playback
   - MAX98357A amplifier via I2S (pins: GPIO 27/26/25)
   - Streams WAV from LittleFS with real-time volume control
   - Volume: 0.0-1.0, default 0.4
   - Subscribes to AudioEvent

6. **TelemetryReporter.h** - Telemetry output over Serial
   - **Standard variants**: Formats threat events as structured JSON (multi-line, pretty-printed)
   - **Flipper Zero variant**: Line-based protocol (newline-terminated, no JSON)
     - Example lines: `STATUS,SCANNING`, `ALERT,RSSI=-62,MAC=AA:BB:CC:DD:EE:FF,RADIO=wifi,CH=6,ID=Flock,CERTAINTY=95`
     - UART pins: GPIO 43 (TX), GPIO 44 (RX) hardwired to Flipper GPIO header
   - Includes: timestamp, radio source, target identity, match indicators, certainty score

7. **Display variants** (e.g., Mini12864Display.h)
   - Variant-specific UI implementation
   - Common features: boot status, scanning screen, radar visualization
   - Mini12864 has menu system with encoder navigation

### Execution Flow

```
setup():
  1. Initialize LittleFS
  2. Initialize SoundEngine → play startup.wav
  3. Initialize Display (variant-specific)
  4. Wire EventBus subscriptions
  5. Initialize RadioScannerManager (WiFi + BLE)
  6. Play ready.wav
  7. Publish SystemReady event

loop():
  1. RadioScannerManager.update()
     - Switch WiFi channel every 500ms
     - Trigger BLE scan every 5 seconds
  2. Display.update() (variant-specific)
  3. Handle user input (encoder, buttons)

Event flow:
  WiFi frame → EventBus → ThreatAnalyzer → check patterns
                                          ↓ (match found)
                                    ThreatEvent → EventBus
                                          ↓
                      ┌───────────────────┴──────────────┐
                      ↓                                   ↓
              TelemetryReporter                    SoundEngine
                 (JSON output)                     (play alert.wav)
                      ↓                                   ↓
                   Display                          Display alert
```

## Common Pitfalls to Avoid

When working with this codebase, **do NOT**:
- Upgrade ESP32 board support beyond version 3.0.7 (causes IRAM overflow)
- Try to use PlatformIO or CMake build systems (this is Arduino IDE only)
- Put EventBus static member definitions in .h files (must be in main .ino file due to Arduino's compilation model)
- Use `esp_wifi_set_channel()` in WiFi callback context (keep callbacks minimal)
- Upload filesystem while serial monitor is open (will fail)
- Forget to copy EventBus boilerplate when creating new variants
- Try to use BLE on ESP32-S2 variants (S2 is WiFi-only)
- Upload WAV files to M5Fire via LittleFS (M5Fire uses SD card, not LittleFS)

## Important Implementation Details

### WiFi Sniffer Configuration
- Uses ESP32 promiscuous mode via `esp_wifi_set_promiscuous_rx_cb()`
- Filter: `WIFI_PROMIS_FILTER_MASK_MGMT` (management frames only)
- Packet handler runs in WiFi context (keep fast, avoid Serial.print)
- Frame subtypes: 0x20 = probe request, 0x80 = beacon

### BLE Scanning
- NimBLE library provides efficient scanning vs. ESP32 BLE
- Scan parameters: 1 second active scan, 5 second interval
- Callback `BLEDeviceObserver` processes advertisements immediately
- Service UUID comparison is case-insensitive and lowercase

### EventBus Pattern
- Static handlers, single subscriber per event type
- Events are structs (pass-by-reference) for efficiency
- No queuing - handlers execute synchronously when published
- **Critical**: EventBus static member definitions and method implementations MUST be in the main .ino file
  - Arduino IDE uses single-file compilation model
  - Static member initialization in .h files won't link properly
  - Look for the pattern in each variant's .ino file around line 30-80:
    ```cpp
    EventBus::WiFiFrameHandler EventBus::wifiHandler = nullptr;
    void EventBus::publishWifiFrame(const WiFiFrameEvent& event) {
        if (wifiHandler) wifiHandler(event);
    }
    ```
  - When adding new variants, copy this boilerplate from an existing variant

### Memory Constraints
- ESP32 has limited IRAM for WiFi callback functions
- Keep `wifiPacketHandler()` minimal - just parse and publish
- Complex processing happens in `ThreatAnalyzer` (runs in main context)
- Board support >3.0.7 exhausts IRAM - hence version constraint

### Audio Playback
- WAV files must be in LittleFS, not SPIFFS
- I2S DMA buffer: 512 bytes
- Volume control: multiply PCM samples before DMA write
- Files opened once per playback, closed after streaming complete

## Configuration Points

### Modify detection patterns:
- Edit `src/DeviceSignatures.h` arrays
- Add entries to NetworkNames, MACPrefixes, BLEIdentifiers, or RavenServices

### Adjust scanning behavior:
- WiFi channel range: `RadioScanner.h::MAX_WIFI_CHANNEL` (default 13)
- Channel hop interval: `RadioScanner.h::CHANNEL_SWITCH_MS` (default 500ms)
- BLE scan timing: `RadioScanner.h::BLE_SCAN_SECONDS` and `BLE_SCAN_INTERVAL_MS`

### Change audio settings:
- Default volume: `SoundEngine.h::DEFAULT_VOLUME` (0.0-1.0)
- I2S pins: `SoundEngine.h::PIN_BCLK/PIN_LRC/PIN_DATA`

### Tune display behavior:
- Display-specific settings in variant's Display.cpp
- Example: Mini12864 radar dot TTL, animation timings, backlight colors

## Testing and Development Workflow

### Quick Test Setup

1. **Create a test WiFi hotspot** on a smartphone:
   - SSID: "Flock" or "flock" (matches DeviceSignatures.h)
   - This will trigger immediate detection

2. **Monitor serial output** (115200 baud):
   - Watch for `[RF] WiFi frame captured` messages (raw frames)
   - Look for threat detection JSON output or UART messages
   - Check for audio file errors if using I2S/SD audio

3. **Test BLE detection** (not on Flipper/ESP32-S2):
   - Use nRF Connect app on smartphone
   - Advertise with device name containing "Flock" or "Penguin"
   - Or advertise Raven service UUID from DeviceSignatures.h

### Debugging Common Issues

**No WiFi frames captured:**
- Verify ESP32 board version is 3.0.7 or older
- Check that WiFi.mode(WIFI_STA) and promiscuous mode initialized
- Look for `[RF] WiFi sniffer activated` in serial output

**No BLE scanning:**
- Check for `[RF] BLE scanner activated` in serial output
- Verify NimBLE-Arduino library installed
- ESP32-S2 does not support BLE (Flipper variant)

**Audio not playing:**
- Check `[Sound] LittleFS mounted` or `[Sound] SD card mounted` in serial output
- Verify filesystem upload completed successfully
- Check for file open errors: `[Sound] Failed to open /data/startup.wav`
- For M5Stack: ensure SD card formatted as FAT32 with files in root

**Display blank:**
- Check wiring matches variant README pinout
- For I2C displays: verify I2C address (usually 0x3C)
- Check for display library installation (U8g2, Adafruit_SSD1306, M5Unified)

### Modifying Detection Signatures

When testing custom signatures:
1. Edit `src/DeviceSignatures.h` in the variant folder
2. Add test patterns to NetworkNames, MACPrefixes, BLEIdentifiers, or RavenServices arrays
3. Update array size constants (e.g., NetworkNameCount)
4. Recompile and upload
5. Test with known device that matches new signature

## Choosing a Variant for Development

**Start with Mini12864 if:**
- You want the most feature-complete reference implementation
- You need a full UI with menus and user controls
- You're developing new display features

**Start with 128x32 OLED if:**
- You want a simpler, more minimal codebase
- You need I2C display support
- You're working on compact form factors

**Start with 128x32 Portable if:**
- You want the simplest possible variant (no audio files, no I2S)
- You're building battery-powered/portable devices
- You want to understand core RF scanning without audio complexity

**Start with Flipper Zero if:**
- You're integrating with Flipper Zero ecosystem
- You need UART/serial protocol examples
- You're working with ESP32-S2 (WiFi-only, no BLE)

**Start with M5Stack Fire if:**
- You have M5Stack hardware
- You want SD card audio instead of LittleFS
- You need built-in battery/power management

## Adding New Hardware Variants

1. **Choose a base variant** to copy (see above)
2. Create new folder under FlockSquawk/ with descriptive name
3. Copy the chosen variant's structure (.ino, src/, data/, README.md)
4. Implement variant-specific display/control/audio code
5. Reuse core components from src/ (EventBus, RadioScanner, ThreatAnalyzer, etc.)
   - These files can usually be copied directly without modification
   - RadioScanner.h and ThreatAnalyzer.h are identical across most variants
6. Wire EventBus subscriptions in setup() to connect your display/audio to events
7. Copy EventBus static member implementations from the base variant's .ino file
8. Update variant-specific pin definitions and hardware initialization
9. Create variant-specific README.md with wiring and setup instructions
10. Update main README.md to list the new variant

## Troubleshooting

### Compilation fails with IRAM overflow:
- Downgrade ESP32 board support to version 3.0.7 or older

### Audio files not found:
- Verify LittleFS filesystem was uploaded via ESP32 Sketch Data Upload
- Check data/ folder contains all three WAV files
- Serial output shows specific file open errors

### No detections occurring:
- Verify target device is in range and transmitting
- Check DeviceSignatures.h contains patterns that match target
- Serial output shows WiFi frames being captured (raw MAC/SSID)
- BLE devices must advertise; passive/non-advertising devices won't be detected

### Display not responding:
- Check wiring matches variant's README pinout
- Verify correct display library installed:
  - Mini12864: U8g2 (SPI display, ST7567 driver)
  - 128x32 OLED variants: Adafruit_SSD1306 and Adafruit_GFX (I2C display)
  - M5Stack variants: M5Unified
- For I2C displays: use I2C scanner sketch to verify address (usually 0x3C, sometimes 0x3D)
- For SPI displays: verify CS, DC, RST, SCK, MOSI pins match configuration

### M5Stack-specific issues:
- **SD card not detected**: Format as FAT32, check card seated properly
- **Audio not playing**: Verify WAV files on SD card root (not in data/ folder, not via LittleFS)
- **Display issues**: Ensure M5Unified library installed (not M5Stack legacy library)

### Flipper Zero-specific issues:
- **No UART output to Flipper**: Verify dev board seated on Flipper GPIO header, check baud rate 115200
- **Compilation errors on S2**: Verify ESP32S2 Dev Module selected (not ESP32 Dev Module)
- **BLE errors on S2**: Normal - ESP32-S2 does not support BLE, only WiFi scanning works

### Upload fails:
- Hold ESP32 BOOT button during upload attempt
- Lower upload speed to 115200 or 9600
- Use quality USB data cable (not charge-only)
- Install CH340 or CP210x USB drivers if needed
- For M5Stack: ensure device not in deep sleep, press reset button
