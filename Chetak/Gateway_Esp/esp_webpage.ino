#include <WiFi.h>
#include <WebServer.h>

// --- CONFIGURATION ---
const char* ssid = "YOUR_WIFI_NAME";      // <--- CHANGE THIS
const char* password = "YOUR_WIFI_PASSWORD"; // <--- CHANGE THIS

// UART Pins for ESP32 (Connecting to Pico)
#define RXD2 16
#define TXD2 17

WebServer server(80);
String latest_data = "Waiting for data...";
String security_status = "SECURE";

void handleRoot() {
  String html = "<html><head><meta http-equiv='refresh' content='2'></head>"
                "<body style='font-family: sans-serif; text-align: center; background: #f4f4f4;'>"
                "<h1>🔐 Secure IoT Dashboard</h1>"
                "<div style='background: white; padding: 20px; margin: 20px auto; width: 50%; border-radius: 10px; box-shadow: 0 0 10px rgba(0,0,0,0.1);'>"
                "<h2>Encrypted Sensor Data</h2>"
                "<p style='font-size: 24px; color: #333; font-family: monospace;'>" + latest_data + "</p>"
                "</div>"
                "<div style='margin-top: 20px; padding: 10px; color: white; background: " + (security_status == "SECURE" ? "green" : "red") + ";'>"
                "<h3>Status: " + security_status + "</h3>"
                "</div></body></html>";
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);      // Debug USB
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2); // UART to Pico

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected. IP: " + WiFi.localIP().toString());

  server.on("/", handleRoot);
  server.begin();
}

void loop() {
  server.handleClient();

  // Listen for data from RISC-V
  if (Serial2.available()) {
    String line = Serial2.readStringUntil('\n');
    line.trim();

    // Parse the output from main.c
    if (line.startsWith("[SANDBOX] Received Encrypted Blob:")) {
      latest_data = line.substring(34); // Extract just the Hex
      security_status = "SECURE";
    }
    else if (line.indexOf("SECURITY ALERT") >= 0) {
      security_status = "⚠️ ATTACK DETECTED! SYSTEM LOCKED ⚠️";
    }
  }
}