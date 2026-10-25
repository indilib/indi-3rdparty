// Proposed UNO R4 WiFi roof relay repair, 2026-10-19. NOT hardware commissioned.
// Pin assignments, electrical polarity and 2000 ms pulse retained from supplied sketch.
// Relay outputs are momentary motor-controller button contacts, NOT roof position.
#include "secrets.h"
#include <WiFiS3.h>
#define NO_OTA_NETWORK
#include <ArduinoOTA.h>
#include "RoofUpload.h"
#include "RoofStorage.h"
#include "BuildIdentity.h"
#ifndef OTA_PASSWORD
#error "Run prepare.py to add the private OTA_PASSWORD to secrets.h"
#endif
static_assert(sizeof(OTA_PASSWORD)==33, "OTA_PASSWORD must be 32 characters");
#include <stdint.h>
#include <stdio.h>

const int RELAY_OPEN = 4;
const int RELAY_CLOSE = 7;
const int RELAY_ABORT = 8;
const int RELAY_UNUSED = 12;
const int LIMIT_OPENED = 2;
const int LIMIT_CLOSED = 3;
// Electrical active level retained; NO/NC contact choice does not define coil polarity.
const bool INVERT_RELAY_LOGIC = true;
#define RELAY_ON(pin) digitalWrite(pin, INVERT_RELAY_LOGIC ? HIGH : LOW)
#define RELAY_OFF(pin) digitalWrite(pin, INVERT_RELAY_LOGIC ? LOW : HIGH)
const uint32_t RELAY_DURATION = 2000;
const uint32_t COMMAND_TIMEOUT = 1000;
const size_t COMMAND_CAPACITY = 96;
const unsigned int READS_PER_LOOP = 64;

WiFiServer server(7624);
WiFiClient client;
WiFiServer uploadServer(7625);
WiFiClient uploadClient;
RoofStorage roofStorage;
RoofUpload<RoofStorage> upload(roofStorage);
bool maintenance=false, uploadSession=false;
uint32_t maintenanceStarted=0;
const uint32_t MAINTENANCE_MS=300000;
void cancelMaintenance();
void serviceUpload();
bool maintenanceClosed();
int activeRelay = -1;
uint32_t relayStartTime = 0;
char incomingCommand[COMMAND_CAPACITY];
size_t commandLength = 0;
bool inCommand = false;
uint32_t commandStartTime = 0;
bool clientSession = false;
void sendMsg(const String &msg);
const size_t EVENT_COUNT = 16;
struct RoofEvent { uint32_t sequence; uint32_t uptime; int pin; char label[24]; };
RoofEvent events[EVENT_COUNT];
uint32_t eventSequence = 0;
size_t eventNext = 0;
size_t eventUsed = 0;

void recordEvent(const char *label, int pin) {
  RoofEvent &entry = events[eventNext];
  entry.sequence = ++eventSequence;
  entry.uptime = static_cast<uint32_t>(millis());
  entry.pin = pin;
  snprintf(entry.label, sizeof(entry.label), "%s", label);
  eventNext = (eventNext + 1) % EVENT_COUNT;
  if (eventUsed < EVENT_COUNT) ++eventUsed;
}

void sendEvent(const String &indexText) {
  // One bounded reply per request. Index 0 is newest, 15 is oldest retained.
  int index = 0;
  if (indexText.length() == 0 || indexText.length() > 2) {
    sendMsg("(NAK:ERROR:BadEventIndex)"); return;
  }
  for (unsigned int i = 0; i < indexText.length(); ++i) {
    const char c = indexText.charAt(i);
    if (c < '0' || c > '9') { sendMsg("(NAK:ERROR:BadEventIndex)"); return; }
    index = index * 10 + c - '0';
  }
  if (index >= static_cast<int>(eventUsed)) {
    sendMsg("(ACK:EVENT:NONE)"); return;
  }
  const RoofEvent &entry = events[(eventNext + EVENT_COUNT - 1 - index) % EVENT_COUNT];
  char reply[120];
  snprintf(reply, sizeof(reply), "(ACK:EVENT:seq=%lu,ms=%lu,age_ms=%lu,pin=%d,event=%s)",
           static_cast<unsigned long>(entry.sequence), static_cast<unsigned long>(entry.uptime),
           static_cast<unsigned long>(uint32_t(static_cast<uint32_t>(millis()) - entry.uptime)),
           entry.pin, entry.label);
  sendMsg(String(reply));
}

