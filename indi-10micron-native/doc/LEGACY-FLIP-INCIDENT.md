# Original legacy flip stall: October 4, 2026

[Original Mount/Capture/INDI-message excerpt](legacy-flip-stall-20261004.txt), with
original line numbers, source hash and redacted local home paths. It is not a
standalone indiserver wire/debug trace.

Recorded environment: KStars 3.8.2 Stable; GM1000HPS firmware 3.4, Q-TYPE2016.
At the connection preceding this incident, driver messages report mount/config
unattended flip **disabled**. Later enabling in the same long log occurs after
this incident and must not be used as its configuration. This differs from the
maintainer's reported GM2000HPS forced/native flip workflow with unattended flip
enabled; they are not established as equivalent reproductions.

03:15:40.263 CDT: Ekos planned a flip, logged HA 0.333363 and meridian diff
0.333333 hours (approximately 5 degrees). This is the logged threshold arithmetic,
not a separately retained settings screenshot. 03:15:55.336: legacy driver logged
Gstat 0 → 1 before the pending frame finished. Frame 54/120 arrived at 03:16:01.005.
At 03:16:03.411/418 Mount was ready and Capture began flip processing; from
03:16:04.263 Mount repeatedly said not tracking/Idle. No successful flip/recovery
is established by this excerpt.

Historical forced-flip checkbox, controller slew/tracking limit snapshots and a
separate full indiserver debug trace have not been found in the retained evidence.
Do not fill those gaps using today's settings or infer a unique cause from status
1. This evidence motivates explicit state/completion tests, but does not prove a
controller fault or that the existing driver cannot be improved. Full current
native flip/capture recovery remains a clear-sky commissioning gate.
