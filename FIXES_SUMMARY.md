# FlockSquawk M5StickC Plus2 - Fixes and New Features

## Issues Fixed and Features Added

### 1. ✅ Fixed QR Code Crash (GFX Error)

**Root Cause:**
- The QR code generation was failing because the GPX XML data exceeded the QR version 6 capacity (~136 bytes)
- No error checking was performed, causing the code to draw an invalid QR code
- Stack allocation of large QR buffer could cause stack overflow

**Solutions Implemented:**
- Added **error checking** for `qrcode_initText()` return value
- Added **data size validation** before QR generation (max 130 bytes)
- Switched to **heap allocation** for QR buffer to prevent stack overflow
- Implemented **compact CSV format** instead of verbose GPX XML (reduces size by ~70%)
- Display clear error messages if data is too large

**New Compact Format:**
```
FS:lat,lon,YYYYMMDDHHMMSS,rssi,id|lat,lon,YYYYMMDDHHMMSS,rssi,id|...
```

### 2. ✅ Persistent Flash Storage

**Implementation:**
- Uses ESP32 **Preferences library** (NVS) for flash storage
- Detections are **automatically saved to flash** when new detection occurs
- Detections are **loaded from flash on boot** - survives reboots!
- Maximum 20 detections stored (configurable via `MAX_STORED_DETECTIONS`)

**Functions Added:**
- `saveDetectionsToFlash()` - Saves all detections to NVS
- `loadDetectionsFromFlash()` - Loads detections on boot

### 3. ✅ Multi-Page UI with Detection List

**New UI Pages:**

#### Main UI (Default)
- Shows WiFi channel, GPS status, coordinates, detections count, battery
- Same as before

#### Detection List Page
- Shows stored detections one at a time
- Displays: ID, radio type, RSSI, GPS coordinates, date/time
- Scroll through detections with Button A
- Shows "Detection #X of Y" counter

**Button Controls:**

| Button | Action | Description |
|--------|--------|-------------|
| **A** (short press) | Scroll detections | On detection list page, cycles through stored detections |
| **A** (double-click) | Show QR code | On main UI, displays QR code with detection data |
| **C** (short press) | Toggle page | Switches between Main UI ↔ Detection List |
| **C** (long press 2s) | Power saver | Toggles power saving mode (existing feature) |

## How to Use the New Features

### Viewing Stored Detections

1. **Press Button C** once to switch to Detection List page
2. **Press Button A** to scroll through detections
3. **Press Button C** again to return to Main UI

### Exporting Detection Data (QR Code)

1. Make sure you're on the Main UI (press C if needed)
2. **Double-click Button A** (two quick presses)
3. Scan the QR code with your phone
4. The data is in compact CSV format: `FS:lat,lon,timestamp,rssi,id|...`
5. Press any button to exit QR view

### Notes:
- **QR code only shows detections with GPS fix**
- If you have more than ~3-4 detections, the QR code may be too large
- In that case, you'll see an error message suggesting to reduce detections or export via serial

### Clearing Old Detections

If you need to clear the detection list to make room for new QR exports:
1. The system stores max 20 detections (circular buffer)
2. Oldest detections are automatically overwritten
3. To manually clear: reflash the firmware or use the serial console (feature could be added)

## Testing the Fixes

### Test QR Code Fix:
1. Make sure GPS has a fix
2. Trigger 1-2 detections (create WiFi hotspot named "Flock")
3. Double-click Button A to show QR code
4. Should display QR code successfully
5. Try with 10+ detections - should show "Data too large" error

### Test Persistent Storage:
1. Trigger some detections with GPS fix
2. Reboot the device (press reset button)
3. After boot, press Button C to view detection list
4. Stored detections should still be there!

### Test Detection List:
1. Store several detections
2. Press Button C to switch to detection list
3. Press Button A to scroll through detections
4. Each detection should show full details
5. Press Button C to return to main UI

## Technical Details

### Changes Made to Code:

1. **Added Preferences library** for NVS flash storage
2. **New UIPage enum** for page state management
3. **New display function** `drawDetectionList()` for showing detections
4. **Compact GPX generation** `generateCompactGPX()` for smaller QR codes
5. **Enhanced QR display** with error checking and heap allocation
6. **Modified button handling** for page navigation and scrolling
7. **Load detections on boot** in `setup()`
8. **Save detections on detect** in `storeDetection()`

### Memory Usage:
- Flash storage: ~1KB per 20 detections (NVS)
- Heap allocation for QR: ~500 bytes (temporary, freed after use)
- Stack saved: ~500 bytes (moved QR buffer to heap)

## Troubleshooting

### QR Code Issues:
- **"Data too large"** - Reduce number of detections or export via serial
- **"Memory error"** - Device low on heap memory, reboot recommended
- **"QR encode failed"** - Data format issue, check serial log

### Detection List Issues:
- **"No stored detections yet"** - Need GPS fix to store detections
- **Detections not persisting** - Check if flash is working (serial log shows load status)

### Serial Output:
- On boot: `[STORAGE] Loaded X detections from flash`
- This confirms persistent storage is working

## Future Enhancements (Optional)

Potential additions:
1. Export full GPX via serial instead of QR for large datasets
2. Manual "Clear All Detections" function
3. Filter detection list by radio type (WiFi/BLE)
4. Show detection on map preview (if screen size allows)
5. Export to SD card instead of QR code

---

**All changes are in:** `m5stack/flocksquawk_m5stick/flocksquawk_m5stick.ino`

Compile and upload to your M5StickC Plus2 to test!