void processCommand(const String &rawCmd);
void sendMsg(const String &msg);
uint32_t clockMs() { return static_cast<uint32_t>(millis()); }
bool isRoofFullyOpen() { return digitalRead(LIMIT_OPENED) == LOW; }
bool isRoofFullyClosed() { return digitalRead(LIMIT_CLOSED) == LOW; }

void resetParser() {
  commandLength = 0;
  incomingCommand[0] = '\0';
  inCommand = false;
}

void relayLog(const char *event, int pin) {
  recordEvent(event, pin);
  // Never wait for USB or a full debug buffer in the control path.
  if (Serial && Serial.availableForWrite() >= 64) {
    Serial.print(event); Serial.print(" pin="); Serial.print(pin);
    Serial.print(" ms="); Serial.println(clockMs());
  }
}

void releaseRelays() {
  const int previous = activeRelay;
  RELAY_OFF(RELAY_OPEN);
  RELAY_OFF(RELAY_CLOSE);
  RELAY_OFF(RELAY_ABORT);
  RELAY_OFF(RELAY_UNUSED);
  activeRelay = -1;
  if (previous != -1) relayLog("RELAY OFF", previous);
}

void serviceRelayDeadline() {
  if (activeRelay != -1 && uint32_t(clockMs() - relayStartTime) >= RELAY_DURATION) {
    recordEvent("PULSE EXPIRED", activeRelay);
    releaseRelays();
  }
}

// Same ON during the current pulse is acknowledged without extending its deadline.
// Opposite direction is refused during a pulse. ABORT preempts it, never overlaps it.
bool beginPulse(int pin) {
  serviceRelayDeadline();
  if (activeRelay == pin) { recordEvent("DUPLICATE ON", pin); return true; }
  if (activeRelay != -1 && pin != RELAY_ABORT) { recordEvent("BUSY REJECTED", pin); return false; }
  const bool preempting = activeRelay != -1;
  releaseRelays();
  if (preempting) delay(25); // All coils OFF while relay contacts release.
  relayStartTime = clockMs();
  activeRelay = pin;
  RELAY_ON(pin);
  relayLog("RELAY ON", pin);
  return true;
}

void releaseOneRelay(int pin) {
  RELAY_OFF(pin);
  if (activeRelay == pin) {
    activeRelay = -1;
    relayLog("RELAY OFF", pin);
  }
}

void setup() {
  // Set the inactive latch BEFORE enabling outputs; no USB/serial dependency.
  const int pins[] = {RELAY_OPEN, RELAY_CLOSE, RELAY_ABORT, RELAY_UNUSED};
  for (int pin : pins) {
    RELAY_OFF(pin);
    pinMode(pin, OUTPUT);
  }
  releaseRelays();
  pinMode(LIMIT_OPENED, INPUT_PULLUP);
  pinMode(LIMIT_CLOSED, INPUT_PULLUP);
  Serial.begin(115200);
  recordEvent("BOOT", -1);
  resetParser();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) delay(500); // Outputs already inactive.
  server.begin();
}

