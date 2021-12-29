# FAQ

## There a several functions I can do on the Handbox but not via your Driver, why?
The Mount has a very limited serial protocol. I can only support operations I found using my command fuzzing and reverse engineering. Anything not showing up using these methods will simply not work. There may be ways to emulate the same behaviour, however I'd rather keep the driver simple and minimize its complexity.

## The mount does not point where I told it to point, what is wrong?
The assumption, the mount is 100% accurate for once.
Let me spoil your expectation, that this will ever happen.
You will only hit the vicinity of your target and have to adjust it using your handbox direction keys. Issue a sync command from the Planetary software you use once you corrected your pointing. But I can not promise you better accuracy of the Goto function, because it is simply not that good. Replacing the entire mount may help the situation.
Furthermore, there seems to be a minor issue with the coordinates too. It shows completely different coordinates on the display, compared to those I sent to the mount.
I tried it with the Ascom driver from Bresser, sending the same coordinates with my driver and the mount pointed to the same direction in both cases. I concluded this seems to be a deviance which may be caused by a bug in either the mount or any driver component. I'm still observing this problem, and fix it once a solution is found.

## Software Autoguiding was removed (and reintroduced), why?
Yes it was experimental from the start, and I decided to remove it due to fact that its simply not possible with this mount, unless the handbox firmware gets a mayor overhaul.
However you can still use the ST-4 cable and it should work as intended by Bresser, or whoever originally created this mount.
Since I don't want to promise functionality that is not possible I removed Autoguiding from the driver. It may be reintroduces however if I'm in the mood.

The is reintroduced with version 0.900 because of user requests. However, its still buggy, and requires a firmware patch for proper operation and is therefore not recommended. If you want reliable guiding use the ST-4 cable.

## The software direction keys do not work but the Handbox keys work, why?
From my examinations I found these motion commands only to work when an object is tracked. The answer boils down to, the manufacturer intended it to work that way.

## The Sync function work behaves weirdly, why?
The sync function only works when:

- Tracking an object
- When the hand box direction keys were used to adjust pointing of the telescope.

If you use the direction keys in a planetary software issueing the sync command will do nothing.
I can not tell you why that is, only that it seems to be intended by the manufacturer.

## The handbox starts beeping when close to the meridian, what is wrong?
You have to issue the "meridian flip" via the handbox unfortunately, because there is no serial protocol command for this, nor does the mount indicate this situation to a remote client.
The meridian flip however is intended to protect your gear from jamming into the mount. So if you do an observation keep an eye on the hand box too. Hooking up a microphone some how may be of help if you intent to use the mount completely remotely.

## Can I look directly into the sun with my telescope without proper filters using your driver?
You want to get permanently blinded? Because this is exactly how you get permanently blinded!

