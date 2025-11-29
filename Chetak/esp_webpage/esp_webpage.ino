#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

//
const char* ssid     = "Premchand";      // <--- CHANGE THIS
const char* password = "HolyAirBAll";  // <--- CHANGE THIS

ESP8266WebServer server(80);

// buffer to store data coming back from the Pico (via Serial)
String picoResponseBuffer = "";

//html webpage 
const char index_html[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8" />
<meta name="viewport" content="width=device-width,initial-scale=1" />
<title>Pico 2 Secure Loader</title>
<style>
:root{--accent:#007bff;--bg:#f7f8fb;--card:#ffffff;--muted:#6b7280}
body{font-family:Inter,system-ui,Arial,sans-serif;background:var(--bg);margin:0;padding:20px;display:flex;justify-content:center}
.container{width:100%;max-width:820px;background:var(--card);border-radius:12px;padding:20px;box-shadow:0 6px 20px rgba(15,23,42,0.06)}
h1{margin:0 0 8px;font-size:20px}
.lead{margin:0 0 18px;color:var(--muted);font-size:14px}
.row{display:flex;gap:12px;align-items:center;margin-bottom:12px}
textarea{width:100%;min-height:260px;padding:12px;border-radius:8px;border:1px solid #e6e9ef;font-family:monospace;font-size:13px;resize:vertical}
select,input[type="text"]{padding:8px;border-radius:8px;border:1px solid #e6e9ef}
.button{background:var(--accent);color:white;padding:10px 14px;border-radius:8px;border:none;cursor:pointer;font-weight:600}
.button.secondary{background:#eef2ff;color:var(--accent);border:1px solid rgba(0,123,255,0.12)}
.controls{display:flex;gap:10px;flex-wrap:wrap}
#statusBox{white-space:pre-wrap;background:#0f1724;color:#e6f0ff;padding:12px;border-radius:8px;min-height:120px;overflow:auto;border:1px solid rgba(0,0,0,0.06)}
.small{font-size:12px;color:var(--muted)}
.footer{margin-top:12px;display:flex;justify-content:space-between;align-items:center}
.progress{height:8px;border-radius:8px;background:#e9eef8;overflow:hidden;margin-top:8px}
.progress > i{display:block;height:100%;width:0;background:linear-gradient(90deg,var(--accent),#00a3ff)}
.notice{font-size:13px;color:#264653}
@media (max-width:600px){.row{flex-direction:column;align-items:stretch}}
</style>
</head>
<body>
<div class="container" role="main">
  <h1>Pico 2 Secure Loader</h1>
  <p class="lead">Paste your RISC-V program (HEX or Base64) in the box, choose format and click <strong>Upload to Pico</strong>.</p>

  <div class="row">
    <label class="small" for="format">Encoding</label>
    <select id="format" aria-label="encoding">
      <option value="hex">HEX (byte pairs)</option>
      <option value="base64">Base64</option>
    </select>

    <label class="small" for="entryName">Name (optional)</label>
    <input id="entryName" type="text" placeholder="program.bin" style="flex:1" />
  </div>

  <textarea id="program" placeholder="Paste HEX (e.g. 7f454c...) or Base64 here..."></textarea>

  <div class="controls" style="margin-top:12px">
    <button id="uploadBtn" class="button">Upload to Pico</button>
    <button id="clearBtn" class="button secondary">Clear</button>
    <button id="stopPollBtn" class="button secondary" style="display:none">Stop Live</button>
  </div>

  <div class="row" style="margin-top:14px">
    <div style="flex:1">
      <div class="small">Transfer progress</div>
      <div class="progress" aria-hidden="true"><i id="progressBar"></i></div>
    </div>
    <div style="width:220px;text-align:right">
      <div class="small">Last status</div>
      <div id="lastStatus" class="small" style="margin-top:6px;color:var(--muted)">idle</div>
    </div>
  </div>

  <h3 style="margin-top:18px;margin-bottom:6px">Pico Response</h3>
  <div id="statusBox">No output yet. Click <em>Upload to Pico</em>.</div>

  <div class="footer">
    <div class="notice">Endpoint: <code>/upload</code> (POST) · Poll: <code>/status</code></div>
    <div class="small">Built for ESP8266 HTTPS server</div>
  </div>
</div>

<script>
(() => {
  const uploadBtn = document.getElementById('uploadBtn');
  const clearBtn  = document.getElementById('clearBtn');
  const stopBtn   = document.getElementById('stopPollBtn');
  const progBox   = document.getElementById('program');
  const statusBox = document.getElementById('statusBox');
  const progressBar = document.getElementById('progressBar');
  const lastStatus  = document.getElementById('lastStatus');

  let polling = false;
  let pollTimer = null;
  let transferInProgress = false;

  function setProgress(p) { progressBar.style.width = Math.max(0, Math.min(100,p)) + '%'; }
  function appendStatus(text){
    statusBox.textContent += text;
    statusBox.scrollTop = statusBox.scrollHeight;
  }
  function resetStatus(){
    statusBox.textContent = '';
    lastStatus.textContent = 'idle';
  }

  clearBtn.addEventListener('click', () => { progBox.value = ''; resetStatus(); setProgress(0); });

  stopBtn.addEventListener('click', () => { if(polling) stopPolling(); });

  uploadBtn.addEventListener('click', async () => {
    if(transferInProgress) return;
    const payload = progBox.value.trim();
    if(!payload){ alert('Paste your HEX or Base64 program first.'); return; }

    const format = document.getElementById('format').value;
    const name   = document.getElementById('entryName').value || 'program';

    try {
      transferInProgress = true;
      uploadBtn.textContent = 'Sending…';
      uploadBtn.disabled = true;
      setProgress(6);
      lastStatus.textContent = 'sending';

      const res = await fetch('/upload', {
        method: 'POST',
        headers: { 'Content-Type': 'text/plain', 'X-Program-Format': format, 'X-Program-Name': name },
        body: payload
      });

      setProgress(34);

      if(!res.ok){
        const txt = await res.text();
        lastStatus.textContent = 'error';
        appendStatus('Upload failed: ' + res.status + '\n' + txt + '\n');
        uploadBtn.disabled = false;
        uploadBtn.textContent = 'Upload to Pico';
        transferInProgress = false;
        setProgress(0);
        return;
      }

      const reply = await res.text();
      setProgress(60);
      appendStatus('Server: ' + reply + '\n');
      lastStatus.textContent = 'waiting for pico';
      startPolling();
      uploadBtn.textContent = 'Uploading…';
    } catch(err){
      appendStatus('Network error: ' + err + '\n');
      lastStatus.textContent = 'network error';
      uploadBtn.disabled = false;
      uploadBtn.textContent = 'Upload to Pico';
      transferInProgress = false;
      setProgress(0);
    }
  });

  async function pollOnce(){
    try {
      const res = await fetch('/status', {cache:'no-cache'});
      if(res.status === 204) return; 
      
      const data = await res.text();
      if(!data) return;
      appendStatus(data);
      setProgress(90);

      const normalized = data.toLowerCase();
      if(normalized.includes('safe') || normalized.includes('malicious') || normalized.includes('error')){
        appendStatus('\n--- Session finished ---\n');
        lastStatus.textContent = 'done';
        stopPolling();
        setProgress(100);
        uploadBtn.disabled = false;
        uploadBtn.textContent = 'Upload to Pico';
        transferInProgress = false;
      } else {
        lastStatus.textContent = 'running';
      }
    } catch(err){ appendStatus('Poll error: ' + err + '\n'); }
  }

  function startPolling(){
    if(polling) return;
    polling = true;
    stopBtn.style.display = 'inline-block';
    pollTimer = setInterval(pollOnce, 700);
  }
  function stopPolling(){
    polling = false;
    stopBtn.style.display = 'none';
    if(pollTimer) { clearInterval(pollTimer); pollTimer = null; }
  }

  // Clear buffer on load
  (async function init(){
    try { await fetch('/status?clear=1', {method:'POST'}); }catch(e){}
    setProgress(0); resetStatus();
  })();
})();
</script>
</body>
</html>
)rawliteral";

//server handling part

void handleRoot() {
  server.send(200, "text/html", index_html);
}

// Receives the HEX/Base64 from Browser -> Sends to Pico via Serial
void handleUpload() {
  if (server.hasArg("plain") == false) {
    server.send(400, "text/plain", "Body not received");
    return;
  }
  
  String payload = server.arg("plain");
  
  // Send the payload to the Pico over UART (Serial)
  // Ensure your Pico is listening on the RX pin connected to ESP8266 TX
  Serial.println(payload); 
  
  server.send(200, "text/plain", "Data sent to Pico (" + String(payload.length()) + " bytes)");
}

// Browser polls this to see what the Pico replied
void handleStatus() {
  // If ?clear=1 is passed, wipe the buffer
  if (server.hasArg("clear")) {
    picoResponseBuffer = "";
    server.send(200, "text/plain", "Cleared");
    return;
  }

  // If buffer is empty, send 204 (No Content) so the JS knows to wait
  if (picoResponseBuffer.length() == 0) {
    server.send(204, "text/plain", "");
  } else {
    // Send the accumulated data and clear the buffer
    server.send(200, "text/plain", picoResponseBuffer);
    picoResponseBuffer = ""; 
  }
}



void setup() {
  //comms with pico
  Serial.begin(115200);
  delay(10);

  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Wait for connection (LED blink could be added here)
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  
  // Note: We can't print "Connected" to Serial easily because 
  // Serial is connected to the Pico for data transfer! 
  // If you need to debug IP, use a different Serial port (Serial1) or blink an LED.

  // Define Web Server Routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/upload", HTTP_POST, handleUpload);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/status", HTTP_POST, handleStatus); // handle clear request

  server.begin();
}

void loop() {
  
  server.handleClient();

  // 2. Read incoming data from Pico (Serial) and add to buffer
  while (Serial.available()) {
    char c = (char)Serial.read();
    picoResponseBuffer += c;
  }
}