void loop() {
  // Relay servicing is independent of roof state and parser/client state.
  serviceRelayDeadline();
  if (!client.connected()) {
    // Releasing a button contact does NOT stop physical roof travel.
    if (clientSession) recordEvent("CLIENT DISCONNECTED", activeRelay);
    clientSession = false;
    releaseRelays();
    resetParser();
    client.stop();
    WiFiClient next = server.available();
    if (next) {
      client = next;
      clientSession = true;
      recordEvent("CLIENT CONNECTED", -1);
    }
  }

  if (inCommand && uint32_t(clockMs() - commandStartTime) >= COMMAND_TIMEOUT) {
    resetParser();
    sendMsg("(NAK:ERROR:CommandTimeout)");
  }
  // Bounded work prevents a busy peer from starving relay deadlines.
  unsigned int reads = 0;
  while (client.connected() && client.available() && reads++ < READS_PER_LOOP) {
    serviceRelayDeadline();
    const int value = client.read();
    if (value < 0) break;
    const char c = static_cast<char>(value);
    if (c == '(') {
      resetParser();
      inCommand = true;
      commandStartTime = clockMs();
    }
    if (!inCommand) continue;
    if (c < 32 || c > 126 || commandLength >= COMMAND_CAPACITY - 1) {
      resetParser();
      sendMsg("(NAK:ERROR:BadFrame)");
      continue;
    }
    incomingCommand[commandLength++] = c;
    incomingCommand[commandLength] = '\0';
    if (c == ')') {
      processCommand(String(incomingCommand));
      resetParser();
    }
  }
  serviceRelayDeadline();
  serviceUpload();
  serviceRelayDeadline();
}

void sendMsg(const String &msg) {
  if (client.connected()) {
    client.print(msg);
    client.print('\n');
  }
}

void processCommand(const String &rawCmd) {
  String cmd = rawCmd;
  cmd.trim();
  if (!cmd.startsWith("(") || !cmd.endsWith(")")) {
    sendMsg("(NAK:ERROR:BadFrame)");
    return;
  }
  cmd = cmd.substring(1, cmd.length() - 1);
  const int p1 = cmd.indexOf(':');
  const String command = p1 < 0 ? cmd : cmd.substring(0, p1);
  if (command == "CON") {
    sendMsg("(ACK:0:V1.5-wifi-candidate)");
    return;
  }
  const int p2 = p1 < 0 ? -1 : cmd.indexOf(':', p1 + 1);
  const String part1 = p1 < 0 ? String("") :
                       (p2 < 0 ? cmd.substring(p1 + 1) : cmd.substring(p1 + 1, p2));
  const String part2 = p2 < 0 ? String("") : cmd.substring(p2 + 1);
  if (command == "GET" && part1 == "VERSION") {
    sendMsg("(ACK:VERSION:" ROOF_BUILD_ID ")"); return;
  }
  if (command == "GET" && part1 == "OTA") {
    sendMsg(maintenance ? "(ACK:OTA:MAINTENANCE)" : "(ACK:OTA:DISABLED)"); return;
  }
  if (command == "SET" && part1 == "OTA") {
    if (part2 == "OFF") {
      cancelMaintenance(); sendMsg("(ACK:OTA:DISABLED)"); return;
    }
    if (part2 != OTA_PASSWORD) { sendMsg("(NAK:ERROR:Auth)"); return; }
    // Credentials never echoed or stored in the event ring.
    if (!maintenanceClosed() || activeRelay != -1 || maintenance) {
      sendMsg("(NAK:ERROR:MaintenanceInterlock)"); return;
    }
    releaseRelays();
    maintenanceStarted=clockMs(); maintenance=true; uploadSession=false;
    uploadServer.begin(); recordEvent("OTA ENABLED", -1);
    sendMsg("(ACK:OTA:MAINTENANCE)"); return;
  }
  if (command == "SET" && maintenance) {
    sendMsg("(NAK:ERROR:Maintenance)"); return;
  }
  if (command == "GET") {
    if (part1 == "EVENT") sendEvent(part2);
    else if (part1 == "OPENED")
      sendMsg("(ACK:OPENED:" + String(isRoofFullyOpen() ? "ON" : "OFF") + ")");
    else if (part1 == "CLOSED")
      sendMsg("(ACK:CLOSED:" + String(isRoofFullyClosed() ? "ON" : "OFF") + ")");
    else if (part1 == "LOCKED") sendMsg("(ACK:LOCKED:OFF)");
    else if (part1 == "AUXSTATE") sendMsg("(ACK:AUXSTATE:OFF)");
    else sendMsg("(NAK:ERROR:BadGET)");
    return;
  }
  if (command == "SET") {
    int pin = -1;
    if (part1 == "OPEN") pin = RELAY_OPEN;
    else if (part1 == "CLOSE") pin = RELAY_CLOSE;
    else if (part1 == "ABORT") pin = RELAY_ABORT;
    if (pin == -1 || (part2 != "ON" && part2 != "OFF")) {
      sendMsg("(NAK:ERROR:BadSET)");
      return;
    }
    if (part2 == "OFF") releaseOneRelay(pin);
    else if (!beginPulse(pin)) {
      sendMsg("(NAK:ERROR:Busy)");
      return;
    }
    sendMsg("(ACK:" + part1 + ":" + part2 + ")");
    return;
  }
  sendMsg("(NAK:ERROR:UnknownCmd)");
}

