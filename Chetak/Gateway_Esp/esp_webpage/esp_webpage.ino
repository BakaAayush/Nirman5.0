#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// ============================================
// CONFIGURATION
// ============================================
const char* WIFI_SSID = "Liku";
const char* WIFI_PASSWORD = "9337028208";
const uint16_t WEB_SERVER_PORT = 80;
const uint32_t SERIAL_BAUD_RATE = 9600; 

ESP8266WebServer server(WEB_SERVER_PORT);

// ============================================
// STATE VARIABLES
// ============================================
String latestData = "Waiting for input...";
String securityStatus = "SYSTEM SECURE";
String submittedData = "";
String statusColor = "#4CAF50"; // Green
String statusIcon = "🔒";
int threatScore = 0;
String threatReason = "N/A";

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
    <title>Autonomous Security Monitor</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh; padding: 20px; color: #333;
        }
        .container { max-width: 1200px; margin: 0 auto; }
        header { text-align: center; color: white; margin-bottom: 30px; }
        header h1 { font-size: 2.5em; text-shadow: 0 3px 6px rgba(0,0,0,0.4); margin-bottom: 8px; }
        header p { font-size: 1.1em; opacity: 0.95; }
        
        .dashboard-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; }
        .card { background: white; border-radius: 12px; padding: 25px; box-shadow: 0 10px 25px rgba(0,0,0,0.3); }

        /* Status Card */
        #status-card {
            background: #4CAF50;
            color: white; text-align: center;
            transition: all 0.5s ease;
            grid-column: 1 / -1;
        }
        #status-card.alert { animation: pulse 1.5s infinite; }
        @keyframes pulse {
            0%, 100% { transform: scale(1); box-shadow: 0 10px 25px rgba(0,0,0,0.3); }
            50% { transform: scale(1.02); box-shadow: 0 15px 35px rgba(244, 67, 54, 0.5); }
        }
        .status-display { font-size: 2.8em; font-weight: bold; margin: 15px 0; }
        .status-sub { font-size: 1em; opacity: 0.9; margin-top: 8px; }
        
        /* Threat Score Bar */
        .threat-meter {
            background: rgba(255,255,255,0.3); height: 30px; border-radius: 15px;
            margin: 20px 0; position: relative; overflow: hidden;
        }
        .threat-bar {
            height: 100%; border-radius: 15px; transition: width 0.5s ease;
            background: linear-gradient(90deg, #4CAF50, #FFC107, #f44336);
            display: flex; align-items: center; justify-content: center;
            font-weight: bold; color: white; text-shadow: 0 1px 3px rgba(0,0,0,0.3);
        }
        
        .data-box {
            background: #f8f9fa; padding: 15px; border-radius: 8px;
            font-family: 'Courier New', monospace; font-weight: bold;
            color: #333; border-left: 5px solid #667eea; margin-top: 10px;
            word-break: break-all; min-height: 60px; font-size: 0.95em;
        }

        form { display: flex; gap: 10px; margin-top: 15px; flex-wrap: wrap; }
        input[type="text"] { 
            flex: 1; min-width: 200px; padding: 14px; border: 2px solid #ddd; 
            border-radius: 8px; font-size: 16px; transition: border 0.3s;
        }
        input[type="text"]:focus { outline: none; border-color: #667eea; }
        
        button {
            background: #667eea; color: white; border: none; padding: 14px 30px;
            border-radius: 8px; cursor: pointer; font-weight: bold; font-size: 16px;
            transition: all 0.3s ease;
        }
        button:hover { background: #5568d3; transform: translateY(-2px); }
        button:active { transform: translateY(0); }
        
        .info-text { color: #666; font-size: 0.9em; margin-top: 8px; line-height: 1.5; }
        
        .submitted-cmd {
            background: #e8eaf6; padding: 12px; border-radius: 6px;
            margin-top: 12px; font-family: 'Courier New', monospace;
            color: #3f51b5; border-left: 4px solid #5c6bc0; font-size: 0.95em;
        }
        
        .threat-badge {
            display: inline-block; background: #ff9800; color: white;
            padding: 4px 12px; border-radius: 12px; font-size: 0.85em;
            margin: 5px 5px 5px 0; font-weight: bold;
        }
        
        .examples {
            background: #fff3cd; border-left: 4px solid #ffc107;
            padding: 12px; margin-top: 15px; border-radius: 6px;
            font-size: 0.9em; color: #856404;
        }
        .examples strong { display: block; margin-bottom: 5px; }
        
        h2 { color: #333; margin-bottom: 10px; display: flex; align-items: center; gap: 8px; }
    </style>
    
    <script>
        setInterval(function() {
            fetch('/data')
                .then(response => response.json())
                .then(json => {
                    document.getElementById('data-out').innerText = json.data;
                    document.getElementById('sec-status').innerText = json.status;
                    document.getElementById('status-icon').innerText = json.icon;
                    document.getElementById('submitted-cmd').innerText = json.submitted;
                    document.getElementById('threat-score-text').innerText = json.threat + '%';
                    document.getElementById('threat-reason').innerText = json.reason;
                    
                    // Update threat bar
                    const threatBar = document.getElementById('threat-bar');
                    threatBar.style.width = json.threat + '%';
                    
                    const statusCard = document.getElementById('status-card');
                    statusCard.style.background = json.color;
                    
                    if (json.color === '#f44336') {
                        statusCard.classList.add('alert');
                    } else {
                        statusCard.classList.remove('alert');
                    }
                })
                .catch(error => console.log('Error:', error));
        }, 500);
    </script>
</head>
<body>
    <div class="container">
        <header>
            <h1>🛡️ Autonomous Security System</h1>
            <p>RISC-V Hardware Sandbox | Real-Time Threat Detection</p>
        </header>
        
        <!-- SECURITY STATUS CARD -->
        <div class="card" id="status-card">
            <div style="font-size: 60px;" id="status-icon">🔒</div>
            <div class="status-display" id="sec-status">SYSTEM SECURE</div>
            <div class="status-sub">Real-Time Behavioral Analysis Active</div>
            
            <div class="threat-meter">
                <div class="threat-bar" id="threat-bar" style="width: 0%;">
                    <span id="threat-score-text">0%</span>
                </div>
            </div>
            <div style="font-size: 0.9em; opacity: 0.9;">
                Threat Detection: <span id="threat-reason">N/A</span>
            </div>
        </div>

        <div class="dashboard-grid" style="margin-top: 20px;">
            <!-- INPUT FORM -->
            <div class="card">
                <h2>📝 Input Test Panel</h2>
                <p class="info-text">Submit any input - the system autonomously analyzes for threats:</p>
                
                <div class="examples">
                    <strong>🧪 Try These Examples:</strong>
                    <div><span class="threat-badge">Safe</span> Hello World</div>
                    <div><span class="threat-badge">SQL</span> admin' OR 1=1--</div>
                    <div><span class="threat-badge">Code</span> system("ls")</div>
                    <div><span class="threat-badge">Overflow</span> AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA</div>
                </div>
                
                <form action="/submit" method="POST" target="hidden-frame">
                    <input type="text" name="userdata" id="user-input" placeholder="Enter any text or code..." required autocomplete="off">
                    <button type="submit">ANALYZE</button>
                </form>
                <iframe name="hidden-frame" style="display:none;"></iframe>
                
                <div class="submitted-cmd">
                    <strong>Last Input:</strong> <span id="submitted-cmd">None</span>
                </div>
            </div>

            <!-- OUTPUT DATA -->
            <div class="card">
                <h2>📡 Analysis Result</h2>
                <p class="info-text">System response (encrypted if safe, blocked if malicious):</p>
                <div class="data-box" id="data-out">Waiting for input...</div>
            </div>
        </div>
    </div>
</body>
</html>
)rawliteral";
  return html;
}

// ============================================
// SERVER HANDLERS
// ============================================

void handleRoot() {
  server.send(200, "text/html", generateWebPage());
}

void handleSubmit() {
  if (server.hasArg("userdata")) {
    String input = server.arg("userdata");
    input.trim();
    submittedData = input;
    Serial.println(input);
    Serial.flush();
  }
  server.send(204); 
}

void handleData() {
  String json = "{";
  json += "\"data\":\"" + latestData + "\",";
  json += "\"status\":\"" + securityStatus + "\",";
  json += "\"color\":\"" + statusColor + "\",";
  json += "\"icon\":\"" + statusIcon + "\",";
  json += "\"threat\":" + String(threatScore) + ",";
  json += "\"reason\":\"" + threatReason + "\",";
  json += "\"submitted\":\"" + (submittedData.length() > 0 ? submittedData : "None") + "\"";
  json += "}";
  
  server.send(200, "application/json", json);
}

// ============================================
// SETUP & LOOP
// ============================================
void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) {
    delay(500);
    timeout++;
  }

  server.on("/", handleRoot);
  server.on("/submit", HTTP_POST, handleSubmit);
  server.on("/data", HTTP_GET, handleData);
  
  server.begin();
}

void loop() {
  server.handleClient();

  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();

    if (line.length() > 0) {
       // Parse threat detection message: "THREAT_DETECTED|SCORE:75|REASON:SQL_INJ CODE_INJ"
       if (line.startsWith("THREAT_DETECTED")) {
         securityStatus = "⚠️ THREAT DETECTED";
         statusColor = "#f44336"; // Red
         statusIcon = "🚨";
         
         // Extract threat score
         int scoreIdx = line.indexOf("SCORE:");
         int reasonIdx = line.indexOf("REASON:");
         if (scoreIdx > 0 && reasonIdx > 0) {
           String scoreStr = line.substring(scoreIdx + 6, reasonIdx - 1);
           threatScore = scoreStr.toInt();
           threatReason = line.substring(reasonIdx + 7);
         }
         
         latestData = "INPUT BLOCKED - MALICIOUS PATTERN DETECTED";
       }
       // Parse safe data: "DATA: XX XX XX | THREAT:5 | CLEAN"
       else if (line.startsWith("DATA:")) {
         securityStatus = "✓ SYSTEM SECURE";
         statusColor = "#4CAF50"; // Green
         statusIcon = "🔒";
         
         // Extract threat score and reason
         int threatIdx = line.indexOf("THREAT:");
         if (threatIdx > 0) {
           int pipeIdx = line.indexOf("|", threatIdx);
           String scoreStr = line.substring(threatIdx + 7, pipeIdx);
           threatScore = scoreStr.toInt();
           threatReason = line.substring(pipeIdx + 2);
         } else {
           threatScore = 0;
           threatReason = "CLEAN";
         }
         
         // Extract hex data
         int dataEnd = line.indexOf("|");
         latestData = line.substring(6, dataEnd > 0 ? dataEnd : line.length());
       }
       // Runtime violation
       else if (line.indexOf("SECURITY ALERT") >= 0 || line.indexOf("RUNTIME") >= 0) {
         securityStatus = "🛑 RUNTIME VIOLATION";
         latestData = "SANDBOX TERMINATED - ILLEGAL MEMORY ACCESS";
         statusColor = "#f44336";
         statusIcon = "⛔";
         threatScore = 100;
         threatReason = "RUNTIME_VIOLATION";
       }
    }
  }
}