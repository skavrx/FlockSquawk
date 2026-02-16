# GPS Integration Summary

## What Was Added

Your M5StickC Plus2 FlockSquawk now supports GPS location tracking! When a camera or surveillance device is detected, the system logs the GPS coordinates where the detection occurred.

## Files Added/Modified

### New Files Created:
1. **`src/GPSHandler.h`** - GPS module interface and NMEA parser
2. **`GPS_INSTALLATION.md`** - Complete GPS setup guide
3. **`GPS_WIRING.txt`** - Quick reference wiring diagram
4. **`GPS_SUMMARY.md`** - This file

### Modified Files:
1. **`flocksquawk_m5stick.ino`** - Added GPS initialization and integration
2. **`src/TelemetryReporter.h`** - Added GPS coordinate logging
3. **`README.md`** - Updated to mention GPS support

## Quick Start Guide

### 1. Install Library
Open Arduino IDE:
- **Tools** → **Manage Libraries**
- Search: **TinyGPSPlus**
- Click **Install**

### 2. Wire the GPS Module
Connect BN-220 to M5StickC Plus2 Grove port:
```
BN-220        M5StickC Plus2
VCC     →     3.3V (red)
GND     →     GND (black)
TX      →     GPIO 33 (white)  ← MOST IMPORTANT
RX      →     GPIO 32 (yellow) [optional]
```

### 3. Upload Firmware
- Open `flocksquawk_m5stick.ino`
- Click **Upload**
- Wait for compilation and upload

### 4. Test GPS
- Open Serial Monitor (115200 baud)
- Look for: `[GPS] GPS module initialized`
- Take device outdoors
- Wait 1-2 minutes for GPS fix
- Watch for: `[GPS] Location updated: XX.XXXXXX, XX.XXXXXX`

### 5. Trigger Detection
- Create WiFi hotspot named "Flock"
- Watch JSON output include location data

## Example Output

When a threat is detected with GPS fix:

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
    "category": "surveillance_device",
    "certainty": 85
  },
  "location": {
    "latitude": 37.774929,
    "longitude": -122.419418,
    "altitude_m": 15.2,
    "satellites": 8,
    "hdop": 1.2,
    "fix_valid": true,
    "timestamp": "2026-02-16T18:45:23Z"
  }
}
```

## Key Features

### Location Data Included:
- **Latitude/Longitude**: WGS84 decimal degrees
- **Altitude**: Meters above mean sea level
- **Satellites**: Number of satellites in use
- **HDOP**: Horizontal accuracy (lower is better)
- **Timestamp**: UTC time from GPS satellites
- **Fix Status**: Whether GPS has valid position

### Smart Integration:
- No performance impact on WiFi/BLE scanning
- GPS runs on separate hardware (Serial2)
- Automatic fix status checking
- Graceful degradation if GPS not connected

### Power Efficient:
- GPS updates processed in background
- No blocking operations
- Can continue detecting even without GPS fix

## Important Notes

### GPS Requires:
✅ Clear view of sky (outdoor or window)
✅ 1-2 minutes for initial fix (cold start)
✅ At least 4 satellites for position
✅ BN-220 TX connected to GPIO 33

### GPS Will NOT Work:
❌ Deep indoors (buildings block signals)
❌ Inside metal enclosures
❌ Underground or in tunnels
❌ If wiring is incorrect

### Performance:
- **Cold Start**: 30 seconds - 2 minutes
- **Warm Start**: 5-30 seconds
- **Hot Start**: 1-5 seconds
- **Accuracy**: 2.5 meters CEP (typical)

## Use Cases

### 1. Surveillance Mapping
Create a map of camera locations:
- Walk through area with FlockSquawk
- Log all detections with GPS coordinates
- Plot on Google Maps or similar
- Identify surveillance hotspots

### 2. Evidence Collection
Document encounters:
- GPS proves location of detection
- Timestamp proves time of detection
- Can be used for reports or analysis

### 3. Coverage Analysis
Understand detection patterns:
- Where are cameras most common?
- Which routes have most surveillance?
- Are there camera-free paths?

### 4. Historical Tracking
Build database over time:
- Compare camera locations month-to-month
- Track new camera installations
- Identify camera removal

## Data Privacy

⚠️ **Important Privacy Considerations**:

1. **Your Location**: GPS logs YOUR location, not just camera locations
2. **Data Storage**: Serial output contains your location history
3. **Logs**: If you save logs to SD card, they contain your movements
4. **Sharing**: Be careful who you share GPS logs with
5. **Security**: GPS data is transmitted unencrypted over serial

**Recommendations**:
- Only enable GPS when actively mapping
- Delete logs after extracting needed data
- Consider encryption if storing long-term
- Be aware of privacy laws in your area

## Troubleshooting

### "No GPS module initialized" message
→ Install TinyGPSPlus library and recompile

### GPS initializes but shows 0 satellites
→ Go outdoors, wait 2-5 minutes for cold start

### GPS shows satellites but no position
→ Need at least 4 satellites for 3D fix, wait longer

### GPS position is wildly incorrect
→ Some modules ship with test coordinates, wait for real fix

### GPS stops working after some time
→ Check wiring - loose connection on GPIO 33

## Advanced Features

### Change Update Rate
Edit `src/GPSHandler.h`:
```cpp
static const uint16_t GPS_UPDATE_MS = 1000;  // milliseconds
```

### Use Different Pins
Edit `src/GPSHandler.h`:
```cpp
static const uint8_t GPS_RX_PIN = 33;  // Your RX pin
static const uint8_t GPS_TX_PIN = 32;  // Your TX pin
```

### Configure GPS Module
Send NMEA commands to change GPS behavior:
- Update rate (1Hz, 5Hz, 10Hz)
- Output sentences (enable/disable specific NMEA)
- Power saving modes
- Datum selection

See GPS_INSTALLATION.md for NMEA command examples.

## Next Steps

### Option 1: Basic Use
- Wire GPS module as shown
- Upload firmware
- Go outside and test
- Trigger detection to see GPS in JSON

### Option 2: SD Card Logging
- Add SD card support (M5StickC Plus2 has SD slot)
- Log JSON to files with timestamps
- Review logs on computer
- Plot locations on map

### Option 3: Real-Time Display
- Modify display code to show GPS status
- Show satellite count on screen
- Display coordinates on detection
- Add GPS indicator LED

### Option 4: Advanced Analysis
- Parse JSON logs with Python/JavaScript
- Create interactive maps (Leaflet, Google Maps)
- Statistical analysis of surveillance density
- Heat maps of camera locations

## Resources

- **TinyGPSPlus Documentation**: https://github.com/mikalhart/TinyGPSPlus
- **NMEA Sentence Reference**: https://www.gpsinformation.org/dale/nmea.htm
- **BN-220 Datasheet**: Search "BN-220 GPS module datasheet"
- **GPS Testing Tools**: GPS Test app (Android), GPS Status (iOS)

## Support

For detailed instructions, see:
- **GPS_INSTALLATION.md** - Complete setup guide
- **GPS_WIRING.txt** - Wiring quick reference
- **README.md** - General FlockSquawk documentation

## Changelog

**2026-02-16**: Initial GPS integration
- Added GPSHandler class for BN-220 support
- Integrated GPS into telemetry JSON output
- Created comprehensive documentation
- Added wiring guides and examples
