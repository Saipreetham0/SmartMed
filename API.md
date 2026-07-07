# SmartMed REST API (v1) — Flutter Integration Guide

Firmware: `2.0.0` · Base path: `/api/v1` · Content type: `application/json`

The device runs an HTTP server on port **80**. All endpoints send permissive
CORS headers, so a Flutter app (mobile or web) can call them directly.

---

## 1. Finding the device (base URL)

The device has two modes:

| Mode | When | Base URL |
|------|------|----------|
| **AP / setup** | No WiFi saved, or can't connect | Phone joins WiFi `Medicine Reminder` / `medkit123`, then `http://192.168.4.1` |
| **STA / online** | Joined home WiFi | `http://smartmed.local` (mDNS) or the DHCP IP shown by `/api/v1/info` |

> mDNS (`smartmed.local`) works on iOS and most Android 12+; if it fails, fall
> back to the IP. You can also let the user scan/pick via `GET /api/v1/info`.

---

## 2. Authentication

Every endpoint **except** `health`, `info`, `pair` and the WiFi-setup calls (in
AP mode) requires a **bearer token**:

```
Authorization: Bearer <deviceToken>
```

### Pairing (one time, over the AP)
While the device is in **setup/AP mode**, call:

```
GET /api/v1/pair  ->  { "ok": true, "token": "…", "device_id": "…" }
```

Store the token on the phone (e.g. `flutter_secure_storage`). After that the app
uses it over the home network too. `pair` returns `403` once the device is online
(so the token can't be harvested remotely).

---

## 3. Endpoints

### `GET /api/v1/health` — _(open)_
```json
{ "ok": true, "status": "up", "fw": "2.0.0" }
```

### `GET /api/v1/info` — _(open)_
```json
{ "ok": true, "device_id": "A1B2C3D4E5F6", "fw": "2.0.0", "chambers": 6,
  "mode": "sta", "ip": "192.168.1.42", "mdns": "smartmed.local",
  "uptime_s": 1234, "free_heap": 210000, "rtc_ok": true }
```

### `GET /api/v1/pair` — _(open in AP mode only)_
Returns the auth token (see §2).

### `GET /api/v1/wifi/scan` — _(open in AP mode, else auth)_
```json
{ "ok": true, "networks": [ { "ssid": "Home", "rssi": -52, "secure": true } ] }
```

### `POST /api/v1/wifi` — _(open in AP mode, else auth)_
Body: `{ "ssid": "Home", "pass": "secret" }`
Saves credentials and **reboots** to join that network.

### `GET /api/v1/status` — _(auth)_
```json
{ "ok": true, "time": "23:00:45", "date": "08/07/2026", "epoch": 1751929245,
  "mode": "sta", "ap": "Medicine Reminder",
  "chambers": [
    { "id": 0, "label": "Aspirin", "hour": 8, "minute": 0, "enabled": true,
      "start": "2026-07-01", "end": "2026-07-14",
      "alerting": false, "taken": false, "taken_count": 12, "missed_count": 1,
      "last_taken": "08/07 08:02", "last_taken_epoch": 1751... } ]
}
```
Poll this every ~2 s to drive the UI. `start`/`end` are the course window (see below);
empty string `""` means unbounded.

### `POST /api/v1/schedules` — _(auth)_
Body: full array of 6 chambers.
```json
{ "chambers": [
  { "label": "Aspirin", "hour": 8, "minute": 0, "enabled": true,
    "start": "2026-07-01", "end": "2026-07-14" }, … ] }
```
- `hour` 0–23, `minute` 0–59. Persisted to flash.
- **`start` / `end`** (optional) — a **course window** in `YYYY-MM-DD` (the format
  a Flutter `DatePicker` / HTML `<input type=date>` produces). A reminder only fires
  on dates **on or after `start`** and **on or before `end`**. Send `""` (or omit) to
  leave a bound open — e.g. `start` only = "from this date onward", both empty =
  "every day, indefinitely". Fields not included in the body are left unchanged.

### `POST /api/v1/time` — _(auth)_
Set the RTC and/or timezone.
```json
{ "year":2026, "month":7, "day":8, "hour":23, "minute":0, "second":0, "tz":19800 }
```
`tz` is UTC offset in seconds (IST = 19800). Any field group is optional.

### `POST /api/v1/dismiss` — _(auth)_
Manually mark an alerting chamber as taken. Body: `{ "chamber": 0 }`

### `POST /api/v1/test` — _(auth)_
Fire a test alert (LED + buzzer) on a chamber. Body: `{ "chamber": 0 }`

### `POST /api/v1/ota` — _(auth)_
`multipart/form-data` upload of `firmware.bin`. On success the device flashes
and reboots. (ArduinoOTA on hostname `smartmed`, password = token, also works
from PlatformIO/Arduino IDE.)

### Errors
Non-2xx responses are `{ "ok": false, "error": "…" }`. `401` = missing/bad token.

---

## 4. Flutter client (drop-in)

```dart
import 'dart:convert';
import 'package:http/http.dart' as http;

class SmartMedApi {
  SmartMedApi(this.baseUrl, {this.token});
  String baseUrl;            // e.g. http://smartmed.local  or  http://192.168.4.1
  String? token;

  Map<String, String> get _headers => {
        'Content-Type': 'application/json',
        if (token != null) 'Authorization': 'Bearer $token',
      };

  Uri _u(String p) => Uri.parse('$baseUrl/api/v1$p');

  Future<Map<String, dynamic>> info() async =>
      jsonDecode((await http.get(_u('/info'))).body);

  /// Call once while connected to the device AP to obtain the token.
  Future<String> pair() async {
    final j = jsonDecode((await http.get(_u('/pair'))).body);
    token = j['token'] as String;
    return token!;
  }

  Future<Map<String, dynamic>> status() async =>
      jsonDecode((await http.get(_u('/status'), headers: _headers)).body);

  /// Each chamber: {label, hour, minute, enabled, start?, end?}
  /// start/end are 'YYYY-MM-DD' course bounds ('' = open).
  Future<void> saveSchedules(List<Map<String, dynamic>> chambers) =>
      http.post(_u('/schedules'),
          headers: _headers, body: jsonEncode({'chambers': chambers}));

  Future<void> setTime(DateTime d, {int tz = 19800}) =>
      http.post(_u('/time'),
          headers: _headers,
          body: jsonEncode({
            'year': d.year, 'month': d.month, 'day': d.day,
            'hour': d.hour, 'minute': d.minute, 'second': d.second, 'tz': tz,
          }));

  Future<void> dismiss(int chamber) => http.post(_u('/dismiss'),
      headers: _headers, body: jsonEncode({'chamber': chamber}));

  Future<void> test(int chamber) => http.post(_u('/test'),
      headers: _headers, body: jsonEncode({'chamber': chamber}));

  Future<List<dynamic>> wifiScan() async =>
      (jsonDecode((await http.get(_u('/wifi/scan'), headers: _headers)).body)
          as Map)['networks'] as List;

  Future<void> wifiConnect(String ssid, String pass) => http.post(_u('/wifi'),
      headers: _headers, body: jsonEncode({'ssid': ssid, 'pass': pass}));
}
```

### Typical app flow
1. First run → guide user to join the `Medicine Reminder` AP → `pair()` →
   store token securely.
2. Show WiFi list (`wifiScan`) → `wifiConnect(ssid, pass)` → device reboots onto
   home WiFi. App switches base URL to `http://smartmed.local`.
3. Normal use: poll `status()`, edit schedules with `saveSchedules()`, `test()`,
   `dismiss()`.

> Add `http` and `flutter_secure_storage` to `pubspec.yaml`.
