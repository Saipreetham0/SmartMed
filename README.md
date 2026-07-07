# SmartMed — Medicine Reminder System (v2)

An ESP32-based 6-chamber medicine reminder with a web control panel, WiFi
setup, adherence tracking, and a REST API for a mobile app. **No GSM/SMS/calls**
— reminders are local (LED + buzzer + LCD) with a networked management UI.

## What's new in v2

- **WiFi station + AP fallback + mDNS** — device joins home WiFi and is reachable
  at `http://smartmed.local`; falls back to a setup Access Point.
- **Modern web control panel** — light/dark, live clock, next-dose view, and an
  unmistakable "take now" alert takeover. Served entirely from the device.
- **Token-authenticated REST API (`/api/v1`)** with CORS — ready for a Flutter
  app. See [API.md](API.md).
- **Per-chamber date window** — each medicine can run for a `From`/`Until` course,
  not just repeat daily.
- **OTA updates** (ArduinoOTA + `POST /api/v1/ota`) and **NTP** time sync.
- **LittleFS persistence** for schedules, adherence counts, WiFi creds, and token.
- **Non-blocking firmware** — a state machine replaces the old blocking alert loop,
  so the web server stays responsive.
- **Testable core** — pure logic extracted to a `MedLogic` library with native
  unit tests, plus on-hardware self-tests.

## Hardware

- ESP32 DevKit v1 · DS3231 RTC · 16×2 I²C LCD · 6× reed switches (MC-38) ·
  6× LEDs · active buzzer.
- Pin maps live in [include/AppConfig.h](include/AppConfig.h) (mapped and verified
  on real hardware; chamber 3's reed is LOW-active).

## Build & flash (PlatformIO)

```bash
pio run -e esp32doit-devkit-v1 -t upload   # firmware
pio test -e native                          # host-side logic tests
pio test -e esp32doit-devkit-v1             # on-hardware self-tests
```

## First-time setup

1. Join WiFi **`Medicine Reminder`** / **`medkit123`**, open `http://192.168.4.1`.
2. Enter your home WiFi in the setup card → device reboots and joins.
3. Reach it afterward at `http://smartmed.local`.

## Repository layout

- `src/main.cpp` — firmware (network, API, reminder state machine)
- `include/AppConfig.h` — pins, constants, chamber model
- `include/WebUI.h` — the web control panel (HTML/CSS/JS in PROGMEM)
- `lib/MedLogic/` — pure, testable scheduling/formatting logic
- `test/` — native + on-hardware tests
- `tools/` — bench utilities (LCD/RTC/reed/LED mappers, box verifier)
- `API.md` — REST API reference + Flutter client

## License

MIT
