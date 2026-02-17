#include <Arduino.h>
#include <WiFi.h>
#include <NimBLEDevice.h>
#include <NimBLEScan.h>
#include <NimBLEAdvertisedDevice.h>
#include <ArduinoJson.h>
#include <M5Unified.h>
#include <Preferences.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"

#include "src/EventBus.h"
#include "src/DeviceSignatures.h"
#include "src/RadioScanner.h"
#include "src/ThreatAnalyzer.h"
#include "src/TelemetryReporter.h"
#include "src/GPSHandler.h"
#include "src/qrcode.h"

// Global system components
RadioScannerManager rfScanner;
ThreatAnalyzer threatEngine;
TelemetryReporter reporter;
GPSHandler gpsModule;
Preferences preferences;

// Detection storage for GPX export
struct StoredDetection {
    double latitude;
    double longitude;
    int year, month, day, hour, minute, second;
    char identifier[32];
    char radioType[10];
    int8_t rssi;
    bool valid;
};

const uint8_t MAX_STORED_DETECTIONS = 20;
StoredDetection detectionHistory[MAX_STORED_DETECTIONS];
uint8_t detectionHistoryIndex = 0;
uint8_t detectionHistoryCount = 0;

// Event bus handler implementations
EventBus::WiFiFrameHandler EventBus::wifiHandler = nullptr;
EventBus::BluetoothHandler EventBus::bluetoothHandler = nullptr;
EventBus::ThreatHandler EventBus::threatHandler = nullptr;
EventBus::SystemEventHandler EventBus::systemReadyHandler = nullptr;

// Save detections to flash memory
void saveDetectionsToFlash() {
    preferences.begin("flocksquawk", false);
    preferences.putUChar("detCount", detectionHistoryCount);
    preferences.putUChar("detIndex", detectionHistoryIndex);

    for (uint8_t i = 0; i < detectionHistoryCount; i++) {
        char key[16];
        snprintf(key, sizeof(key), "det%d", i);
        preferences.putBytes(key, &detectionHistory[i], sizeof(StoredDetection));
    }

    preferences.end();
}

// Load detections from flash memory
void loadDetectionsFromFlash() {
    preferences.begin("flocksquawk", true);  // Read-only mode
    detectionHistoryCount = preferences.getUChar("detCount", 0);
    detectionHistoryIndex = preferences.getUChar("detIndex", 0);

    for (uint8_t i = 0; i < detectionHistoryCount; i++) {
        char key[16];
        snprintf(key, sizeof(key), "det%d", i);
        preferences.getBytes(key, &detectionHistory[i], sizeof(StoredDetection));
    }

    preferences.end();
    Serial.printf("[STORAGE] Loaded %d detections from flash\n", detectionHistoryCount);
}

// Function to store a detection in history
void storeDetection(const ThreatEvent& threat) {
    if (!gpsModule.hasValidFix()) {
        return;  // Only store detections with valid GPS
    }

    StoredDetection& det = detectionHistory[detectionHistoryIndex];
    det.latitude = gpsModule.getLatitude();
    det.longitude = gpsModule.getLongitude();
    gpsModule.getDateTime(det.year, det.month, det.day, det.hour, det.minute, det.second);
    strncpy(det.identifier, threat.identifier, sizeof(det.identifier) - 1);
    det.identifier[sizeof(det.identifier) - 1] = '\0';
    strncpy(det.radioType, threat.radioType, sizeof(det.radioType) - 1);
    det.radioType[sizeof(det.radioType) - 1] = '\0';
    det.rssi = threat.rssi;
    det.valid = true;

    detectionHistoryIndex = (detectionHistoryIndex + 1) % MAX_STORED_DETECTIONS;
    if (detectionHistoryCount < MAX_STORED_DETECTIONS) {
        detectionHistoryCount++;
    }

    // Save to flash memory
    saveDetectionsToFlash();
}

// Generate compact GPX XML from stored detections
String generateGPX() {
    String gpx = "<?xml version=\"1.0\"?>\n<gpx version=\"1.1\" creator=\"FlockSquawk\">\n";

    for (uint8_t i = 0; i < detectionHistoryCount; i++) {
        StoredDetection& det = detectionHistory[i];
        if (!det.valid) continue;

        gpx += "<wpt lat=\"";
        gpx += String(det.latitude, 6);
        gpx += "\" lon=\"";
        gpx += String(det.longitude, 6);
        gpx += "\">\n";

        if (det.year > 0) {
            char timeStr[32];
            snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                det.year, det.month, det.day, det.hour, det.minute, det.second);
            gpx += "<time>";
            gpx += timeStr;
            gpx += "</time>\n";
        }

        gpx += "<name>";
        gpx += det.identifier;
        gpx += "</name>\n";

        gpx += "<desc>";
        gpx += det.radioType;
        gpx += ", ";
        gpx += det.rssi;
        gpx += "dBm</desc>\n";

        gpx += "</wpt>\n";
    }

    gpx += "</gpx>";
    return gpx;
}

