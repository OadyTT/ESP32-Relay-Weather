# ESP32 Smart Relay & Weather Monitor

โปรเจค IoT บน ESP32 สำหรับควบคุม Relay 3 ตัวด้วยปุ่มกด แสดงเวลาจริงจาก NTP, สภาพอากาศและคุณภาพอากาศจาก OpenWeatherMap บนจอ OLED 0.96 นิ้ว พร้อมแจ้งเตือนผ่าน Telegram และเชื่อมต่อ MQTT Broker (HiveMQ)

--- # 23May2026

## สารบัญ

- [ฟีเจอร์หลัก](#ฟีเจอร์หลัก)
- [อุปกรณ์ที่ใช้](#อุปกรณ์ที่ใช้)
- [การเชื่อมต่อวงจร](#การเชื่อมต่อวงจร)
- [Library ที่ใช้](#library-ที่ใช้)
- [การติดตั้งและเปิดโปรแกรม](#การติดตั้งและเปิดโปรแกรม)
- [การตั้งค่า WiFi ครั้งแรก](#การตั้งค่า-wifi-ครั้งแรก)
- [การ Reset WiFi](#การ-reset-wifi)
- [การตั้งค่า Telegram Bot](#การตั้งค่า-telegram-bot)
- [NTP เวลาจริง](#ntp-เวลาจริง)
- [MQTT (HiveMQ)](#mqtt-hivemq)
- [Layout หน้าจอ OLED](#layout-หน้าจอ-oled)
- [การทำงานของปุ่ม Switch](#การทำงานของปุ่ม-switch)
- [การแจ้งเตือน Telegram](#การแจ้งเตือน-telegram)
- [การตั้งค่าในโปรแกรม](#การตั้งค่าในโปรแกรม)
- [Serial Monitor Output](#serial-monitor-output)
- [โครงสร้างไฟล์](#โครงสร้างไฟล์)
- [ข้อควรระวัง](#ข้อควรระวัง)

---

## ฟีเจอร์หลัก

| ฟีเจอร์ | รายละเอียด |
|--------|-----------|
| ควบคุม Relay | 3 ตัว Toggle ON/OFF ด้วยปุ่ม SW1–SW3 |
| WiFi Manager | ตั้งค่า WiFi ผ่าน Captive Portal — ไม่ต้อง hardcode |
| NTP เวลาจริง | ซิงค์เวลาจาก `pool.ntp.org` timezone **Asia/Bangkok (UTC+7)** |
| สภาพอากาศ | อุณหภูมิ, ความชื้น, ความเร็วลม จาก OpenWeatherMap |
| คุณภาพอากาศ | AQI, PM2.5, PM10, CO, NO₂, O₃ |
| อัปเดตอัตโนมัติ | ดึงข้อมูลใหม่ทุก **2 นาที** |
| จอ OLED | แสดงเวลา, อากาศ, สถานะ Relay + indicator MQTT |
| Telegram แจ้งเตือน | Online, Relay ON/OFF, รายงานอากาศ, แจ้งเตือน AQI/PM2.5 |
| MQTT Telemetry | Publish weather, relay state, status ไปยัง HiveMQ |
| MQTT Control | Subscribe รับคำสั่ง ON/OFF relay จากภายนอก |
| LWT | Broker แจ้ง offline อัตโนมัติเมื่อบอร์ดหลุด |
| WiFi Reset | กด SW1 ค้าง **5 วินาที** ขณะ boot เพื่อล้าง WiFi |
| Debounce | ป้องกัน Switch Bounce ด้วย millis() 20ms |
| Serial Debug | แสดงข้อมูลทั้งหมดผ่าน Serial Monitor 115200 baud |

---

## อุปกรณ์ที่ใช้

| อุปกรณ์ | รุ่น / สเปค |
|--------|------------|
| Microcontroller | ESP32 DevKit V1 (DOIT) |
| จอแสดงผล | OLED 0.96" I2C SSD1306 128×64 px |
| Relay Module | 3-Channel Active Low Relay |
| ปุ่มกด | Tactile Switch × 3 (Active Low + External Pull-up 10kΩ) |
| Power Supply | USB 5V / 1A ขึ้นไป |

---

## การเชื่อมต่อวงจร

### Relay Module (Active Low)

| Relay | GPIO | การทำงาน |
|-------|------|---------|
| Relay 1 | GPIO **17** | LOW = ON, HIGH = OFF |
| Relay 2 | GPIO **16** | LOW = ON, HIGH = OFF |
| Relay 3 | GPIO **4**  | LOW = ON, HIGH = OFF |

```
ESP32 GPIO17 ──[10kΩ]── Relay IN1
ESP32 GPIO16 ──[10kΩ]── Relay IN2
ESP32 GPIO4  ──[10kΩ]── Relay IN3
Relay JD-VCC ── 5V
Relay GND    ── GND
```

### Switch (Active Low + External Pull-up 10kΩ)

| Switch | GPIO | หน้าที่ใน loop | หน้าที่ขณะ boot |
|--------|------|--------------|----------------|
| SW1 | GPIO **34** | Toggle Relay1 (กดสั้น) | กดค้าง 5 วิ = Reset WiFi |
| SW2 | GPIO **35** | Toggle Relay2 | — |
| SW3 | GPIO **32** | Toggle Relay3 | — |

```
3.3V ──[10kΩ]──┬── GPIO34 (SW1)
               └── SW1 ── GND
```

> GPIO 34 และ 35 เป็น **Input-Only** — ต้องใช้ External Pull-up 10kΩ เสมอ

### OLED 0.96" I2C (SSD1306)

| OLED | GPIO ESP32 | หมายเหตุ |
|------|------------|---------|
| VCC  | 3.3V | ห้ามต่อ 5V |
| GND  | GND | |
| SDA  | GPIO **21** | I2C Data |
| SCL  | GPIO **22** | I2C Clock |

> I2C Address: `0x3C` (default)

---

## Library ที่ใช้

| Library | Version | หน้าที่ |
|---------|---------|--------|
| `Arduino.h` | built-in | Arduino core |
| `WiFi.h` | built-in (ESP32) | WiFi stack |
| `WiFiManager` (tzapu) | ^2.0.17 | Captive Portal ตั้งค่า WiFi |
| `HTTPClient.h` | built-in (ESP32) | HTTP GET/POST |
| `ArduinoJson` (bblanchon) | ^7.0.0 | Parse/Build JSON |
| `Wire.h` | built-in | I2C protocol |
| `time.h` | built-in (ESP32) | NTP sync + `getLocalTime()` |
| `Adafruit SSD1306` | ^2.5.7 | ควบคุม OLED display |
| `Adafruit GFX Library` | ^1.11.9 | Graphics primitives |
| `PubSubClient` (knolleary) | ^2.8 | MQTT client |

> PlatformIO ดาวน์โหลด library ทั้งหมดอัตโนมัติจาก `platformio.ini`

---

## การติดตั้งและเปิดโปรแกรม

### สิ่งที่ต้องติดตั้ง

- [Visual Studio Code](https://code.visualstudio.com)
- **PlatformIO IDE** Extension ใน VS Code
- **CH340 USB Driver** (สำหรับ ESP32 DevKit V1)

### ขั้นตอน

**1. ติดตั้ง PlatformIO IDE**
- เปิด VS Code → `Ctrl+Shift+X` → ค้นหา `PlatformIO IDE` → Install → Restart VS Code

**2. เปิด Project**
```
File → Open Folder → เลือกโฟลเดอร์ ESP32-FirstProject-with-AI
```

**3. แก้ไขค่าตั้งต้นใน `src/main.cpp`**

| บรรทัด | define | ค่าที่ต้องแก้ |
|--------|--------|-------------|
| 17 | `OWM_API_KEY` | API Key จาก openweathermap.org |
| 23 | `TG_BOT_TOKEN` | Token จาก @BotFather |
| 24 | `TG_CHAT_ID` | Chat ID ของคุณ |
| 31 | `BOARD_ID` | ชื่อ unique ของบอร์ดนี้ |

**4. Build และ Upload**
- กด **Build** (✓) → กด **Upload** (→) ที่ Status Bar
- shortcut: `Ctrl+Alt+U`
- หาก Upload ไม่ได้ → กด **BOOT** บนบอร์ดค้างไว้ระหว่าง Upload

**5. เปิด Serial Monitor**
- กดไอคอนปลั๊กที่ Status Bar หรือ `Ctrl+Alt+S`
- Baud: **115200**

---

## การตั้งค่า WiFi ครั้งแรก

1. OLED แสดง `Connecting WiFi... / Connect to: ESP32-Setup`
2. ESP32 เปิด AP ชื่อ **"ESP32-Setup"**
3. เชื่อมต่อ WiFi **"ESP32-Setup"** ด้วยมือถือหรือคอมพิวเตอร์
4. เปิด `http://192.168.4.1` → **Configure WiFi** → เลือก SSID → ใส่ Password → **Save**
5. ESP32 restart และเชื่อมต่อ WiFi อัตโนมัติ
6. Telegram ได้รับ `✅ ESP32 Online` พร้อม IP + สถานะ MQTT

> AP Portal หมดเวลาใน **120 วินาที** แล้ว ESP32 จะ restart ใหม่

---

## การ Reset WiFi

1. **กด SW1 ค้างไว้ขณะบอร์ดกำลัง boot**
2. OLED แสดง progress bar นับถอยหลัง 5 วินาที
3. ค้างครบ **5 วินาที** → ลบ credentials → restart → เปิด AP Portal ใหม่
4. ปล่อยก่อน 5 วินาที → ยกเลิก บูตตามปกติ

---

## การตั้งค่า Telegram Bot

### สร้าง Bot
1. เปิด Telegram → คุยกับ **@BotFather** → `/newbot` → รับ **Bot Token**

### หา Chat ID
1. ส่งข้อความอะไรก็ได้ให้ Bot
2. เปิด `https://api.telegram.org/bot<TOKEN>/getUpdates`
3. ดูค่า `"id"` ใน `"chat"`

### ใส่ค่าใน code
```cpp
#define TG_BOT_TOKEN  "123456789:ABCdefGHIjklMNOpqrsTUVwxyz"
#define TG_CHAT_ID    "987654321"
```

---

## NTP เวลาจริง

| ค่า | รายละเอียด |
|-----|-----------|
| NTP Server | `pool.ntp.org` |
| Timezone | **Asia/Bangkok** (UTC+7) |
| UTC Offset | `25200` วินาที (7 × 3600) |
| DST | `0` (ไทยไม่มี DST) |
| Retry | สูงสุด 20 ครั้ง (~10 วินาที) |
| อัปเดต OLED | ทุก **1 วินาที** |

**รูปแบบที่แสดงบน OLED:**
```
23/05/2026  14:35:22
```

---

## MQTT (HiveMQ)

### Broker
| ค่า | รายละเอียด |
|-----|-----------|
| Broker | `broker.hivemq.com` |
| Port | `1883` (TCP, ไม่เข้ารหัส) |
| Auth | ไม่ต้อง (HiveMQ Free) |
| Client ID | `ESP32_{BOARD_ID}_{random_hex}` |
| Keep Alive | 60 วินาที |
| Auto-reconnect | ทุก 5 วินาที |

### Topic Schema

```
esp32/{BOARD_ID}/
├── telemetry/
│   ├── status      ← online / offline (LWT)
│   ├── weather     ← ข้อมูลอากาศ JSON
│   └── relay       ← สถานะ relay JSON
└── control/
    └── relay/
        ├── 1       ← รับคำสั่ง ON/OFF Relay1
        ├── 2       ← รับคำสั่ง ON/OFF Relay2
        └── 3       ← รับคำสั่ง ON/OFF Relay3
```

### Telemetry Payloads

**`telemetry/status`** (retained):
```json
{
  "board_id": "esp32_nst_01",
  "status": "online",
  "ip": "192.168.1.105",
  "time": "23/05/2026 14:35:22"
}
```

**`telemetry/weather`** (retained):
```json
{
  "board_id": "esp32_nst_01",
  "city": "Nakhon Si Thammarat",
  "temp": 32.4,
  "humidity": 78,
  "aqi": 2,
  "pm25": 12.3,
  "pm10": 18.7
}
```

**`telemetry/relay`** (retained):
```json
{
  "board_id": "esp32_nst_01",
  "relay1": false,
  "relay2": true,
  "relay3": false
}
```

### สั่ง Relay ผ่าน MQTT

Publish ไปยัง `esp32/{BOARD_ID}/control/relay/{1|2|3}`:

| Payload | ผลลัพธ์ |
|---------|--------|
| `ON` หรือ `1` หรือ `TRUE` | เปิด Relay |
| `OFF` หรือ `0` หรือ `FALSE` | ปิด Relay |

**LWT (Last Will Testament):**
เมื่อบอร์ดหลุดจาก Broker โดยไม่ได้ disconnect ปกติ:
```json
{"status": "offline"}
```
จะถูก publish ไปยัง `telemetry/status` อัตโนมัติโดย Broker

### OLED Indicator
- **●** (ขาว ขวาบน) = MQTT Connected
- **○** (ขอบ ขวาบน) = MQTT Offline

### ทดสอบด้วย MQTT Explorer
```
Host:      broker.hivemq.com
Port:      1883
Subscribe: esp32/esp32_nst_01/telemetry/#
Publish:   esp32/esp32_nst_01/control/relay/1
Payload:   ON
```

---

## Layout หน้าจอ OLED

จอ 128×64 pixels แบ่ง 5 แถว:

```
┌──────────────────────────────●┐  y=0
│▓▓ Nakhon Si Thammarat ▓▓▓▓▓▓ │  Title bar (ขาว/ดำ)  ●=MQTT status
│ 23/05/2026  14:35:22          │  y=11  วันที่ + เวลา (NTP)
│ T:32.4°C         H:78%       │  y=22  อุณหภูมิ / ความชื้น
│ AQI:2(Good)      PM:12        │  y=33  AQI / PM2.5
│───────────────────────────────│  y=43  เส้นแบ่ง
│ ╔R1:ON╗  [R2:--]  [R3:--]    │  y=45  Relay status boxes
└───────────────────────────────┘  y=63
```

| สัญลักษณ์ | ความหมาย |
|----------|---------|
| กล่องขาวทึบ `R1:ON` | Relay เปิดอยู่ |
| กล่องเส้นขอบ `R2:--` | Relay ปิดอยู่ |
| ● (ขวาบน title bar) | MQTT Connected |
| ○ (ขวาบน title bar) | MQTT Offline |

**ลำดับ Startup:**
```
Connecting WiFi... → WiFi Connected! → NTP Syncing → MQTT Connect → Fetching weather... → หน้าหลัก
```

---

## การทำงานของปุ่ม Switch

| ปุ่ม | GPIO | กดสั้น (loop) | กดค้าง 5 วิ (boot เท่านั้น) |
|-----|------|-------------|--------------------------|
| SW1 | 34 | Toggle Relay1 ON↔OFF | Reset WiFi credentials |
| SW2 | 35 | Toggle Relay2 ON↔OFF | — |
| SW3 | 32 | Toggle Relay3 ON↔OFF | — |

- Debounce: **20ms** (non-blocking millis)
- OLED + MQTT + Telegram อัปเดตทันทีเมื่อ Relay เปลี่ยน

---

## การแจ้งเตือน Telegram

| เหตุการณ์ | ข้อความ |
|----------|--------|
| บอร์ด Online | `✅ ESP32 Online` + IP + สถานะ MQTT |
| Relay เปลี่ยน (ปุ่ม) | `🔌 Relay1 เปิด (ON) ✅` |
| Relay เปลี่ยน (MQTT) | `📡 [MQTT] Relay1 เปิด (ON) ✅` + `สั่งผ่าน MQTT` |
| อัปเดตอากาศ (ทุก 2 นาที) | Temp, Hum, AQI, PM2.5, PM10 |
| AQI/PM2.5 เกินค่ากำหนด | `⚠️ แจ้งเตือนคุณภาพอากาศ!` |
| คุณภาพอากาศกลับปกติ | `✅ คุณภาพอากาศกลับสู่ปกติ` |

**Threshold ที่แก้ได้:**
```cpp
#define AQI_ALERT_THRESHOLD   3      // AQI >= 3 แจ้งเตือน
#define PM25_ALERT_THRESHOLD  35.0f  // PM2.5 >= 35 µg/m³ แจ้งเตือน
```

---

## การตั้งค่าในโปรแกรม

ไฟล์ [src/main.cpp](src/main.cpp):

```cpp
// NTP
#define NTP_SERVER  "pool.ntp.org"
#define TZ_OFFSET   25200              // UTC+7 Asia/Bangkok

// OpenWeatherMap
#define OWM_API_KEY          "your_api_key"
#define OWM_CITY             "Nakhon Si Thammarat"
#define OWM_COUNTRY          "TH"
#define WEATHER_INTERVAL_MS  (2UL * 60UL * 1000UL)

// Telegram
#define TG_BOT_TOKEN          "your_bot_token"
#define TG_CHAT_ID            "your_chat_id"
#define AQI_ALERT_THRESHOLD   3
#define PM25_ALERT_THRESHOLD  35.0f

// MQTT
#define MQTT_BROKER    "broker.hivemq.com"
#define MQTT_PORT      1883
#define BOARD_ID       "esp32_nst_01"   // ← เปลี่ยนให้ unique ต่อบอร์ด
#define MQTT_RECONNECT_MS  5000

// OLED
#define OLED_ADDRESS  0x3C

// Switch / Relay
#define DEBOUNCE_MS         20
#define WIFI_RESET_HOLD_MS  5000
```

---

## Serial Monitor Output

Baud rate: **115200**

```
[WiFi] Connected — IP: 192.168.1.105
[NTP] Syncing......
[NTP] 23/05/2026 14:35:22
[MQTT] Connecting as ESP32_esp32_nst_01_3f2a ...
[MQTT] Connected
[MQTT] Status: online
[MQTT] Published relay state
[Telegram] ส่งสำเร็จ
========== สภาพอากาศ นครศรีธรรมราช ==========
  อุณหภูมิ    : 32.4 °C (รู้สึกเหมือน 38.1 °C)
  ความชื้น    : 78 %
  ความเร็วลม  : 2.1 m/s
  สภาพ        : มีเมฆมาก
------------- คุณภาพอากาศ (AQI) ---------------
  AQI         : 2 (พอใช้)
  PM2.5       : 12.3 µg/m³
  PM10        : 18.7 µg/m³
================================================
[MQTT] Published weather
[Telegram] ส่งสำเร็จ
SW1 กด -> Relay1 ON
[MQTT] Published relay state
[Telegram] ส่งสำเร็จ
[MQTT] Received [esp32/esp32_nst_01/control/relay/2]: ON
[MQTT] Relay2 -> ON
[MQTT] Published relay state
```

---

## โครงสร้างไฟล์

```
ESP32-FirstProject-with-AI/
├── src/
│   └── main.cpp              # โปรแกรมหลัก
├── platformio.ini            # config board + library dependencies
├── ESP32DevkitBoard.md       # เอกสารอ้างอิง hardware และ wiring
├── README.md                 # ไฟล์นี้
└── .pio/                     # build cache (auto-generated)
    └── libdeps/              # library ที่ดาวน์โหลดอัตโนมัติ
```

**platformio.ini:**
```ini
[env:esp32doit-devkit-v1]
platform = espressif32
board = esp32doit-devkit-v1
framework = arduino
monitor_speed = 115200
lib_deps =
  bblanchon/ArduinoJson@^7.0.0
  adafruit/Adafruit SSD1306@^2.5.7
  adafruit/Adafruit GFX Library@^1.11.9
  tzapu/WiFiManager@^2.0.17
  knolleary/PubSubClient@^2.8
```

---

## ข้อควรระวัง

- **OLED ใช้ 3.3V เท่านั้น** — ต่อ 5V จะเสียหาย
- **GPIO 34, 35 เป็น Input-Only** — ต้องต่อ External Pull-up 10kΩ เสมอ
- **Relay Active Low** — LOW = เปิด, HIGH = ปิด
- **OpenWeatherMap Free** — limit 60 calls/นาที การดึงทุก 2 นาทีใช้ 2 calls ปลอดภัย
- **HiveMQ Free Broker** — ไม่มี auth, ไม่เข้ารหัส (port 1883) เหมาะกับการทดสอบ production ควรใช้ TLS port 8883
- **BOARD_ID ต้องไม่ซ้ำ** — ถ้ามีหลายบอร์ด topic จะชนกันและ relay จะถูกควบคุมพร้อมกัน
- **Bot Token / Chat ID / API Key** — เป็นข้อมูลลับ ไม่ควร commit ขึ้น git สาธารณะ
- **PubSubClient buffer** — default 256 bytes ถ้า payload ใหญ่ขึ้นให้เพิ่ม `mqtt.setBufferSize(512)`

---

## License

โปรเจคนี้พัฒนาเพื่อการศึกษาและใช้งาน IoT บน ESP32