// Maintenance is explicit, temporary and never inferred from a command ACK.
bool maintenanceClosed() { return isRoofFullyClosed() && !isRoofFullyOpen(); }
void cancelMaintenance() {
  upload.abort(); uploadClient.stop(); uploadSession=false;
  if (maintenance) { uploadServer.end(); recordEvent("OTA DISABLED", -1); }
  maintenance=false; releaseRelays();
}
void serviceUpload() {
  if (!maintenance) return;
  releaseRelays();
  if (!maintenanceClosed() || uint32_t(clockMs()-maintenanceStarted)>=MAINTENANCE_MS) {
    cancelMaintenance(); return;
  }
  if (!uploadSession) {
    WiFiClient next=uploadServer.available();
    if (!next) return;
    uploadClient=next; uploadSession=true; upload.start(clockMs());
    recordEvent("OTA CONNECTED", -1);
  }
  upload.tick(clockMs(), maintenanceClosed());
  if (!uploadClient.connected()) {
    upload.fail("disconnected");
  } else {
    // Fixed work per pass: roof limits, command lockout and deadlines continue.
    for(unsigned int n=0;n<64 && uploadClient.available();++n) {
      int b=uploadClient.read(); if(b<0) break;
      upload.feed(uint8_t(b),clockMs(),OTA_PASSWORD,maintenanceClosed());
      if(upload.ready) {
        uploadClient.print("READY\n"); upload.ready=false;
        // Wait for a distinct COMMIT exchange, not pipelined body bytes.
        break;
      }
      if(upload.phase==RoofUpload<RoofStorage>::FAILED ||
         upload.phase==RoofUpload<RoofStorage>::APPLIED) break;
    }
  }
  if(upload.phase==RoofUpload<RoofStorage>::FAILED) {
    uploadClient.print("ERROR:"); uploadClient.print(upload.error); uploadClient.print('\n');
    recordEvent("OTA REJECTED", -1); uploadClient.stop(); uploadSession=false;
  } else if(upload.phase==RoofUpload<RoofStorage>::APPLIED) {
    // Recheck physical sensors immediately before committing staged flash.
    if(!maintenanceClosed() || activeRelay!=-1) { cancelMaintenance(); return; }
    uploadClient.print("APPLYING\n");
    recordEvent("OTA APPLYING", -1); releaseRelays();
    delay(100); uploadClient.stop(); client.stop(); uploadServer.end(); server.end();
    roofStorage.apply(); // Successful apply resets the MCU; power must remain stable.
    // An unexpected return is a failed apply, not a successful upgrade.
    maintenance=false; uploadSession=false; recordEvent("OTA APPLY FAILED", -1);
    server.begin();
  }
}
