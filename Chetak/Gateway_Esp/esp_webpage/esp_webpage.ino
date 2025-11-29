#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// ============================================
// CONFIGURATION
// ============================================
const char* WIFI_SSID = "Liku";
const char* WIFI_PASSWORD = "9337028208";
const uint16_t WEB_SERVER_PORT = 80;

// BAUD RATE (Must match Pico)
const uint32_t SERIAL_BAUD_RATE = 9600; 

// ============================================
// GLOBAL OBJECTS
// ============================================
ESP8266WebServer server(WEB_SERVER_PORT);

// ============================================
// STATE VARIABLES
// ============================================
String latestData = "Waiting for data...";
String securityStatus = "SECURE";
String submittedData = "Nothing submitted yet.";

// ============================================
// HTML GENERATION
// ============================================
String generateWebPage() {
  bool isSecure = (securityStatus == "SECURE");
  String statusColor = isSecure ? "#4CAF50" : "#f44336"; // Green or Red
  String statusIcon = isSecure ? "🔒" : "⚠️";
  
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta http-equiv="refresh" content="3">
    <title>IoT Security Dashboard</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh; padding: 20px; color: #333;
        }
        .container { max-width: 1200px; margin: 0 auto; }
        header { text-align: center; color: white; margin-bottom: 30px; }
        header h1 { font-size: 2.5em; text-shadow: 2px 2px 4px rgba(0,0,0,0.3); }
        .dashboard-grid {
            display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 20px;
        }
        .card {
            background: white; border-radius: 15px; padding: 25px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.2);
        }
        .card-header {
            display: flex; align-items: center; gap: 10px;
            margin-bottom: 15px; border-bottom: 2px solid #f0f0f0; padding-bottom: 15px;
        }
        .data-display {
            background: #f8f9fa; padding: 20px; border-radius: 10px;
            border-left: 4px solid #667eea; font-family: 'Courier New', monospace;
            word-wrap: break-word; color: #333; font-weight: bold;
        }
        .status-card { background: )rawliteral" + statusColor + R"rawliteral(; color: white; text-align: center; }
        .status-card .card-header { border-bottom-color: rgba(255,255,255,0.3); justify-content: center; }
        .status-card h2 { color: white; }
        .status-display { font-size: 2em; font-weight: bold; padding: 20px; }
        
        button {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white; padding: 12px 30px; border: none; border-radius: 25px;
            cursor: pointer; font-size: 16px; width: 100%; margin-top: 10px;
        }
        textarea { width: 100%; padding: 10px; border-radius: 5px; border: 1px solid #ddd; }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>🔐 Secure IoT Dashboard</h1>
            <p>Hardware-Isolated RISC-V Monitoring</p>
        </header>
        
        <div class="dashboard-grid">
            <!-- Encrypted Data -->
            <div class="card">
                <div class="card-header"><span>📡</span><h2>Encrypted Data</h2></div>
                <div class="data-display">)rawliteral" + latestData + R"rawliteral(</div>
            </div>
            
            <!-- Security Status -->
            <div class="card status-card">
                <div class="card-header"><span>)rawliteral" + statusIcon + R"rawliteral(</span><h2>Status</h2></div>
                <div class="status-display">)rawliteral" + securityStatus + R"rawliteral(</div>
            </div>
            
            <!-- Submit Data -->
            <div class="card">
                <div class="card-header"><span>📝</span><h2>Submit Data</h2></div>
                <form action="/submit" method="POST">
                    <textarea name="userdata" placeholder="Enter data..." required></textarea>
                    <button type="submit">Submit</button>
                </form>
            </div>

             <!-- Last Submitted -->
            <div class="card">
                <div class="card-header"><span>📋</span><h2>Last Submitted</h2></div>
                <div class="data-display">)rawliteral" + submittedData + R"rawliteral(</div>
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
    Serial.println("WEB INPUT: " + submittedData); // This sends data OUT to Pico (and USB)
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

// ============================================
// SETUP
// ============================================
void setup() {
  // Initialize Hardware Serial (Pins 1 & 3)
  // This handles BOTH USB Debugging AND Pico Communication
  Serial.begin(SERIAL_BAUD_RATE);
  
  delay(1000);
  Serial.println("\n--- ESP8266 STARTED ---");

  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    // We avoid printing dots '.' continuously to keep the Serial clean for the Pico
  }
  
  // Print IP once (Pico will see this too, but should ignore it)
  Serial.println("");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/submit", HTTP_POST, handleSubmit);
  
  server.begin();
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
  server.handleClient();

  // Listen for data from Pico on Hardware Serial
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();

    // 1. Encrypted Data
    if (line.startsWith("DATA:")) {
      latestData = line.substring(6); 
      securityStatus = "SECURE";
      // We don't print "Received" to Serial here to avoid feedback loops
    }
    // 2. Attack Alert
    else if (line.indexOf("SECURITY ALERT") >= 0) {
      securityStatus = "⚠️ ATTACK DETECTED";
    }
  }
}