namespace {
    const uint16_t STARTUP_BEEP_FREQ = 2000;
    const uint16_t ALERT_BEEP_FREQ = 2600;
    const uint16_t BEEP_DURATION_MS = 80;
    const uint16_t BEEP_GAP_MS = 60;
    const uint16_t RADAR_LINE_COLOR = TFT_GREEN;
    const uint16_t STATUS_TEXT_COLOR = TFT_WHITE;
    const uint32_t DOT_UPDATE_MS = 500;
    const uint32_t BATTERY_UPDATE_MS = 3000;
    const uint32_t GPS_UPDATE_MS = 1000;
    const uint32_t RSSI_UPDATE_MS = 500;
    const int8_t RSSI_MIN_DBM = -100;
    const int8_t RSSI_MAX_DBM = -20;
    const uint32_t ALERT_DURATION_MS = 4000;
    const uint32_t ALERT_FLASH_MS = 300;
    const uint16_t ALERT_BEEP_MS = 180;
    const uint32_t SCREEN_ON_MS = 4000;
    const uint32_t POWER_SAVE_MSG_MS = 2000;
    const uint32_t STATUS_MSG_MS = 1500;
    const uint8_t DISPLAY_BRIGHTNESS_ON = 80;

    void playBeepPattern(uint16_t frequency, uint16_t durationMs, uint8_t count) {
        for (uint8_t i = 0; i < count; i++) {
            M5.Speaker.tone(frequency, durationMs);
            delay(durationMs + BEEP_GAP_MS);
        }
    }

    int8_t latestRssi = RSSI_MIN_DBM;
    bool alertActive = false;
    bool alertVisible = false;
    uint32_t alertStartMs = 0;
    uint32_t alertLastFlashMs = 0;
    uint32_t alertUntilMs = 0;
    uint32_t detectionCount = 0;
    bool powerSaverEnabled = true;
    portMUX_TYPE threatMux = portMUX_INITIALIZER_UNLOCKED;
    volatile bool threatPending = false;
    ThreatEvent pendingThreat;
    portMUX_TYPE wifiMux = portMUX_INITIALIZER_UNLOCKED;
    volatile bool wifiFramePending = false;
    WiFiFrameEvent pendingWiFiFrame;
    bool statusMessageActive = false;
    uint32_t statusMessageUntilMs = 0;

    enum class DisplayState {
        Awake,
        PowerSaveMessage,
        ShowingQR,
        Off
    };

    enum class UIPage {
        MainUI,
        DetectionList
    };

    DisplayState displayState = DisplayState::Awake;
    UIPage currentPage = UIPage::MainUI;
    uint32_t displayStateMs = 0;
    uint8_t detectionListScrollPos = 0;

    // Button double-click detection
    const uint32_t DOUBLE_CLICK_WINDOW_MS = 400;
    uint32_t lastBtnAPress = 0;
    bool waitingForDoubleClick = false;

    void setDisplayOn() {
        M5.Display.wakeup();
        M5.Display.setBrightness(DISPLAY_BRIGHTNESS_ON);
    }

    void setDisplayOff() {
        M5.Display.setBrightness(0);
        M5.Display.sleep();
    }

    // New clean display layout with LARGE text
    void drawMainDisplay(uint8_t channel, uint8_t battery, uint32_t detections, bool animDot) {
        M5.Display.fillScreen(TFT_BLACK);

        int16_t yPos = 0;

        // ===== LINE 1: WiFi Status - Size 2 =====
        M5.Display.setTextSize(2);
        M5.Display.setCursor(4, yPos);
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Display.print("WiFi:");
        M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
        M5.Display.printf("CH%d", channel);

        // Scanning indicator
        M5.Display.setTextColor(animDot ? TFT_GREEN : TFT_DARKGREY, TFT_BLACK);
        M5.Display.print(" *");

        yPos += 20;

        // ===== LINE 2: GPS Status - Size 2 =====
        M5.Display.setCursor(4, yPos);
        int sats = gpsModule.getSatellites();
        bool hasFix = gpsModule.hasValidFix();

        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Display.print("GPS:");

        if (hasFix) {
            M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
            M5.Display.printf("FIX(%d)", sats);
        } else if (sats > 0) {
            M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
            M5.Display.printf("WAIT(%d)", sats);
        } else {
            M5.Display.setTextColor(TFT_RED, TFT_BLACK);
            M5.Display.print("NOSIG");
        }

        yPos += 20;

        // ===== LINE 3-4: GPS Coordinates or status =====
        M5.Display.setTextSize(2);
        if (hasFix) {
            // Latitude
            M5.Display.setCursor(4, yPos);
            M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
            M5.Display.printf("%.5f", gpsModule.getLatitude());
            yPos += 20;

            // Longitude
            M5.Display.setCursor(4, yPos);
            M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
            M5.Display.printf("%.5f", gpsModule.getLongitude());
            yPos += 20;
        } else {
            M5.Display.setCursor(4, yPos);
            M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
            M5.Display.print("No GPS fix");
            yPos += 40;
        }

        // Separator
        M5.Display.drawLine(0, yPos, M5.Display.width(), yPos, TFT_DARKGREY);
        yPos += 5;

        // ===== DETECTIONS & BATTERY - Size 2 =====
        M5.Display.setCursor(4, yPos);
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Display.print("DET:");
        M5.Display.setTextColor(detections > 0 ? TFT_RED : TFT_GREEN, TFT_BLACK);
        M5.Display.printf("%d", detections);

        // Battery on same line
        char batText[8];
        snprintf(batText, sizeof(batText), "B:%d%%", battery);
        int16_t batWidth = M5.Display.textWidth(batText);
        M5.Display.setCursor(M5.Display.width() - batWidth - 4, yPos);
        uint16_t batColor = (battery <= 20) ? TFT_RED : (battery <= 50) ? TFT_ORANGE : TFT_GREEN;
        M5.Display.setTextColor(batColor, TFT_BLACK);
        M5.Display.print(batText);

        yPos += 20;

        // ===== RSSI on bottom if available - Size 1 =====
        if (latestRssi > -90) {
            M5.Display.setTextSize(1);
            M5.Display.setCursor(4, yPos);
            M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
            M5.Display.printf("Signal: %d dBm", latestRssi);
        }
    }

