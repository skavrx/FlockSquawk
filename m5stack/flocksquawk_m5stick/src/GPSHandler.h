#ifndef GPS_HANDLER_H
#define GPS_HANDLER_H

#include <Arduino.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

/**
 * GPSHandler - Manages BN-220 GPS module via UART
 *
 * Parses NMEA sentences and provides location data.
 * For M5StickC Plus2, use GPIO 33 (RX) and GPIO 32 (TX)
 * connected to BN-220 TX and RX respectively.
 */
class GPSHandler {
public:
    static const uint32_t GPS_BAUD_RATE = 9600;
    static const uint8_t GPS_RX_PIN = 26;  // Connect to BN-220 TX (G26)
    static const uint8_t GPS_TX_PIN = 25;  // Connect to BN-220 RX (G25, optional)
    static const uint16_t GPS_UPDATE_MS = 1000;

    void initialize() {
        // Initialize Serial2 for GPS communication
        gpsSerial.begin(GPS_BAUD_RATE, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
        Serial.println("[GPS] GPS module initialized on Serial2");
        Serial.printf("[GPS] RX pin: %d, TX pin: %d, Baud: %d\n", GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD_RATE);
        lastUpdate = millis();
    }

    void update() {
        // Feed GPS data to TinyGPSPlus parser
        while (gpsSerial.available() > 0) {
            char c = gpsSerial.read();
            gps.encode(c);
        }

        // Periodic status updates
        unsigned long now = millis();
        if (now - lastUpdate >= GPS_UPDATE_MS) {
            if (gps.location.isUpdated()) {
                Serial.printf("[GPS] Location updated: %.6f, %.6f (sats: %d)\n",
                    gps.location.lat(), gps.location.lng(), gps.satellites.value());
            }
            lastUpdate = now;
        }
    }

    bool hasValidFix() {
        return gps.location.isValid() && gps.location.age() < 2000;
    }

    double getLatitude() {
        return gps.location.lat();
    }

    double getLongitude() {
        return gps.location.lng();
    }

    double getAltitude() {
        return gps.altitude.meters();
    }

    int getSatellites() {
        return gps.satellites.value();
    }

    double getHdop() {
        return gps.hdop.hdop();
    }

    uint32_t getLocationAge() {
        return gps.location.age();
    }

    void getDateTime(int& year, int& month, int& day, int& hour, int& minute, int& second) {
        if (gps.date.isValid() && gps.time.isValid()) {
            year = gps.date.year();
            month = gps.date.month();
            day = gps.date.day();
            hour = gps.time.hour();
            minute = gps.time.minute();
            second = gps.time.second();
        } else {
            year = month = day = hour = minute = second = 0;
        }
    }

    uint32_t getCharsProcessed() const {
        return gps.charsProcessed();
    }

    uint32_t getSentencesWithFix() const {
        return gps.sentencesWithFix();
    }

    uint32_t getFailedChecksum() const {
        return gps.failedChecksum();
    }

private:
    HardwareSerial gpsSerial{2};  // Use Serial2
    TinyGPSPlus gps;
    unsigned long lastUpdate = 0;
};

#endif
