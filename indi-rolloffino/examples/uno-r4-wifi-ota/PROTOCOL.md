# Protocol and failure behavior

Command TCP 7624; ASCII parenthesized frames, newline-terminated replies.
Single command client; keep the same connection throughout preflight/maintenance.
Rapid separate clients previously produced connection resets on reference WiFiS3.

| Request | Response / effect |
|---|---|
| (CON:0:0) | ACK with protocol version; no Actions advertised |
| (GET:VERSION:0) | (ACK:VERSION:build-id) |
| (GET:OPENED:0), (GET:CLOSED:0) | Corresponding ACK:...:ON or OFF |
| (SET:OPEN:ON), (SET:CLOSE:ON), (SET:ABORT:ON) | ACK and bounded two-second pulse |
| (GET:OTA:0) | ACK:OTA:DISABLED or MAINTENANCE |
| (SET:OTA:32-hex-token) | Enter authenticated maintenance only if closed, not open, relays inactive |
| (SET:OTA:OFF) | Cancel maintenance / close listener |
| (GET:EVENT:index) | Newest-first volatile event, index 0..15; no relay side effect |

Duplicate ON does not extend the current pulse. Opposite direction is refused
while busy. ABORT preempts with a 25 ms release interval. OFF releases its target;
unrelated OFF cannot cancel another output's deadline. Disconnect releases outputs
and parser state without replay. Incomplete command absolute timeout is 1000 ms.

Maintenance blocks software motor commands, expires at five minutes and cancels
on incompatible limits. TCP 7625 accepts:

```
ROOFOTA1 <32 lowercase hex token> <decimal binary size> <8 hex CRC32>\n
<binary bytes>
```

Only after size, CRC, vector and staging readback checks does receiver send READY.
Client must then send a distinct COMMIT newline; receiver sends APPLYING before
flash apply/reboot. Do not pipeline COMMIT with body. Timeout is 10 seconds idle,
180 seconds overall, within 300-second maintenance. Board/size guard plus token
are not cryptographic firmware signatures. A disconnected or incomplete transfer
does not apply. Power interruption during apply remains a USB recovery risk.

To verify partial-upload timeout, use the supplied interrupted-upload.py, with
mount parked, roof closed, motor OFF, native/standard INDI connection available,
and RollOff ino disconnected. It stages only 64 bytes and never sends COMMIT.