    // Draw detection list page
    void drawDetectionList(uint8_t scrollPos) {
        M5.Display.fillScreen(TFT_BLACK);
        M5.Display.setTextSize(1);

        // Header
        M5.Display.setCursor(2, 2);
        M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
        M5.Display.printf("DETECTIONS (%d)", detectionHistoryCount);

        if (detectionHistoryCount == 0) {
            M5.Display.setCursor(4, 25);
            M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
            M5.Display.println("No stored");
            M5.Display.println("detections yet");
            M5.Display.println();
            M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
            M5.Display.println("C: Back to main");
            return;
        }

        // Ensure scroll position is valid
        if (scrollPos >= detectionHistoryCount) {
            scrollPos = 0;
        }

        // Display current detection
        StoredDetection& det = detectionHistory[scrollPos];
        int16_t yPos = 15;

        M5.Display.setTextSize(1);
        M5.Display.setCursor(2, yPos);
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Display.printf("#%d/%d", scrollPos + 1, detectionHistoryCount);
        yPos += 12;

        // Separator
        M5.Display.drawLine(0, yPos, M5.Display.width(), yPos, TFT_DARKGREY);
        yPos += 3;

        // Identifier
        M5.Display.setCursor(2, yPos);
        M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
        M5.Display.printf("ID: %s", det.identifier);
        yPos += 10;

        // Radio type and RSSI
        M5.Display.setCursor(2, yPos);
        M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
        M5.Display.printf("%s: %ddBm", det.radioType, det.rssi);
        yPos += 10;

        // GPS coordinates
        M5.Display.setCursor(2, yPos);
        M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
        M5.Display.printf("%.5f", det.latitude);
        yPos += 10;

        M5.Display.setCursor(2, yPos);
        M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
        M5.Display.printf("%.5f", det.longitude);
        yPos += 10;

        // Date/Time
        if (det.year > 0) {
            M5.Display.setCursor(2, yPos);
            M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
            M5.Display.printf("%04d-%02d-%02d", det.year, det.month, det.day);
            yPos += 10;

            M5.Display.setCursor(2, yPos);
            M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
            M5.Display.printf("%02d:%02d:%02d UTC", det.hour, det.minute, det.second);
            yPos += 10;
        }

        // Navigation help
        M5.Display.setCursor(2, M5.Display.height() - 10);
        M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
        M5.Display.print("A:Scroll C:Back");
    }

    void initScanningUi(uint8_t channel, uint32_t nowMs) {
        setDisplayOn();
        currentPage = UIPage::MainUI;
        drawMainDisplay(channel, M5.Power.getBatteryLevel(), detectionCount, true);
        displayState = DisplayState::Awake;
        displayStateMs = nowMs;
    }

    // Generate compact CSV format for QR (much smaller than GPX XML)
    String generateCompactGPX() {
        String data = "FS:";  // FlockSquawk header
        for (uint8_t i = 0; i < detectionHistoryCount; i++) {
            StoredDetection& det = detectionHistory[i];
            if (!det.valid) continue;
            if (i > 0) data += "|";
            // Format: lat,lon,YYYYMMDDHHMMSS,rssi,id
            char buf[80];
            snprintf(buf, sizeof(buf), "%.5f,%.5f,%04d%02d%02d%02d%02d%02d,%d,%s",
                det.latitude, det.longitude,
                det.year, det.month, det.day, det.hour, det.minute, det.second,
                det.rssi, det.identifier);
            data += buf;
        }
        return data;
    }

    void displayQRCode() {
        setDisplayOn();
        M5.Display.fillScreen(TFT_BLACK);
        M5.Display.setTextSize(1);

        // Header
        M5.Display.setCursor(4, 2);
        M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
        M5.Display.printf("GPX EXPORT (%d)", detectionHistoryCount);

        if (detectionHistoryCount == 0) {
            M5.Display.setCursor(4, 20);
            M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
            M5.Display.println("No detections");
            M5.Display.println("with GPS fix");
            M5.Display.println();
            M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
            M5.Display.println("Press to exit");
            return;
        }

        // Generate compact data (CSV format instead of verbose GPX XML)
        String compactData = generateCompactGPX();

        // Check if data fits in QR code (version 6 ECC_LOW ~136 bytes capacity)
        if (compactData.length() > 130) {
            M5.Display.setCursor(4, 20);
            M5.Display.setTextColor(TFT_RED, TFT_BLACK);
            M5.Display.println("Data too large");
            M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
            M5.Display.printf("Size: %d bytes\n", compactData.length());
            M5.Display.printf("Max: 130 bytes\n");
            M5.Display.println();
            M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
            M5.Display.println("Reduce detections");
            M5.Display.println("or export via");
            M5.Display.println("serial instead");
            M5.Display.println();
            M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
            M5.Display.println("Press to exit");
            return;
        }

        // Allocate QR buffer on heap to avoid stack overflow
        uint16_t bufferSize = qrcode_getBufferSize(6);
        uint8_t* qrcodeBytes = (uint8_t*)malloc(bufferSize);
        if (!qrcodeBytes) {
            M5.Display.setCursor(4, 20);
            M5.Display.setTextColor(TFT_RED, TFT_BLACK);
            M5.Display.println("Memory error");
            M5.Display.println();
            M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
            M5.Display.println("Press to exit");
            return;
        }

        // Create QR code with error checking
        QRCode qrcode;
        int8_t result = qrcode_initText(&qrcode, qrcodeBytes, 6, ECC_LOW, compactData.c_str());

        if (result < 0) {
            M5.Display.setCursor(4, 20);
            M5.Display.setTextColor(TFT_RED, TFT_BLACK);
            M5.Display.println("QR encode failed");
            M5.Display.println();
            M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
            M5.Display.println("Press to exit");
            free(qrcodeBytes);
            return;
        }

        // Draw QR code centered
        int qrScale = 2;  // Pixel size for each QR module
        int qrSize = qrcode.size * qrScale;
        int offsetX = (M5.Display.width() - qrSize) / 2;
        int offsetY = 20;  // Below header

        for (uint8_t y = 0; y < qrcode.size; y++) {
            for (uint8_t x = 0; x < qrcode.size; x++) {
                uint16_t color = qrcode_getModule(&qrcode, x, y) ? TFT_BLACK : TFT_WHITE;
                M5.Display.fillRect(offsetX + x * qrScale, offsetY + y * qrScale,
                    qrScale, qrScale, color);
            }
        }

        // Free heap memory
        free(qrcodeBytes);

        // Footer text
        M5.Display.setCursor(4, M5.Display.height() - 10);
        M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
        M5.Display.print("Scan to get data");
    }

