#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

const char* ssid = "Liku";      
const char* password = "9337028208";


#include <SoftwareSerial.h>
SoftwareSerial Serial2(14, 12);  // RX=14 (D5), TX=12 (D6)

ESP8266WebServer server(80);

String latest_data = "Waiting for data...";
String security_status = "SECURE";
String pasted_data = "Nothing submitted yet.";

//web page
String makePage() {
  String html =
    "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<meta http-equiv='refresh' content='3'>"
    "<style>"
    "body{font-family:sans-serif;background:#f4f4f4;text-align:center;}"
    ".box{background:white;padding:20px;margin:20px auto;width:80%;max-width:500px;"
    "border-radius:10px;box-shadow:0 0 10px rgba(0,0,0,0.1);}"
    "textarea{width:90%;height:120px;font-size:16px;border-radius:8px;padding:10px;}"
    "button{padding:10px 20px;font-size:16px;border:none;border-radius:8px;background:#333;color:white;}"
    "</style></head><body>"

    "<h1> Secure IoT Dashboard</h1>"

    "<div class='box'>"
    "<h2>Encrypted Sensor Data</h2>"
    "<p style='font-size:22px;font-family:monospace;'>" + latest_data + "</p>"
    "</div>"

    "<div class='box' style='background:" + (security_status == "SECURE" ? "lightgreen" : "salmon") + ";'>"
    "<h2>Status</h2><h3>" + security_status + "</h3>"
    "</div>"

    "<div class='box'>"
    "<h2>Paste Your Data</h2>"
    "<form action='/submit' method='POST'>"
    "<textarea name='userdata' placeholder='Paste text here...'></textarea><br><br>"
    "<button type='submit'>Submit</button>"
    "</form>"
    "</div>"

    "<div class='box'>"
    "<h2>Last Submitted Data</h2>"
    "<p style='font-size:18px;white-space:pre-wrap;'>" + pasted_data + "</p>"
    "</div>"

    "</body></html>";

  return html;
}


void handleRoot() {
  server.send(200, "text/html", makePage());
}


void handleSubmit() {
  if (server.hasArg("userdata")) {
    pasted_data = server.arg("userdata");
    pasted_data.trim();
    Serial.println("Received from browser: " + pasted_data);
  }
  server.sendHeader("Location", "/");
  server.send(303);   
}


void setup() {
  Serial.begin(115200);
  Serial2.begin(115200);

  WiFi.begin(ssid, password);
  Serial.print("Connecting");

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
   server.on("/submit", HTTP_POST, handleSubmit);
  server.begin();
}

}


void loop() {
  server.handleClient();


  if (Serial2.available()) {
    String line = Serial2.readStringUntil('\n');
    line.trim();

    if (line.startsWith("[SANDBOX] Received Encrypted Blob:")) {
      latest_data = line.substring(34);
      security_status = "SECURE";
    }
    else if (line.indexOf("SECURITY ALERT") >= 0) {
      security_status = "⚠️ ATTACK DETECTED! SYSTEM LOCKED ⚠️";
    }
  }
}