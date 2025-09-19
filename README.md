# SIM800L Arduino Door Alarm

**Prototype**: SIM800L-based door alarm (Arduino) — lightweight alarm that calls/accepts SMS commands from a whitelist, stores config in EEPROM and supports temporary SMS-based authentication.

> Clear purpose: a compact, hackable prototype for learning GSM control, EEPROM storage, and embedded string parsing.

---

## Table of contents
- [Status](#status)
- [Features](#features)
- [Hardware](#hardware)
- [Wiring (quick)](#wiring-quick)
- [Software / Libraries](#software--libraries)
- [Build & Upload (Arduino)](#build--upload-arduino)
- [Configuration & Defaults](#configuration--defaults)
- [SMS command protocol (usage)](#sms-command-protocol-usage)
- [EEPROM layout](#eeprom-layout)
- [Runtime behaviour / state machine notes](#runtime-behaviour--state-machine-notes)
- [Known issues & gotchas (important)](#known-issues--gotchas-important)
- [TODO / Roadmap (prioritised)](#todo--roadmap-prioritised)
- [Security notes](#security-notes)
- [Contributing / Development notes](#contributing--development-notes)
- [License](#license)

---

## Status
Prototype / alpha. This repository is intended as a learning project and functional prototype — not a finished, production-grade device. The code is written for AVR (Arduino), uses `SoftwareSerial` for SIM800L communication and stores a small whitelist and a password hash in EEPROM.

Use with caution (especially power supply to SIM800L) and test on bench before deploying.

---

## Features
- Door state detection (reed/switch on `switchpin`), LED indicators and alarm (siren) control.
- When alarm triggers, attempts to call numbers stored in the one by one whitelist.
- Receive SMS commands and parse them: `auth`, `add`, `del`, `setpass`, `enable`, `disable`, `stats`, `alarm off` (some are partially implemented).
- Local whitelist stored in EEPROM (structured entries + simple checksum).
- MD5-hashed password stored in EEPROM (first 16 bytes).
- Temporary authentication window: `auth <password>` authenticates the sender for a short time so they can `add` and `delete` numbers, and `set` the password.

---

## Hardware
Minimum hardware used in this project (what the code expects):
- **Arduino** (Uno/Nano/ATmega328) — any AVR with enough RAM/EEPROM.
- **SIM800L** GSM/GPRS module connected via `SoftwareSerial`.
  - `SoftwareSerial mySerial(3, 2);` — this means Arduino pin **3** is `RX` (listens to SIM TX) and pin **2** is `TX` (sends to SIM RX). Ensure wiring follows that convention.
- Reed switch / door switch connected to `switchpin` (pin `4`) using `INPUT_PULLUP` (switch to GND when closed).
- Status LEDs: `redledpin` = pin `5`, `greenledpin` = pin `6`.
- Alarm output (siren) on `alarmpin` = pin `7`.

**Power notes**: SIM800L requires a stable power source able to provide high current peaks (up to ~2A) during transmission. DO NOT power SIM800L from the Arduino 5V regulator if you cannot supply these peaks — use a separate supply (LiPo or regulated 4V) with common ground.

---

## Wiring (quick)
- SIM800L TX -> Arduino digital pin 3 (SoftwareSerial RX)
- SIM800L RX -> Arduino digital pin 2 (SoftwareSerial TX)
- SIM800L GND -> Arduino GND
- SIM800L VCC -> appropriate 3.7–4.2V power source (see power notes)
- Door switch(normally open) -> Arduino pin 4 (configured INPUT_PULLUP) to GND when closed
- Red LED -> pin 5 (through resistor) -> GND
- Green LED -> pin 6 (through resistor) -> GND
- Alarm (transistor / driver) -> pin 7 (use a transistor/MOSFET and flyback diode if driving a speaker or motor)

---

## Software / Libraries
- Arduino IDE (or equivalent toolchain)
- Uses Arduino built-in libraries: `EEPROM`, `SoftwareSerial`.
- External library: `MD5` (an MD5 helper library). Install via Library Manager or include a compatible MD5 header/source.

IMPORTANT NOTE: change SoftwareSerial buffer length to 128 for reliable operation.

---

## Build & Upload (Arduino)
1. Install required MD5 library in Arduino IDE.
2. Open the sketch in Arduino IDE.
3. Select the correct board and COM port.
4. Upload. Monitor using Serial Monitor 9600 baud.

**Tip**: run `AT` tests in the Serial Monitor with SIM module connected. SIM800L usually responds with `OK`.

---

## Configuration & Defaults
The main configurable constants are near the top of the sketch:
- `eepromStartIndex = 16` — first EEPROM byte reserved for the stored password hash; structured numbers are stored after this.
- `alarmDuration = 60000` — alarm active duration in milliseconds (60s by default).
- `authTimeOut = 900000` — temporary authentication window in ms (900000 ms = 15 minutes).

**Defaults in `setup()`** (for prototype convenience):
- `writeNumberToEeprom("989029026240");` — example whitelist entry is written on every boot in this prototype. Remove or change in deployment.
- `storePassword("1234");` — prototype stores default password `1234` on every boot (unsafe) — change/remove before real use.

---

## SMS command protocol (usage)
**Format**: SMS messages expected as plain ASCII lines. Example incoming raw SMS format accepted by the code:
```
+CMT: "+ZZXXXXXXXXX","", "<date>"
<command>
```
Commands (text commands are case-sensitive as implemented):
- `auth <password>` — authenticate sender for a short window. Example: `auth 1234`
- `add <12-digit-number>` — add a phone number to whitelist (requires prior `auth` from sender)
  - Expected format currently: 12 digits (e.g. `989xxxxxxxxx`) — code checks `number.length() == 12`.
- `del <12-digit-number>` — remove a number from EEPROM whitelist (requires authenticated sender)
- `setpass <password>` — replace stored password (requires authenticated sender)
- `enable` / `disable` — enable or disable alarm logic (requires whitelist sender)
- `stats` — respond/send current door/alarm state (whitelist sender)
- `alarm off` — (hook present but not fully implemented/verified in prototype)

**Notes**:
- Whitelist numbers are stored as 12-character strings (country code + local); adapt format to your locale.
- The `auth` command is allowed even if sender is not already whitelisted; successful `auth` adds sender to a short-term `authenticated` list.

---

## EEPROM layout
- Bytes `[0..eepromStartIndex-1]` (0..15 by default) are written with the 16-byte MD5 hash generated by `storePassword()`.
- Starting at `eepromStartIndex` the sketch stores fixed-size entries of type `storedNumber`:
  ```c
  struct storedNumber { char phoneNumber[13]; unsigned char checksum; };
  // sizeof == 14 bytes
  ```
- Each storedNumber occupies 14 bytes (13 bytes for `phoneNumber` including null terminator, + 1 checksum). The simple checksum is the byte-sum of `phoneNumber` bytes.

---

## Runtime behaviour / state machine notes
- `loop()` calls `updateSerial()` to read available data from `mySerial` (SIM), then `handlecommand()` to parse and handle SMS/commands. There are blocking `delay()` calls during dialing and a simple timed dial loop during alarm.
- When the door switch indicates opening and `enable==true`, the alarm arms and the device iterates the whitelist to call each stored number for a set period.

---

## Known issues & gotchas (important)
> These items are real issues in the prototype. Address them before trusting the device in a real installation.

### 1. **Parsing & string handling fragility**
- The code mixes `String` objects and C-style `char[]` buffers. On AVR (ATmega328) `String` frequently causes heap fragmentation; prefer fixed `char[]` buffers and `strncpy`/`strcmp` to reduce instability.
- `extractSMS()` uses `strtok()` and `strncpy()`; `strtok()` modifies the input buffer and its global state can interfere with other parsers. Be careful if you later change to zero-copy. Sanitize `
`/`\n` and non-printable characters before parsing.

### 2. **SoftwareSerial reliability / buffer size**
- `SoftwareSerial` on AVR is CPU-intensive and has a small internal buffer; long SMS or bursts can be truncated. In testing this was observed: increasing buffer / using `AltSoftSerial` or using a board with a second hardware UART (Mega/Leonardo/ESP32) makes parsing far more reliable.

### 3. **Power / SIM800L issues**
- SIM modules are sensitive to power instability; brownouts during TX bursts can cause weird characters, truncated responses or resets. Use a beefy, low-ESR supply and place decoupling caps.



### 4. **String length/format assumptions**
- `writeNumberToEeprom()` insists `number.length() == 12`. Phone formats vary — be explicit about expected format or relax the check and sanitize input.

### 5. **Security: MD5 & defaults**
- Using MD5 without salt is weak for password storage; and the prototype stores default password `1234` at every boot (in `setup()`), which is insecure. Use a stronger, salted hash and remove default passwords from `setup()`.

---

## TODO / Roadmap (prioritised)
**High priority (fix before real deployment):**
1. fix "1234" password set on every boot.
2. make the device give feedback with sms about the excution of commands.
**Low priority**
1.implement state machine for better readability. 

---

## Security notes
- Remove `storePassword("1234")` from `setup()` before publishing hardware to others or deploying.
- Do not expose the serial debug port to attackers if device deployed in public.
- Consider stronger password hashing and adding a salt stored separately.


