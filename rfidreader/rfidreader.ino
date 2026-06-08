#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <SPI.h>
#include <MFRC522.h>

// --- Configuration ---
const char* AP_SSID = "RFID-Writer-Portal";
const byte DNS_PORT = 53;

// --- Pin Definitions ---
#define SS_PIN    5
#define RST_PIN  21
#define SCK_PIN  18
#define MISO_PIN 19
#define MOSI_PIN 23

// --- Global Objects ---
MFRC522 rfid(SS_PIN, RST_PIN);
DNSServer dnsServer;
AsyncWebServer server(80);

// --- State Variables ---
String currentCardData = "No card read yet";
String pendingWriteData = "";
bool writeModeActive = false;
String writeStatus = "System Idle. Tap a card to read.";

// MIFARE Default Key (FFFFFFFFFFFF)
MFRC522::MIFARE_Key key = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Target Block (Sector 1, Block 4)
const byte targetBlock = 4; 

// --- HTML Portal Content (Fixed Redirect Bug) ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>RFID Read/Write Portal</title>
  <style>
    body { font-family: Arial, sans-serif; text-align: center; margin-top: 30px; background-color: #f4f4f9; color: #333; }
    .container { max-width: 450px; margin: auto; padding: 20px; background: white; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); }
    h1 { color: #007bff; }
    .box { font-size: 1.1rem; background: #e9ecef; padding: 15px; border-radius: 5px; margin: 15px 0; border-left: 5px solid #007bff; text-align: left; word-break: break-all; }
    input[type="text"] { width: 100%; padding: 10px; margin: 10px 0; border: 1px solid #ccc; border-radius: 4px; box-sizing: border-box; font-size: 1rem; }
    button { background-color: #28a745; color: white; padding: 10px 20px; border: none; border-radius: 4px; cursor: pointer; font-size: 1rem; width: 100%; }
    button:hover { background-color: #218838; }
    .status { font-weight: bold; margin-top: 15px; padding: 10px; border-radius: 5px; background: #f8f9fa; }
  </style>
</head>
<body>
  <div class="container">
    <h1>RFID Read / Write</h1>
    
    <h3>Card Contents (Block 4):</h3>
    <div class="box" id="card-contents">Waiting for card...</div>
    
    <hr>
    
    <h3>Write Data to Card</h3>
    <form id="writeForm">
      <input type="text" id="textToCard" name="textToCard" maxlength="16" placeholder="Enter max 16 characters..." required>
      <button type="submit">Prepare Write to Card</button>
    </form>
    
    <div class="status" id="status-display">Initializing...</div>
  </div>

  <script>
    // Handle form submission without reloading or changing pages
    document.getElementById('writeForm').addEventListener('submit', function(e) {
      e.preventDefault(); // Stop standard browser redirect
      
      let formData = new FormData();
      formData.append('textToCard', document.getElementById('textToCard').value);
      
      fetch('/write', {
        method: 'POST',
        body: formData
      });
    });

    // Periodically sync UI layout with ESP32 data variables
    setInterval(function() {
      fetch('/status')
        .then(response => response.json())
        .then(data => {
          document.getElementById('card-contents').innerText = data.cardData;
          document.getElementById('status-display').innerText = data.writeStatus;
          
          let statusBox = document.getElementById('status-display');
          if(data.writeMode) {
             statusBox.style.color = "#d39e00"; // Amber warning text
             statusBox.style.backgroundColor = "#fff3cd"; // Light yellow box
          } else {
             if(data.writeStatus.includes("Success")) {
                statusBox.style.color = "#155724"; // Success green text
                statusBox.style.backgroundColor = "#d4edda"; // Light green box
             } else {
                statusBox.style.color = "#004085"; // Standard dark blue text
                statusBox.style.backgroundColor = "#e8f4fd"; // Neutral light blue box
             }
          }
        });
    }, 1000);
  </script>
</body>
</html>
)rawliteral";

class CaptiveRequestHandler : public AsyncWebHandler {
public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}
  bool canHandle(AsyncWebServerRequest *request) override { return true; }
  void handleRequest(AsyncWebServerRequest *request) override {
    request->send_P(200, "text/html", index_html);
  }
};

void setup() {
  Serial.begin(115200);

  // Initialize Hardware SPI & RFID
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  rfid.PCD_Init();

  // Setup AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID);
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  // Web routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{";
    json += "\"cardData\":\"" + currentCardData + "\",";
    json += "\"writeStatus\":\"" + writeStatus + "\",";
    json += "\"writeMode\":" + String(writeModeActive ? "true" : "false");
    json += "}";
    request->send(200, "application/json", json);
  });

  // Background Form Submission receiver
  server.on("/write", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("textToCard", true)) {
      pendingWriteData = request->getParam("textToCard", true)->value();
      writeModeActive = true;
      writeStatus = "READY: Tap your RFID card now to WRITE...";
      Serial.println("Staging Payload: " + pendingWriteData);
    }
    request->send(200, "text/plain", "OK"); // Kept hidden behind the scenes now
  });

  server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);
  server.begin();
  Serial.println("System Ready.");
}

void loop() {
  dnsServer.processNextRequest();

  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  Serial.println("--- Card Tapped ---");
  MFRC522::StatusCode status;
  byte buffer[18];
  byte size = sizeof(buffer);

  // 1. Authenticate Sector 1 (contains block 4) using Key A
  status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, targetBlock, &key, &(rfid.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print("Auth Failed: "); Serial.println(rfid.GetStatusCodeName(status));
    currentCardData = "Authentication Failed.";
    goto card_end;
  }

  // 2. Perform Write Mode if active
  if (writeModeActive) {
    byte dataBlock[16] = {0};
    int len = pendingWriteData.length();
    for (int i = 0; i < 16; i++) {
      if (i < len) dataBlock[i] = pendingWriteData[i];
      else dataBlock[i] = ' '; 
    }

    status = rfid.MIFARE_Write(targetBlock, dataBlock, 16);
    if (status == MFRC522::STATUS_OK) {
      Serial.println("Write Successful!");
      writeStatus = "Success: Written '" + pendingWriteData + "' to card!";
    } else {
      Serial.print("Write Failed: "); Serial.println(rfid.GetStatusCodeName(status));
      writeStatus = "Error: Write transaction failed.";
    }
    
    writeModeActive = false; 
  }

  // 3. Read back current state
  status = rfid.MIFARE_Read(targetBlock, buffer, &size);
  if (status == MFRC522::STATUS_OK) {
    String readString = "";
    for (uint8_t i = 0; i < 16; i++) {
      if (isprint(buffer[i])) readString += (char)buffer[i];
      else readString += ".";
    }
    readString.trim();
    currentCardData = readString;
    Serial.println("Current Block Data: " + readString);
  } else {
    Serial.print("Read Failed: "); Serial.println(rfid.GetStatusCodeName(status));
    currentCardData = "Failed to read block memory.";
  }

card_end:
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}