    void drawCenteredMessage(const char* line1, const char* line2, uint16_t bgColor, uint16_t textColor) {
        M5.Display.fillScreen(bgColor);
        M5.Display.setTextColor(textColor, bgColor);
        M5.Display.setTextSize(2);
        int16_t lineHeight = M5.Display.fontHeight();
        int16_t totalHeight = (line2 && line2[0] != '\0') ? (lineHeight * 2) + 6 : lineHeight;
        int16_t startY = (M5.Display.height() - totalHeight) / 2;
        if (startY < 0) startY = 0;
        M5.Display.setCursor(0, startY);
        M5.Display.println(line1);
        if (line2 && line2[0] != '\0') {
            M5.Display.println(line2);
        }
    }

    void drawAlertText(bool visible) {
        const char* text = "Flock Detected";
        M5.Display.setTextSize(2);
        int16_t textWidth = M5.Display.textWidth(text);
        int16_t textHeight = M5.Display.fontHeight();
        int16_t x = (M5.Display.width() - textWidth) / 2;
        int16_t y = (M5.Display.height() - textHeight) / 2;
        if (x < 0) x = 0;
        if (visible) {
            M5.Display.setTextColor(TFT_BLACK, TFT_RED);
            M5.Display.setCursor(x, y);
            M5.Display.print(text);
        } else {
            M5.Display.fillRect(x, y, textWidth, textHeight, TFT_RED);
        }
    }

    void setAlertLed(bool on) {
        M5.Power.setLed(on);
    }

    void startAlert(uint32_t nowMs) {
        setDisplayOn();
        detectionCount++;
        alertActive = true;
        alertVisible = false;
        alertStartMs = nowMs;
        alertLastFlashMs = 0;
        alertUntilMs = nowMs + ALERT_DURATION_MS;
        M5.Display.fillScreen(TFT_RED);
        drawAlertText(true);
    }

    bool updateAlert(uint32_t nowMs) {
        if (!alertActive) return false;
        if (nowMs >= alertUntilMs) {
            alertActive = false;
            setAlertLed(false);
            return false;
        }

        if (nowMs - alertLastFlashMs >= ALERT_FLASH_MS) {
            alertVisible = !alertVisible;
            if (alertVisible) {
                drawAlertText(true);
                M5.Speaker.tone(ALERT_BEEP_FREQ, ALERT_BEEP_MS);
            } else {
                drawAlertText(false);
            }
            setAlertLed(alertVisible);
            alertLastFlashMs = nowMs;
        }

        return true;
    }

    void triggerAlert(uint32_t nowMs) {
        if (!alertActive) {
            startAlert(nowMs);
            return;
        }
        detectionCount++;
        alertUntilMs = nowMs + ALERT_DURATION_MS;
    }

    void showStatusMessage(const char* line1, const char* line2) {
        setDisplayOn();
        drawCenteredMessage(line1, line2, TFT_BLACK, TFT_WHITE);
        statusMessageActive = true;
        statusMessageUntilMs = millis() + STATUS_MSG_MS;
    }

    void showPowerSaveMessage() {
        setDisplayOn();
        drawCenteredMessage("Entering power", "saving mode", TFT_BLACK, TFT_WHITE);
        displayState = DisplayState::PowerSaveMessage;
        displayStateMs = millis();
    }
}

void EventBus::publishWifiFrame(const WiFiFrameEvent& event) {
    if (wifiHandler) wifiHandler(event);
}

void EventBus::publishBluetoothDevice(const BluetoothDeviceEvent& event) {
    if (bluetoothHandler) bluetoothHandler(event);
}

void EventBus::publishThreat(const ThreatEvent& event) {
    if (threatHandler) threatHandler(event);
}

void EventBus::publishSystemReady() {
    if (systemReadyHandler) systemReadyHandler();
}

void EventBus::subscribeWifiFrame(WiFiFrameHandler handler) {
    wifiHandler = handler;
}

void EventBus::subscribeBluetoothDevice(BluetoothHandler handler) {
    bluetoothHandler = handler;
}

void EventBus::subscribeThreat(ThreatHandler handler) {
    threatHandler = handler;
}

void EventBus::subscribeSystemReady(SystemEventHandler handler) {
    systemReadyHandler = handler;
}

// RadioScannerManager implementation
void RadioScannerManager::initialize() {
    configureWiFiSniffer();
    configureBluetoothScanner();
}

void RadioScannerManager::configureWiFiSniffer() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(wifiPacketHandler);
    esp_wifi_set_channel(currentWifiChannel, WIFI_SECOND_CHAN_NONE);
    
    Serial.println("[RF] WiFi sniffer activated");
}

