#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// ============================================
// CONFIGURATION
// ============================================
const char* WIFI_SSID = "Liku";
const char* WIFI_PASSWORD = "9337028208";
const uint16_t WEB_SERVER_PORT = 80;

// MUST match the Pico's baud rate (9600)
const uint32_t SERIAL_BAUD_RATE = 9600; 

// ============================================
// GLOBAL OBJECTS
// ============================================
ESP8266WebServer server(WEB_SERVER_PORT);

// ============================================
// STATE VARIABLES
// ============================================
String latestData = "Waiting for input...";
String securityStatus = "SYSTEM SECURE";
String submittedData = "No commands sent yet.";
String statusColor = "#4CAF50"; // Start Green
String statusIcon = "🔒";

// ============================================
// HTML GENERATION
// ============================================
String generateWebPage() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta http-equiv="refresh" content="2">
    <title>RISC-V Security Monitor</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #1e3c72 0%, #2a5298 100%);
            min-height: 100vh; padding: 20px; color: #333;
        }
        .container { max-width: 1000px; margin: 0 auto; }
        
        header { text-align: center; color: white; margin-bottom: 30px; }
        header h1 { font-size: 2.2em; text-shadow: 0 2px 4px rgba(0,0,0,0.3); margin-bottom: 5px; }
        header p { opacity: 0.8; font-size: 1.1em; }

        .dashboard-grid {
            display: grid; grid-template-columns: 1fr; gap: 20px;
        }
        
        .card {
            background: white; border-radius: 12px; padding: 25px;
            box-shadow: 0 8px 20px rgba(0,0,0,0.2);
        }

        .status-card {
            background: )rawliteral" + statusColor + R"rawliteral(;
            color: white; text-align: center;
            transition: background 0.5s ease;
        }
        .status-display { font-size: 2.5em; font-weight: bold; margin: 10px 0; }
        .status-sub { font-size: 1.2em; opacity: 0.9; }

        .data-box {
            background: #f1f3f4; padding: 15px; border-radius: 8px;
            font-family: 'Courier New', monospace; font-weight: bold;
            color: #333; border-left: 5px solid #2a5298;
            margin-top: 10px; word-break: break-all;
        }

        /* Form Styles */
        form { display: flex; gap: 10px; margin-top: 15px; }
        input[type="text"] {
            flex-grow: 1; padding: 12px; border: 2px solid #ddd;
            border-radius: 6px; font-size: 16px;
        }
        button {
            background: #2a5298; color: white; border: none;
            padding: 12px 25px; border-radius: 6px; cursor: pointer;
            font-weight: bold; font-size: 16px;
        }
        button:hover { background: #1e3c72; }
        
        .helper-text { font-size: 0.9em; color: #666; margin-top: 5px; }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>🛡️ RISC-V Dual-Core Monitor</h1>
            <p>Hardware-Enforced Sandbox Protection</p>
        </header>
        
        <!-- SECURITY STATUS (Top Priority) -->
        <div class="card status-card">
            <div style="font-size: 40px;">)rawliteral" + statusIcon + R"rawliteral(</div>
            <div class="status-display">)rawliteral" + securityStatus + R"rawliteral(</div>
            <div class="status-sub">Core 0 Watchdog Status</div>
        </div>

        <div class="dashboard-grid">
            <!-- INPUT FORM -->
            <div class="card">
                <h2>📝 Inject Command</h2>
                <p>Send text to the Sandbox (Core 1). Type <b>ATTACK</b> to simulate malware.</p>
                <form action="/submit" method="POST">
                    <input type="text" name="userdata" placeholder="Type here..." required autofocus>
                    <button type="submit">SEND</button>
                </form>
                <div class="helper-text">Last Sent: <strong>)rawliteral" + submittedData + R"rawliteral(</strong></div>
            </div>

            <!-- OUTPUT DATA -->
            <div class="card">
                <h2>📡 Core 1 Output</h2>
                <p>Encrypted result returned by the Sandbox:</p>
                <div class="data-box">)rawliteral" + latestData + R"rawliteral(</div>
            </div>
        </div>
    </div>
</body>
</html>
)rawliteral";
  return html;
}

// ============================================
// HTTP HANDLERS
// ============================================
void handleRoot() {
  server.send(200, "text/html", generateWebPage());
}

void handleSubmit() {
  if (server.hasArg("userdata")) {
    submittedData = server.arg("userdata");
    submittedData.trim();
    
    // --- CRITICAL STEP ---
    // Send the user input over GPIO 1 (TX) to the Pico!
    // The Pico (Core 0) is listening for this.
    Serial.println(submittedData); 
  }
  
  // Reload page to show "Last Sent"
  server.sendHeader("Location", "/");
  server.send(303);
}

// ============================================
// SETUP
// ============================================
void setup() {
  // Initialize Hardware Serial (Pins 1 & 3)
  // This connects to Pico GP4/GP5
  Serial.begin(SERIAL_BAUD_RATE);
  
  delay(1000);
  
  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  
  // Note: Since Serial is connected to Pico, IP is hard to see.
  // Check your router for the device IP or unplug RX/TX momentarily to debug.

  server.on("/", handleRoot);
  server.on("/submit", HTTP_POST, handleSubmit);
  
  server.begin();
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
  server.handleClient();

  // Listen for messages from Pico (Core 0 or Core 1) on GPIO 3 (RX)
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();

    if (line.length() > 0) {
       
       // 1. DATA: (Normal Encrypted Output from Core 1)
       if (line.startsWith("DATA:")) {
         latestData = line.substring(6); // Remove "DATA: "
         securityStatus = "SYSTEM SECURE";
         statusColor = "#4CAF50"; // Green
         statusIcon = "🔒";
       }
       
       // 2. SECURITY ALERT: (Warning from Core 0)
       else if (line.indexOf("SECURITY ALERT") >= 0) {
         securityStatus = "MALICIOUS INPUT DETECTED";
         latestData = "--- PROCESS TERMINATED BY KERNEL ---";
         statusColor = "#f44336"; // Red
         statusIcon = "⚠️";
       }
    }
  }
}