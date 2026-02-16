# BN-220 GPS Module Installation for M5StickC Plus2

## Overview

This guide explains how to add GPS location tracking to your FlockSquawk M5StickC Plus2 device. When a threat is detected, the system will log the GPS coordinates in the JSON telemetry output.

## Hardware Requirements

- **M5StickC Plus2** (running FlockSquawk firmware)
- **BN-220 GPS Module** (or compatible UART GPS module)
- **4 jumper wires** (female-to-female or female-to-male depending on your setup)
- Optional: Small breadboard for easier wiring

## BN-220 GPS Module Pinout

The BN-220 typically has 4 pins:
```
VCC  - Power (3.3V or 5V)
GND  - Ground
TX   - Transmit data (GPS → ESP32)
RX   - Receive data (ESP32 → GPS, optional for basic operation)
```

## Wiring Diagram

Connect the BN-220 to your M5StickC Plus2 as follows:

```
BN-220 GPS          M5StickC Plus2
-----------         ---------------
VCC       ───────>  3.3V (pin on back Grove connector)
GND       ───────>  GND  (pin on back Grove connector)
TX        ───────>  GPIO 33 (Grove connector)
RX        ───────>  GPIO 32 (Grove connector, optional)
```

### M5StickC Plus2 Grove Connector Pinout

The Grove connector on the back of the M5StickC Plus2 has:
```
Pin 1 (Yellow): GPIO 32
Pin 2 (White):  GPIO 33
Pin 3 (Red):    3.3V
Pin 4 (Black):  GND
```

## Detailed Wiring Steps

1. **Power Off** your M5StickC Plus2 before making connections

2. **Connect Ground (GND)**
   - Connect BN-220 GND to M5StickC Plus2 GND (black wire on Grove)

3. **Connect Power (VCC)**
   - Connect BN-220 VCC to M5StickC Plus2 3.3V (red wire on Grove)
   - Note: BN-220 can work with 3.3V or 5V, but use 3.3V for safety

4. **Connect GPS TX → ESP32 RX**
   - Connect BN-220 TX pin to GPIO 33 on M5StickC Plus2 (white wire on Grove)
   - This allows GPS data to flow from the module to the ESP32

5. **Connect ESP32 TX → GPS RX (Optional)**
   - Connect GPIO 32 to BN-220 RX (yellow wire on Grove)
   - Only needed if you want to send configuration commands to GPS module
   - For basic operation (reading location), this connection is optional

## Physical Setup Tips

### Using Grove Connector (Easiest)
1. If you have a Grove-to-4pin cable, this is the cleanest solution
2. Connect directly to the Grove port on the back of M5StickC Plus2
3. Match the wire colors as described above

### Using Loose Wires
1. You may need to solder header pins to the Grove pads if they're not populated
2. Use female-to-female jumper wires to connect to the BN-220
3. Consider using heat shrink or electrical tape to secure connections

### Mounting the GPS Module
1. **Antenna Placement**: The BN-220 has a ceramic patch antenna
   - Face the antenna upward (toward the sky)
   - Keep metal objects away from the antenna
   - Avoid placing inside metal enclosures

2. **Cable Management**: Keep GPS wires short but allow flexibility for positioning

3. **Outdoor Use**: The BN-220 needs a clear view of the sky
   - Initial GPS fix can take 30 seconds to 2 minutes
   - Subsequent fixes are faster (hot start)
   - Indoor use may not get a GPS fix

## Software Setup

### Required Arduino Library

Install the **TinyGPSPlus** library:
1. Open Arduino IDE
2. Go to **Tools** → **Manage Libraries**
3. Search for "TinyGPSPlus" by Mikal Hart
4. Click **Install**

### Compile and Upload

1. Open `flocksquawk_m5stick.ino` in Arduino IDE
2. Verify the GPS pins in `src/GPSHandler.h` match your wiring:
   ```cpp
   static const uint8_t GPS_RX_PIN = 33;  // Connect to BN-220 TX
   static const uint8_t GPS_TX_PIN = 32;  // Connect to BN-220 RX
   ```
3. Compile and upload the firmware

## Verification and Testing

### 1. Check Serial Output

Open Serial Monitor (115200 baud) and look for:
```
[GPS] GPS module initialized on Serial2
[GPS] RX pin: 33, TX pin: 32, Baud: 9600
```

### 2. Wait for GPS Fix

Initial GPS fix requires:
- Clear view of the sky (outdoors or near a window)
- 30 seconds to 2 minutes for cold start
- 5-30 seconds for warm start

You'll see messages like:
```
[GPS] Location updated: 37.774929, -122.419418 (sats: 8)
```

### 3. Trigger a Detection

Create a test WiFi hotspot with SSID "Flock" to trigger detection.

### 4. Check JSON Output

When a threat is detected, the JSON output will include GPS data:

**With Valid GPS Fix:**
```json
{
  "event": "target_detected",
  "ms_since_boot": 45678,
  "source": {
    "radio": "wifi",
    "channel": 6,
    "rssi": -62
  },
  "target": {
    "identity": {
      "mac": "aa:bb:cc:dd:ee:ff",
      "oui": "aa:bb:cc",
      "label": "Flock"
    },
    "indicators": {
      "ssid_match": true,
      "mac_match": false,
      "name_match": false,
      "service_uuid_match": false
    },
    "category": "surveillance_device",
    "certainty": 85
  },
  "metadata": {
    "frame_type": "beacon",
    "detection_method": "combined_signature"
  },
  "location": {
    "latitude": 37.774929,
    "longitude": -122.419418,
    "altitude_m": 15.2,
    "satellites": 8,
    "hdop": 1.2,
    "fix_age_ms": 450,
    "fix_valid": true,
    "timestamp": "2026-02-16T18:45:23Z"
  }
}
```