void RadioScannerManager::configureBluetoothScanner() {
    NimBLEDevice::init("");
    bleScanner = NimBLEDevice::getScan();
    bleScanner->setActiveScan(true);
    bleScanner->setInterval(100);
    bleScanner->setWindow(99);
    
    class BLEDeviceObserver : public NimBLEScanCallbacks {
        void onResult(const NimBLEAdvertisedDevice* device) override {
            BluetoothDeviceEvent event;
            memset(&event, 0, sizeof(event));
            
            NimBLEAddress addr = device->getAddress();
            std::string addrStr = addr.toString();
            sscanf(addrStr.c_str(), "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
                   &event.mac[0], &event.mac[1], &event.mac[2],
                   &event.mac[3], &event.mac[4], &event.mac[5]);
            
            event.rssi = device->getRSSI();
            
            if (device->haveName()) {
                strncpy(event.name, device->getName().c_str(), sizeof(event.name) - 1);
            }
            
            event.hasServiceUUID = device->haveServiceUUID();
            if (event.hasServiceUUID && device->getServiceUUIDCount() > 0) {
                NimBLEUUID uuid = device->getServiceUUID(0);
                strncpy(event.serviceUUID, uuid.toString().c_str(), sizeof(event.serviceUUID) - 1);
            }
            
            EventBus::publishBluetoothDevice(event);
        }
        
        void onScanEnd(const NimBLEScanResults& results, int reason) override {
            // Scan completed
        }
    };
    
    bleScanner->setScanCallbacks(new BLEDeviceObserver());
    Serial.println("[RF] Bluetooth scanner initialized");
}

void RadioScannerManager::update() {
    switchWifiChannel();
    performBLEScan();
}

uint8_t RadioScannerManager::getCurrentWifiChannel() {
    return currentWifiChannel;
}

void RadioScannerManager::switchWifiChannel() {
    unsigned long now = millis();
    if (now - lastChannelSwitch >= CHANNEL_SWITCH_MS) {
        currentWifiChannel++;
        if (currentWifiChannel > MAX_WIFI_CHANNEL) {
            currentWifiChannel = 1;
        }
        esp_wifi_set_channel(currentWifiChannel, WIFI_SECOND_CHAN_NONE);
        lastChannelSwitch = now;
    }
}

void RadioScannerManager::performBLEScan() {
    unsigned long now = millis();
    if (now - lastBLEScan >= BLE_SCAN_INTERVAL_MS && !isScanningBLE) {
        if (bleScanner && !bleScanner->isScanning()) {
            bleScanner->start(BLE_SCAN_SECONDS, false);
            isScanningBLE = true;
            lastBLEScan = now;
        }
    }
    
    if (isScanningBLE && bleScanner && !bleScanner->isScanning()) {
        if (now - lastBLEScan > BLE_SCAN_SECONDS * 1000) {
            bleScanner->clearResults();
            isScanningBLE = false;
        }
    }
}

struct WiFi80211Header {
    uint16_t frameControl;
    uint16_t duration;
    uint8_t destination[6];
    uint8_t source[6];
    uint8_t bssid[6];
    uint16_t sequence;
};

void RadioScannerManager::wifiPacketHandler(void* buffer, wifi_promiscuous_pkt_type_t type) {
    const wifi_promiscuous_pkt_t* packet = (wifi_promiscuous_pkt_t*)buffer;
    const uint8_t* rawData = packet->payload;
    
    if (packet->rx_ctrl.sig_len < 24) return;
    
    const WiFi80211Header* header = (const WiFi80211Header*)rawData;
    uint8_t frameSubtype = (header->frameControl & 0x00F0) >> 4;
    
    bool isProbeRequest = (frameSubtype == 0x04);
    bool isBeacon = (frameSubtype == 0x08);
    
    if (!isProbeRequest && !isBeacon) return;
    
    WiFiFrameEvent event;
    memset(&event, 0, sizeof(event));
    
    memcpy(event.mac, header->source, 6);
    event.rssi = packet->rx_ctrl.rssi;
    event.frameSubtype = frameSubtype;
    event.channel = RadioScannerManager::currentWifiChannel;
    
    const uint8_t* payload = rawData + sizeof(WiFi80211Header);
    
    if (isBeacon) {
        payload += 12;
    }
    
    if (packet->rx_ctrl.sig_len > (payload - rawData) + 2) {
        if (payload[0] == 0 && payload[1] <= 32) {
            size_t ssidLen = payload[1];
            memcpy(event.ssid, payload + 2, ssidLen);
            event.ssid[ssidLen] = '\0';
        }
    }
    
    EventBus::publishWifiFrame(event);
}

uint8_t RadioScannerManager::currentWifiChannel = 1;
unsigned long RadioScannerManager::lastChannelSwitch = 0;
unsigned long RadioScannerManager::lastBLEScan = 0;
NimBLEScan* RadioScannerManager::bleScanner = nullptr;
bool RadioScannerManager::isScanningBLE = false;

// ThreatAnalyzer implementation
void ThreatAnalyzer::initialize() {
    // Analyzer ready
}

void ThreatAnalyzer::analyzeWiFiFrame(const WiFiFrameEvent& frame) {
    bool nameMatch = strlen(frame.ssid) > 0 && matchesNetworkName(frame.ssid);
    bool macMatch = matchesMACPrefix(frame.mac);
    
    if (nameMatch || macMatch) {
        uint8_t certainty = calculateCertainty(nameMatch, macMatch, false);
        emitThreatDetection(frame, "wifi", certainty);
    }
}

