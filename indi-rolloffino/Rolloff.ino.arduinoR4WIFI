//2025 This sketch enables a Blutezeit 1400-lb gate opener to OPEN/CLOSE with limit switches via WIFI. The WIFI address is dynamic; the PORT is 7624. 
//It was built with a Arduino Uno R4 WIFI and the Arduino 4-Relay Shield (bought directly from Arduino) using the NO ports attached to OPEN/CLOSE/STOP and the COMMON ports all attached to GND.
//Limit switches were ME8108 from amazon utilizing NO contacts. 2 wires required/one is attached to a ground with a 10k resistor the other contact is attached to D2 (OPENED) and D3 (CLOSED)
//It also enables updating sketches via Wifi... connect with the ARDUINO IDE on the SAME network with the IP address and Port.
//Designed to integrate with the Rolloffino driver in INDI
///////////////////////////////////////////////////////////////////////
#include "secrets.h"      // Must come before any use of WIFI_SSID / WIFI_PASSWORD
#include <WiFiS3.h>
#include <OTAUpdate.h>

// —— Configuration ———————————————————————————————————————
// Relay shield pins
const int RELAY_OPEN   = 4;
const int RELAY_CLOSE  = 7;
const int RELAY_ABORT  = 8;
const int RELAY_UNUSED = 12;

// Limit switches (active LOW)
const int LIMIT_OPENED = 2;
const int LIMIT_CLOSED = 3;

// If you wire C→NO instead of C→NC, set this to true
const bool INVERT_RELAY_LOGIC = true;

// Helper macros: respect the invert flag
#define RELAY_ON(pin)   digitalWrite(pin, INVERT_RELAY_LOGIC ? HIGH : LOW)
#define RELAY_OFF(pin)  digitalWrite(pin, INVERT_RELAY_LOGIC ? LOW  : HIGH)

// Pulse duration (ms)
const unsigned long RELAY_DURATION = 2000;

// TCP server
WiFiServer server(7624);
WiFiClient client;

// Roof states
enum RoofState { IDLE, OPENING, CLOSING, ABORTING };
RoofState roofState = IDLE;
unsigned long relayStartTime = 0;

// Parser buffers
String incomingCommand;
bool inCommand = false;

// Forward declarations
void processCommand(const String &cmd);
void sendMsg(const String &msg);

// Helpers
bool isRoofFullyOpen()   { return digitalRead(LIMIT_OPENED) == LOW; }
bool isRoofFullyClosed(){ return digitalRead(LIMIT_CLOSED) == LOW; }

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // — Relays all off (NC closed, NO open)
  pinMode(RELAY_OPEN,   OUTPUT); RELAY_OFF(RELAY_OPEN);
  pinMode(RELAY_CLOSE,  OUTPUT); RELAY_OFF(RELAY_CLOSE);
  pinMode(RELAY_ABORT,  OUTPUT); RELAY_OFF(RELAY_ABORT);
  pinMode(RELAY_UNUSED, OUTPUT); RELAY_OFF(RELAY_UNUSED);

  // Limit switches
  pinMode(LIMIT_OPENED,  INPUT_PULLUP);
  pinMode(LIMIT_CLOSED,  INPUT_PULLUP);

  // Connect to Wi-Fi
  Serial.print("Connecting to WiFi ");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }
  Serial.println("\nConnected, IP = " + WiFi.localIP().toString());

  // Start TCP server
  server.begin();
  Serial.println("Server listening on port 7624");
}

void loop() {
  // Handle OTA uploads (note: poll() not handle())
   OTAUpdate(); 

  // Clean up disconnected client
  if (client && !client.connected()) {
    client.stop();
    Serial.println(">> Client disconnected");
    roofState = IDLE;
  }

  // Accept new connection
  if (!client || !client.connected()) {
    WiFiClient nc = server.available();
    if (nc) {
      client = nc;
      inCommand = false;
      Serial.println(">> Client Connected!");
    }
  }

  // Read incoming characters
  if (client && client.connected()) {
    while (client.available()) {
      char c = client.read();
      Serial.print("‹in:› "); Serial.println(c);
      if (c == '(') {
        inCommand = true;
        incomingCommand = "";
      }
      if (inCommand) {
        incomingCommand += c;
        if (c == ')') {
          processCommand(incomingCommand);
          inCommand = false;
        }
      }
    }
  }

  // Auto-turn-off after pulse
  if ((roofState == OPENING || roofState == CLOSING || roofState == ABORTING)
      && (millis() - relayStartTime >= RELAY_DURATION)) {
    RELAY_OFF(RELAY_OPEN);
    RELAY_OFF(RELAY_CLOSE);
    RELAY_OFF(RELAY_ABORT);
    roofState = IDLE;
    Serial.println("Action complete, IDLE");
  }
}

// Send a response over TCP + newline
void sendMsg(const String &msg) {
  if (client && client.connected()) {
    client.print(msg);
    client.print('\n');
    Serial.println("Sent: " + msg);
  }
}

// Parse and execute full commands "(...)"  
void processCommand(const String &rawCmd) {
  String cmd = rawCmd;
  cmd.trim();
  if (cmd.startsWith("(") && cmd.endsWith(")"))
    cmd = cmd.substring(1, cmd.length() - 1);

  Serial.println("Received: " + cmd);

  int p1 = cmd.indexOf(':');
  String command = (p1 < 0 ? cmd : cmd.substring(0, p1));
  if (command == "CON") {
    sendMsg("(ACK:0:V1.3)");
    return;
  }

  int p2 = cmd.indexOf(':', p1 + 1);
  String part1 = (p1 < 0 ? "" : cmd.substring(p1 + 1, p2));
  String part2 = (p2 < 0 ? "" : cmd.substring(p2 + 1));

  if (command == "GET") {
    if (part1 == "OPENED")
      sendMsg("(ACK:OPENED:" + String(isRoofFullyOpen() ? "ON":"OFF") + ")");
    else if (part1 == "CLOSED")
      sendMsg("(ACK:CLOSED:" + String(isRoofFullyClosed() ? "ON":"OFF") + ")");
    else if (part1 == "LOCKED")
      sendMsg("(ACK:LOCKED:OFF)");
    else if (part1 == "AUXSTATE")
      sendMsg("(ACK:AUXSTATE:OFF)");
    else
      sendMsg("(NAK:ERROR:BadGET)");
    return;
  }

  if (command == "SET") {
    if (part1 == "OPEN") {
      if (part2 == "ON") {
        RELAY_ON(RELAY_OPEN);
        relayStartTime = millis(); roofState = OPENING;
        sendMsg("(ACK:OPEN:ON)");
      } else {
        RELAY_OFF(RELAY_OPEN); roofState = IDLE;
        sendMsg("(ACK:OPEN:OFF)");
      }
    }
    else if (part1 == "CLOSE") {
      if (part2 == "ON") {
        RELAY_ON(RELAY_CLOSE);
        relayStartTime = millis(); roofState = CLOSING;
        sendMsg("(ACK:CLOSE:ON)");
      } else {
        RELAY_OFF(RELAY_CLOSE); roofState = IDLE;
        sendMsg("(ACK:CLOSE:OFF)");
      }
    }
    else if (part1 == "ABORT") {
      RELAY_ON(RELAY_ABORT);
      relayStartTime = millis(); roofState = ABORTING;
      sendMsg("(ACK:ABORT:ON)");
    }
    else {
      sendMsg("(NAK:ERROR:BadSET)");
    }
    return;
  }

  // Unknown command
  sendMsg("(NAK:ERROR:UnknownCmd)");
}