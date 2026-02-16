#ifndef TELEMETRY_REPORTER_H
#define TELEMETRY_REPORTER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "EventBus.h"

// Forward declaration to avoid circular dependency
class GPSHandler;

class TelemetryReporter {
public:
    void initialize();
    void handleThreatDetection(const ThreatEvent& threat);
    void setGPSHandler(GPSHandler* handler);

private:
    unsigned long bootTime;
    GPSHandler* gpsHandler = nullptr;

    void serializeThreatToJSON(const ThreatEvent& threat, JsonDocument& doc);
    void appendSourceInfo(const ThreatEvent& threat, JsonDocument& doc);
    void appendTargetIdentity(const ThreatEvent& threat, JsonDocument& doc);
    void appendIndicators(const ThreatEvent& threat, JsonDocument& doc);
    void appendMetadata(const ThreatEvent& threat, JsonDocument& doc);
    void appendGPSLocation(JsonDocument& doc);
    void outputJSON(const JsonDocument& doc);
};

#endif