void ThreatAnalyzer::analyzeBluetoothDevice(const BluetoothDeviceEvent& device) {
    bool nameMatch = strlen(device.name) > 0 && matchesBLEName(device.name);
    bool macMatch = matchesMACPrefix(device.mac);
    bool uuidMatch = device.hasServiceUUID && matchesRavenService(device.serviceUUID);
    
    if (nameMatch || macMatch || uuidMatch) {
        uint8_t certainty = calculateCertainty(nameMatch, macMatch, uuidMatch);
        const char* category = determineCategory(uuidMatch);
        emitThreatDetection(device, "bluetooth", certainty, category);
    }
}

bool ThreatAnalyzer::matchesNetworkName(const char* ssid) {
    if (!ssid) return false;
    
    for (size_t i = 0; i < DeviceProfiles::NetworkNameCount; i++) {
        if (strcasestr(ssid, DeviceProfiles::NetworkNames[i])) {
            return true;
        }
    }
    return false;
}

bool ThreatAnalyzer::matchesMACPrefix(const uint8_t* mac) {
    char macStr[9];
    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x", mac[0], mac[1], mac[2]);
    
    for (size_t i = 0; i < DeviceProfiles::MACPrefixCount; i++) {
        if (strncasecmp(macStr, DeviceProfiles::MACPrefixes[i], 8) == 0) {
            return true;
        }
    }
    return false;
}

bool ThreatAnalyzer::matchesBLEName(const char* name) {
    if (!name) return false;
    
    for (size_t i = 0; i < DeviceProfiles::BLEIdentifierCount; i++) {
        if (strcasestr(name, DeviceProfiles::BLEIdentifiers[i])) {
            return true;
        }
    }
    return false;
}

bool ThreatAnalyzer::matchesRavenService(const char* uuid) {
    if (!uuid) return false;
    
    for (size_t i = 0; i < DeviceProfiles::RavenServiceCount; i++) {
        if (strcasecmp(uuid, DeviceProfiles::RavenServices[i]) == 0) {
            return true;
        }
    }
    return false;
}

uint8_t ThreatAnalyzer::calculateCertainty(bool nameMatch, bool macMatch, bool uuidMatch) {
    if (nameMatch && macMatch && uuidMatch) return 100;
    if (nameMatch && macMatch) return 95;
    if (uuidMatch) return 90;
    if (nameMatch || macMatch) return 85;
    return 70;
}

const char* ThreatAnalyzer::determineCategory(bool isRaven) {
    return isRaven ? "acoustic_detector" : "surveillance_device";
}

void ThreatAnalyzer::emitThreatDetection(const WiFiFrameEvent& frame, const char* radio, uint8_t certainty) {
    ThreatEvent threat;
    memset(&threat, 0, sizeof(threat));
    memcpy(threat.mac, frame.mac, 6);
    strncpy(threat.identifier, frame.ssid, sizeof(threat.identifier) - 1);
    threat.rssi = frame.rssi;
    threat.channel = frame.channel;
    threat.radioType = radio;
    threat.certainty = certainty;
    threat.category = "surveillance_device";
    
    EventBus::publishThreat(threat);
}

void ThreatAnalyzer::emitThreatDetection(const BluetoothDeviceEvent& device, const char* radio, uint8_t certainty, const char* category) {
    ThreatEvent threat;
    memset(&threat, 0, sizeof(threat));
    memcpy(threat.mac, device.mac, 6);
    strncpy(threat.identifier, device.name, sizeof(threat.identifier) - 1);
    threat.rssi = device.rssi;
    threat.channel = 0;
    threat.radioType = radio;
    threat.certainty = certainty;
    threat.category = category;
    
    EventBus::publishThreat(threat);
}

// TelemetryReporter implementation
void TelemetryReporter::initialize() {
    bootTime = millis();
}

void TelemetryReporter::setGPSHandler(GPSHandler* handler) {
    gpsHandler = handler;
}

void TelemetryReporter::handleThreatDetection(const ThreatEvent& threat) {
    DynamicJsonDocument doc(3072);  // Increased size for GPS data

    doc["event"] = "target_detected";
    doc["ms_since_boot"] = millis() - bootTime;

    appendSourceInfo(threat, doc);
    appendTargetIdentity(threat, doc);
    appendIndicators(threat, doc);
    appendMetadata(threat, doc);
    appendGPSLocation(doc);

    outputJSON(doc);
}

void TelemetryReporter::appendSourceInfo(const ThreatEvent& threat, JsonDocument& doc) {
    JsonObject source = doc.createNestedObject("source");
    source["radio"] = threat.radioType;
    source["channel"] = threat.channel;
    source["rssi"] = threat.rssi;
}

void TelemetryReporter::appendTargetIdentity(const ThreatEvent& threat, JsonDocument& doc) {
    JsonObject target = doc.createNestedObject("target");
    JsonObject identity = target.createNestedObject("identity");
    
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
             threat.mac[0], threat.mac[1], threat.mac[2],
             threat.mac[3], threat.mac[4], threat.mac[5]);
    identity["mac"] = macStr;
    
    char oui[9];
    snprintf(oui, sizeof(oui), "%02x:%02x:%02x", threat.mac[0], threat.mac[1], threat.mac[2]);
    identity["oui"] = oui;
    
    identity["label"] = threat.identifier;
}

