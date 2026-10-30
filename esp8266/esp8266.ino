#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h> // Required for HTTPS

const char* ssid = "GG_TEP";
const char* password = "lolaflorez&";

// 1. Updated to HTTPS because Cloudflare Workers require it
const char* cloudflareUrl = "https://atmega-esp32-cloudflare-test.nico-holzman.workers.dev/update";

String latestPayload = "";
bool newDataAvailable = false;
String uartBuffer = "";

unsigned long lastSendTime = 0;
const unsigned long sendInterval = 5000; // 5 seconds

void setup() {
  // Serial1 (GPIO 2 - TX only) used for debugging via a USB-to-TTL adapter
  Serial1.begin(115200);
  delay(10);
  Serial1.println("\n[ESP8266] Booting...");

  // 1. Initialize main serial port at 9600 baud for the ATmega
  Serial.begin(9600);
  delay(10);

  // 2. SWAP: Moves UART hardware pins to GPIO 13 (RX) and GPIO 15 (TX)
  Serial.swap();
  Serial1.println("[ESP8266] UART Swapped. Make sure ATmega TX is connected to GPIO 13.");

  // Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial1.print("[ESP8266] Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial1.print(".");
  }
  Serial1.println("\n[ESP8266] WiFi Connected!");
}

void loop() {
  // 1. Read JSON data streamed from the ATmega over the swapped pins (GPIO 13)
  while (Serial.available() > 0) {
    char c = (char)Serial.read();

    if (c == '\n') {
      uartBuffer.trim(); 
      if (uartBuffer.length() > 5 && uartBuffer.startsWith("{")) {
        latestPayload = uartBuffer;
        newDataAvailable = true; 
        Serial1.println("[UART] Received valid JSON payload.");
      }
      uartBuffer = "";
    } else if (c != '\r') {
      uartBuffer += c;
    }
  }

  // 2. Send payload to Cloudflare every 5 seconds using HTTPS
  if (millis() - lastSendTime >= sendInterval) { // Fixed missing '-' operator
    lastSendTime = millis();

    if (newDataAvailable && WiFi.status() == WL_CONNECTED) {
      newDataAvailable = false; // Consume data so it isn't sent twice

      WiFiClientSecure client;     // Secure client for HTTPS
      client.setInsecure();        // Bypass SSL certificate validation (required for ESP8266 to trust Cloudflare)
      HTTPClient http;
      http.setTimeout(10000);      // 10 second timeout

      Serial1.println("[HTTP] Sending HTTPS POST to Cloudflare...");

      if (http.begin(client, cloudflareUrl)) {
        http.addHeader("Content-Type", "application/json");
        
        int httpResponseCode = http.POST(latestPayload);
        
        if (httpResponseCode > 0) {
          Serial1.printf("[HTTP] POST success! Response code: %d\n", httpResponseCode);
          String response = http.getString();
          Serial1.println("[HTTP] Response body: " + response);
        } else {
          Serial1.printf("[HTTP] POST failed. Error: %s\n", http.errorToString(httpResponseCode).c_str());
        }
        
        http.end();
      } else {
        Serial1.println("[HTTP] Connection failed (http.begin error)");
      }
    }
  }
}