#pragma once
#include <chrono>
#include <mutex>
#include <string>
#include <vector>
namespace tenmicron {
struct State {
 double raHours, decDegrees, azDegrees, altDegrees, julianDate;
 char pierSide; // Native E/W; deliberately not mapped to INDI yet.
 int nativeStatus;
 bool slewOrSettling, leapSecond;
 std::vector<std::string> extraFields;
};
struct MeridianState {
 int slewLimitDegrees, trackLimitDegrees, timeToEndMinutes, allowedSide;
 bool unattendedFlip;
 int targetDestinationSide; // Stored target: 0=none/unreachable, 2=west, 3=east.
};
struct FlipRequestResult {
 bool accepted;
 std::string message;
 State before; // Fresh tracking state captured under the same lock as FLIP.
 bool confirmedRejection=false; // Exact 0 followed by unchanged, settled tracking.
};
struct SlewResult {
 int code; // 0=accepted, 1..5=mount rejection, -1=local preflight/target rejection.
 std::string message;
 bool accepted() const { return code == 0; }
};
std::string formatTargetRA(double hours);
std::string formatTargetDec(double degrees);
State parseInfo(const std::string &payload);
std::string statusName(int code);
class Protocol {
 public:
 explicit Protocol(std::chrono::milliseconds timeout = std::chrono::milliseconds(2000));
 ~Protocol();
 Protocol(const Protocol&) = delete;
 Protocol& operator=(const Protocol&) = delete;
 void connect(const std::string &host, unsigned short port);
 void disconnect();
 void ultraPrecision();
 std::string product();
 std::string firmware();
 std::string controller();
 State poll();
 MeridianState meridian();
 // Motion layer only: not exposed by the read-only INDI driver.
 SlewResult slew(double raHours, double decDegrees, int forcedSide = 0);
 SlewResult syncOffset(double raHours, double decDegrees); // CM with explicitly acknowledged CMCFG0; never adds model points.
 FlipRequestResult requestFlip(double raHours, double decDegrees); // Protocol foundation only: not exposed by INDI yet.
 void haltSlew(); // Q: does not explicitly stop tracking.
 void stopAll(); // STOP: includes tracking.
 void park();
 void unpark();
 void setTracking(bool enabled);
 private:
 std::string exchangeUnlocked(const std::string &command, bool response, bool oneByte = false, const std::string &allowedBytes = "01", bool slewReply = false);
 int fd_ = -1;
 bool pendingFlagTerminator_ = false;
 std::chrono::milliseconds timeout_;
 std::mutex mutex_;
 void closeSocket();
 std::string exchange(const std::string &command, bool response, bool oneByte = false, const std::string &allowedBytes = "01");
};
}