void TelemetryReporter::appendIndicators(const ThreatEvent& threat, JsonDocument& doc) {
    JsonObject indicators = doc["target"].createNestedObject("indicators");
    
    bool hasName = strlen(threat.identifier) > 0;
    indicators["ssid_match"] = (hasName && strcmp(threat.radioType, "wifi") == 0);
    indicators["mac_match"] = true;
    indicators["name_match"] = (hasName && strcmp(threat.radioType, "bluetooth") == 0);
    indicators["service_uuid_match"] = (strcmp(threat.category, "acoustic_detector") == 0);
}

void TelemetryReporter::appendMetadata(const ThreatEvent& threat, JsonDocument& doc) {
    JsonObject metadata = doc.createNestedObject("metadata");

    if (strcmp(threat.radioType, "wifi") == 0) {
        metadata["frame_type"] = "beacon";
    } else {
        metadata["frame_type"] = "advertisement";
    }

    metadata["detection_method"] = "combined_signature";
}

void TelemetryReporter::appendGPSLocation(JsonDocument& doc) {
    if (!gpsHandler) {
        return;  // No GPS handler configured
    }

    JsonObject location = doc.createNestedObject("location");

    if (gpsHandler->hasValidFix()) {
        location["latitude"] = gpsHandler->getLatitude();
        location["longitude"] = gpsHandler->getLongitude();
        location["altitude_m"] = gpsHandler->getAltitude();
        location["satellites"] = gpsHandler->getSatellites();
        location["hdop"] = gpsHandler->getHdop();
        location["fix_age_ms"] = gpsHandler->getLocationAge();
        location["fix_valid"] = true;

        // Add timestamp if available
        int year, month, day, hour, minute, second;
        gpsHandler->getDateTime(year, month, day, hour, minute, second);
        if (year > 0) {
            char timestamp[32];
            snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                year, month, day, hour, minute, second);
            location["timestamp"] = timestamp;
        }
    } else {
        location["fix_valid"] = false;
        location["satellites"] = gpsHandler->getSatellites();
        location["note"] = "Waiting for GPS fix";
    }
}

void TelemetryReporter::outputJSON(const JsonDocument& doc) {
    serializeJson(doc, Serial);
    Serial.println();
}

// Main system initialization
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n\n========================================");
    Serial.println("FlockSquawk M5StickC Plus2 with GPS");
    Serial.println("========================================\n");

    Serial.println("[INIT] Starting M5.begin()...");
    M5.begin();
    Serial.println("[INIT] M5.begin() complete");

    M5.Display.setRotation(1);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.clear(TFT_BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.println("Flock Detector");
    M5.Display.setTextSize(1);
    M5.Display.println("with GPS tracking");
    M5.Display.setTextSize(2);
    M5.Display.println("");
    M5.Display.println("Starting...");

    Serial.println("[INIT] Playing startup beeps...");
    playBeepPattern(STARTUP_BEEP_FREQ, BEEP_DURATION_MS, 3);
    Serial.println("[INIT] Beeps complete");

    Serial.println("[INIT] Loading saved detections from flash...");
    loadDetectionsFromFlash();

    Serial.println("\nInitializing Threat Detection System...");
    Serial.println();
    
    EventBus::subscribeWifiFrame([](const WiFiFrameEvent& event) {
        portENTER_CRITICAL(&wifiMux);
        pendingWiFiFrame = event;
        wifiFramePending = true;
        portEXIT_CRITICAL(&wifiMux);
    });
    
    EventBus::subscribeBluetoothDevice([](const BluetoothDeviceEvent& event) {
        threatEngine.analyzeBluetoothDevice(event);
    });
    
    EventBus::subscribeThreat([](const ThreatEvent& event) {
        portENTER_CRITICAL(&threatMux);
        pendingThreat = event;
        threatPending = true;
        portEXIT_CRITICAL(&threatMux);
    });
    
    Serial.println("[INIT] Initializing threat engine...");
    threatEngine.initialize();

    Serial.println("[INIT] Initializing telemetry reporter...");
    reporter.initialize();

    Serial.println("[INIT] Initializing GPS module...");
    M5.Display.setTextSize(1);
    M5.Display.println("Init GPS (G26/G25)");
    gpsModule.initialize();
    Serial.println("[INIT] GPS module initialized");
    M5.Display.println("GPS ready");
    delay(500);

    Serial.println("[INIT] Linking GPS to reporter...");
    reporter.setGPSHandler(&gpsModule);

    Serial.println("[INIT] Initializing RF scanner...");
    rfScanner.initialize();
    Serial.println("[INIT] RF scanner initialized");

    Serial.println("\n========================================");
    Serial.println("System operational - scanning for targets");
    Serial.println("========================================\n");
    
    initScanningUi(RadioScannerManager::getCurrentWifiChannel(), millis());
    EventBus::publishSystemReady();
}