**Without GPS Fix (Still Waiting):**
```json
{
  ...
  "location": {
    "fix_valid": false,
    "satellites": 0,
    "note": "Waiting for GPS fix"
  }
}
```

## Troubleshooting

### No GPS Output in Serial Monitor

**Problem**: You don't see `[GPS] GPS module initialized` message

**Solutions**:
- Verify TinyGPSPlus library is installed
- Check that `src/GPSHandler.h` is present in the project
- Recompile and upload the firmware

### GPS Not Getting Fix

**Problem**: System shows 0 satellites, no location updates

**Solutions**:
1. **Check Wiring**: Verify BN-220 TX is connected to GPIO 33
2. **Check Power**: Ensure BN-220 has 3.3V power (LED should light up)
3. **Check Antenna**: Position GPS module with antenna facing up
4. **Go Outside**: GPS needs clear view of sky, won't work indoors
5. **Wait Longer**: Cold start can take 2-5 minutes
6. **Check Module**: Red LED on BN-220 should blink once per second when searching

### GPS Data Garbled

**Problem**: Seeing strange characters instead of NMEA sentences

**Solutions**:
- Verify baud rate is 9600 (default for BN-220)
- Check TX/RX connections aren't swapped
- Ensure good electrical connections (no loose wires)

### GPS Works But Coordinates Wrong

**Problem**: Getting GPS fix but location is incorrect

**Solutions**:
- Wait for more satellites (need at least 4 for 3D fix)
- Check HDOP value (lower is better, <2 is good)
- Some modules ship with test coordinates in memory - wait for real fix
- Verify you're using WGS84 datum (TinyGPSPlus default)

### Interferes with WiFi Scanning

**Problem**: WiFi detections slow down after adding GPS

**Solutions**:
- This shouldn't happen - GPS uses separate Serial2 hardware
- Verify you're not blocking in GPS code
- Check that `gpsModule.update()` is called in loop() without delays

## Advanced Configuration

### Change GPS Update Rate

Edit `src/GPSHandler.h`:
```cpp
static const uint16_t GPS_UPDATE_MS = 1000;  // Status update interval
```

### Change GPS Pins

If you need to use different pins, edit `src/GPSHandler.h`:
```cpp
static const uint8_t GPS_RX_PIN = 33;  // Your RX pin
static const uint8_t GPS_TX_PIN = 32;  // Your TX pin
```

### Configure GPS Module Settings

The BN-220 can be configured via NMEA commands. To send configuration:

```cpp
void GPSHandler::sendCommand(const char* cmd) {
    gpsSerial.println(cmd);
}
```

Example commands:
- Set 5Hz update rate: `$PMTK220,200*2C`
- Set 1Hz update rate: `$PMTK220,1000*1F`
- Enable only RMC+GGA: `$PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28`

## GPS Data Format Reference

The location object includes:

| Field | Type | Description |
|-------|------|-------------|
| `latitude` | float | Latitude in decimal degrees (-90 to 90) |
| `longitude` | float | Longitude in decimal degrees (-180 to 180) |
| `altitude_m` | float | Altitude in meters above mean sea level |
| `satellites` | int | Number of satellites used for fix |
| `hdop` | float | Horizontal Dilution of Precision (lower = better) |
| `fix_age_ms` | int | Milliseconds since last GPS update |
| `fix_valid` | bool | True if GPS has valid fix |
| `timestamp` | string | UTC timestamp from GPS (ISO 8601 format) |

## Power Consumption Notes

- BN-220 GPS module typically draws 20-30mA
- This will reduce M5StickC Plus2 battery life
- Consider power management strategies for extended operation
- GPS antenna draws more power when actively searching for fix

## Alternative GPS Modules

The code should work with other UART GPS modules that output NMEA sentences:

- **NEO-6M** - Common alternative, similar to BN-220
- **NEO-7M** - Faster fix time
- **NEO-8M** - Better sensitivity
- **Air530** - Lower power consumption
- **ATGM336H** - Chinese alternative

All of these use 9600 baud NMEA by default and should work with TinyGPSPlus.

## Safety and Legal Considerations

- **Privacy**: GPS logging records your location. Handle data responsibly.
- **Security**: GPS data in JSON is transmitted over serial unencrypted
- **Legal**: Ensure your use complies with local laws regarding location tracking
- **Storage**: Consider storing logs securely if saving to SD card

## Support

If you encounter issues:
1. Check wiring carefully against this guide
2. Verify GPS module has clear view of sky
3. Monitor serial output for GPS status messages
4. Test GPS module separately with simple NMEA reader sketch
5. Check TinyGPSPlus library documentation for parser details

## Example Use Case

This GPS integration enables you to:
1. **Map Detections**: Plot surveillance device locations on a map
2. **Track Movement**: Record where you encountered devices
3. **Pattern Analysis**: Identify high-density surveillance areas
4. **Evidence**: Document location and time of detections
5. **Geofencing**: Trigger alerts in specific areas (requires custom code)
