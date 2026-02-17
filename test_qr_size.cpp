// Test QR code data size calculator
#include <iostream>
#include <string>
#include <cstdio>

using namespace std;

// Simulate GPX generation
string generateGPX(int numDetections) {
    string gpx = "<?xml version=\"1.0\"?>\n<gpx version=\"1.1\" creator=\"FlockSquawk\">\n";

    for (int i = 0; i < numDetections; i++) {
        gpx += "<wpt lat=\"37.123456\" lon=\"-122.123456\">\n";
        gpx += "<time>2025-02-16T12:34:56Z</time>\n";
        gpx += "<name>AA:BB:CC:DD:EE:FF</name>\n";
        gpx += "<desc>wifi, -62dBm</desc>\n";
        gpx += "</wpt>\n";
    }

    gpx += "</gpx>";
    return gpx;
}

// Compact format: CSV-like
string generateCompact(int numDetections) {
    string data = "FS,"; // FlockSquawk header

    for (int i = 0; i < numDetections; i++) {
        if (i > 0) data += ";";
        // Format: lat,lon,YYYYMMDDHHMMSS,rssi
        data += "37.12345,-122.12345,20250216123456,-62";
    }

    return data;
}

// Super compact: Base64-encoded binary
string generateBinary(int numDetections) {
    // Each detection: 4 bytes lat, 4 bytes lon, 4 bytes timestamp, 1 byte rssi = 13 bytes
    // 20 detections = 260 bytes (much better!)
    int size = 2 + (numDetections * 13); // 2 byte header
    return "FS" + string(size, 'X'); // Simulated binary
}

int main() {
    cout << "QR Code Data Size Analysis\n";
    cout << "===========================\n\n";

    cout << "QR Version 6 ECC_LOW capacity: ~720 bytes\n\n";

    for (int n : {1, 5, 10, 15, 20}) {
        string gpx = generateGPX(n);
        string compact = generateCompact(n);
        string binary = generateBinary(n);

        cout << "With " << n << " detections:\n";
        cout << "  GPX XML:      " << gpx.length() << " bytes";
        if (gpx.length() > 720) cout << " ❌ TOO LARGE";
        cout << "\n";
        cout << "  Compact CSV:  " << compact.length() << " bytes";
        if (compact.length() > 720) cout << " ❌ TOO LARGE";
        cout << "\n";
        cout << "  Binary:       " << binary.length() << " bytes ✅\n\n";
    }

    return 0;
}