void loop() {
    M5.update();
    rfScanner.update();
    gpsModule.update();

    static uint8_t lastChannel = 0;
    static uint32_t lastDisplayUpdateMs = 0;
    static bool animDot = false;
    static bool wasAlertActive = false;
    static bool powerToggleHandled = false;
    static bool lastShouldPowerSave = false;

    uint8_t channel = RadioScannerManager::getCurrentWifiChannel();
    uint32_t now = millis();
    bool shouldPowerSave = powerSaverEnabled;

    // Button C (BtnB) - Toggle between main UI and detection list (short press only)
    static bool btnBWasLongPress = false;
    if (M5.BtnB.wasReleased() && !btnBWasLongPress) {
        if (displayState == DisplayState::ShowingQR) {
            // Exit QR code view
            initScanningUi(channel, now);
        } else if (currentPage == UIPage::MainUI) {
            // Switch to detection list
            currentPage = UIPage::DetectionList;
            detectionListScrollPos = 0;
            setDisplayOn();
            drawDetectionList(detectionListScrollPos);
            displayState = DisplayState::Awake;
            displayStateMs = now;
        } else if (currentPage == UIPage::DetectionList) {
            // Switch back to main UI
            initScanningUi(channel, now);
        }
    }

    // Button A - Different behavior depending on current page
    if (M5.BtnA.wasPressed()) {
        if (displayState == DisplayState::ShowingQR) {
            // Exit QR code view
            initScanningUi(channel, now);
        } else if (currentPage == UIPage::DetectionList) {
            // Scroll through detection list
            if (detectionHistoryCount > 0) {
                detectionListScrollPos = (detectionListScrollPos + 1) % detectionHistoryCount;
                setDisplayOn();
                drawDetectionList(detectionListScrollPos);
                displayState = DisplayState::Awake;
                displayStateMs = now;
            }
        } else if (currentPage == UIPage::MainUI) {
            // Main UI: Double-click detection for QR code
            if (waitingForDoubleClick && (now - lastBtnAPress) < DOUBLE_CLICK_WINDOW_MS) {
                // Double-click detected - show QR code
                waitingForDoubleClick = false;
                displayState = DisplayState::ShowingQR;
                displayQRCode();
            } else {
                // First click - wait for double-click
                waitingForDoubleClick = true;
                lastBtnAPress = now;
            }
        }
    }

    // Reset double-click waiting after timeout
    if (waitingForDoubleClick && (millis() - lastBtnAPress) > DOUBLE_CLICK_WINDOW_MS) {
        waitingForDoubleClick = false;
    }

    if (wifiFramePending) {
        WiFiFrameEvent frameCopy;
        portENTER_CRITICAL(&wifiMux);
        frameCopy = pendingWiFiFrame;
        wifiFramePending = false;
        portEXIT_CRITICAL(&wifiMux);
        latestRssi = frameCopy.rssi;
        threatEngine.analyzeWiFiFrame(frameCopy);
    }

    if (threatPending) {
        ThreatEvent threatCopy;
        portENTER_CRITICAL(&threatMux);
        threatCopy = pendingThreat;
        threatPending = false;
        portEXIT_CRITICAL(&threatMux);
        reporter.handleThreatDetection(threatCopy);
        storeDetection(threatCopy);  // Store for GPX export
        triggerAlert(now);
    }

    // Long press Button B for power saver toggle
    if (M5.BtnB.pressedFor(2000) && !powerToggleHandled) {
        powerSaverEnabled = !powerSaverEnabled;
        powerToggleHandled = true;
        btnBWasLongPress = true;
        showStatusMessage("Power saver", powerSaverEnabled ? "ON" : "OFF");
        shouldPowerSave = powerSaverEnabled;
        lastShouldPowerSave = shouldPowerSave;
    }

    if (M5.BtnB.wasReleased()) {
        powerToggleHandled = false;
        // Reset long press flag after a short delay
        if (btnBWasLongPress) {
            delay(200);
            btnBWasLongPress = false;
        }
    }

    if (M5.BtnA.wasPressed() && shouldPowerSave) {
        initScanningUi(channel, now);
        lastChannel = channel;
        lastDisplayUpdateMs = now;
    }

    bool isAlerting = updateAlert(now);
    if (isAlerting) {
        wasAlertActive = true;
        return;
    }
    if (wasAlertActive && !isAlerting) {
        if (shouldPowerSave) {
            setDisplayOff();
            displayState = DisplayState::Off;
        } else {
            initScanningUi(channel, now);
            lastChannel = channel;
            lastDisplayUpdateMs = now;
        }
    }
    wasAlertActive = isAlerting;

    if (statusMessageActive && now >= statusMessageUntilMs) {
        statusMessageActive = false;
        if (shouldPowerSave) {
            initScanningUi(channel, now);
        } else {
            initScanningUi(channel, now);
        }
    }

    if (!isAlerting && !statusMessageActive && shouldPowerSave != lastShouldPowerSave) {
        lastShouldPowerSave = shouldPowerSave;
        if (shouldPowerSave) {
            initScanningUi(channel, now);
        } else {
            initScanningUi(channel, now);
        }
    }

    if (!isAlerting && !statusMessageActive) {
        if (shouldPowerSave) {
            if (displayState == DisplayState::Awake && now - displayStateMs >= SCREEN_ON_MS) {
                showPowerSaveMessage();
            } else if (displayState == DisplayState::PowerSaveMessage && now - displayStateMs >= POWER_SAVE_MSG_MS) {
                setDisplayOff();
                displayState = DisplayState::Off;
            }
        } else if (displayState != DisplayState::Awake) {
            initScanningUi(channel, now);
        }
    }

    // Update display periodically (but not in QR code mode)
    if (!isAlerting && !statusMessageActive && displayState == DisplayState::Awake) {
        if (currentPage == UIPage::MainUI) {
            // Update main UI display
            if (now - lastDisplayUpdateMs >= DOT_UPDATE_MS || channel != lastChannel) {
                animDot = !animDot;
                drawMainDisplay(channel, M5.Power.getBatteryLevel(), detectionCount, animDot);
                lastChannel = channel;
                lastDisplayUpdateMs = now;
            }
        } else if (currentPage == UIPage::DetectionList) {
            // Detection list page - refresh less frequently
            if (now - lastDisplayUpdateMs >= 1000) {
                drawDetectionList(detectionListScrollPos);
                lastDisplayUpdateMs = now;
            }
        }
    }

    // Don't update display if showing QR code
    if (displayState == DisplayState::ShowingQR) {
        delay(30);
        return;
    }

    delay(30);
}
