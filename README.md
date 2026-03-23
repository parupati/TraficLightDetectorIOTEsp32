# Traffic Light Signal Detector - IoT ESP32-S3

Real-time traffic signal light detection using the **Freenove ESP32-S3 WROOM** board with an onboard OV2640 camera. The system captures camera frames, analyzes pixel colors in HSV color space, identifies red and green signal lights, and provides visual feedback through both an onboard WS2812 RGB LED and a web-based dashboard accessible from any browser on the same network.

Includes both **C++ (Arduino/PlatformIO)** and **MicroPython** implementations of the same functionality.

---

## How It Works

```
OV2640 Camera
  --> Capture frame in RGB565 (320x240)
  --> Convert each sampled pixel to HSV color space
  --> Count pixels in red hue range (0-12, 168-179) and green hue range (35-85)
  --> Filter: only bright (V > 80) and saturated (S > 80) pixels qualify
  --> If red pixels > threshold and > green pixels --> RED detected
  --> If green pixels > threshold and > red pixels --> GREEN detected
  --> Update onboard LED + web dashboard
  --> Switch back to JPEG for live stream
  --> Repeat ~3-5 times per second
```

### Detection Algorithm

The detection uses **HSV (Hue-Saturation-Value)** color space instead of RGB because it separates color information (hue) from brightness, making it robust under varying lighting conditions.

| Color | Hue Range | Saturation | Value |
|-------|-----------|------------|-------|
| Red | 0-12 or 168-179 | > 80 | > 80 |
| Green | 35-85 | > 80 | > 80 |

- Pixels are sampled every 2nd (C++) or 3rd (MicroPython) pixel for performance
- A minimum threshold of **0.5% of sampled pixels** must match to trigger detection
- The color with more matching pixels wins when both are present

### Onboard LED Feedback

| LED Color | Meaning |
|-----------|---------|
| Blue (dim) | No signal detected / scanning |
| Red | Red signal light detected |
| Green | Green signal light detected |
| Orange | WiFi connection failed |
| Blue (bright) | Starting up |

---

## Hardware

| Component | Details |
|-----------|---------|
| Board | [Freenove ESP32-S3 WROOM](https://store.freenove.com/products/fnk0085) |
| MCU | ESP32-S3 dual-core 240 MHz, 8MB Flash, PSRAM |
| Camera | OV2640, connected via ribbon cable |
| RGB LED | WS2812 NeoPixel on GPIO 48 |
| Connection | USB-C |

---

## Project Structure

```
OperationRainbow/
|-- src/                        # C++ implementation (Arduino/PlatformIO)
|   |-- main.cpp                # Camera init, WiFi, detection loop, NeoPixel control
|   |-- app_httpd.cpp           # Web server: live MJPEG stream + status API + dashboard
|-- include/
|   |-- camera_pins.h           # Freenove ESP32-S3 OV2640 GPIO pin definitions
|   |-- color_detect.h          # HSV color detection algorithm (C)
|-- micropython/                # MicroPython implementation
|   |-- main.py                 # Camera init, WiFi, detection loop, NeoPixel control
|   |-- color_detect.py         # HSV color detection algorithm (Python)
|   |-- web_server.py           # HTTP server with dashboard + capture + status API
|-- platformio.ini              # PlatformIO build configuration
|-- .gitignore
```

---

## Web Dashboard

Both implementations serve a web dashboard accessible at `http://<board-ip>/` from any browser on the same WiFi network.

**Features:**
- Live camera feed (MJPEG stream in C++, periodic JPEG capture in MicroPython)
- Virtual signal light indicator that mirrors the physical LED
- Real-time pixel count stats (red count, green count, total sampled)
- Red/green ratio bar visualization
- Auto-refreshing (500ms polling in C++, 800ms in MicroPython)

**Endpoints:**

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Web dashboard with camera feed and detection status |
| `/capture` | GET | Single JPEG snapshot |
| `/stream` | GET | Live MJPEG stream (C++ only, port 81) |
| `/status` | GET | JSON detection state: `{"detected":1,"red":457,"green":0,"total":8560}` |

---

## Setup - C++ (PlatformIO)

### Prerequisites

- [VS Code](https://code.visualstudio.com/) with [PlatformIO IDE](https://platformio.org/install/ide?install=vscode) extension
- Freenove ESP32-S3 WROOM board connected via USB

### Steps

1. Clone the repository:
   ```bash
   git clone https://github.com/parupati/TraficLightDetectorIOTEsp32.git
   cd TraficLightDetectorIOTEsp32
   ```

2. Open the project in VS Code (PlatformIO will auto-detect it)

3. Edit WiFi credentials in `src/main.cpp`:
   ```cpp
   const char *ssid     = "YOUR_WIFI_SSID";
   const char *password = "YOUR_WIFI_PASSWORD";
   ```

4. Click the **Upload** button in the PlatformIO toolbar (or `Ctrl+Alt+U`)

5. Open **Serial Monitor** (plug icon in bottom bar) at 115200 baud

6. Press **RST** on the board -- the serial output will display the IP address

7. Open `http://<ip-address>` in your browser

### Build Configuration

```ini
[env:esp32-s3-devkitm-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
lib_deps = adafruit/Adafruit NeoPixel@^1.12.0
board_build.arduino.memory_type = qio_opi
board_build.flash_mode = qio
board_upload.flash_size = 8MB
board_build.partitions = default_8MB.csv
build_flags =
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_CDC_ON_BOOT=0
```

---

## Setup - MicroPython

### Prerequisites

- Python 3.x with `mpremote` installed (`pip install mpremote`)
- MicroPython firmware with camera support

### Steps

1. Download the Freenove ESP32-S3 camera firmware from [micropython-camera-API releases](https://github.com/cnadler86/micropython-camera-API/releases) (file: `mpy_cam-v1.27.0-FREENOVE_ESP32S3_CAM.zip`)

2. Flash the firmware using [ESP Web Flasher](https://esp.huhn.me/):
   - Connect the board via USB
   - Open the web flasher in Chrome
   - Set flash address to `0x0`
   - Select the `firmware.bin` file
   - Click Program

   Or use esptool:
   ```bash
   pip install esptool
   esptool --chip esp32s3 --port COM8 erase_flash
   esptool --chip esp32s3 --port COM8 write_flash 0x0 firmware.bin
   ```

3. Edit WiFi credentials in `micropython/main.py`:
   ```python
   SSID = "YOUR_WIFI_SSID"
   PASSWORD = "YOUR_WIFI_PASSWORD"
   ```

4. Upload the Python files:
   ```bash
   mpremote connect COM8 cp micropython/color_detect.py :color_detect.py
   mpremote connect COM8 cp micropython/web_server.py :web_server.py
   mpremote connect COM8 cp micropython/main.py :main.py
   mpremote connect COM8 reset
   ```

5. The board will auto-run `main.py` on boot and print the IP address to the serial console

### Switching Between C++ and MicroPython

The two implementations are independent. Flashing one completely replaces the other on the board:

- **To use C++:** Upload via PlatformIO (overwrites MicroPython firmware)
- **To use MicroPython:** Flash the MicroPython firmware + upload `.py` files (overwrites C++ firmware)

Your source files on the PC are never affected -- both coexist in the same project folder.

---

## Camera Pin Configuration

Freenove ESP32-S3 WROOM uses the `ESP32S3_EYE` camera pinout:

| Function | GPIO |
|----------|------|
| XCLK | 15 |
| SDA (SIOD) | 4 |
| SCL (SIOC) | 5 |
| D0 (Y2) | 11 |
| D1 (Y3) | 9 |
| D2 (Y4) | 8 |
| D3 (Y5) | 10 |
| D4 (Y6) | 12 |
| D5 (Y7) | 18 |
| D6 (Y8) | 17 |
| D7 (Y9) | 16 |
| VSYNC | 6 |
| HREF | 7 |
| PCLK | 13 |
| PWDN | -1 (not used) |
| RESET | -1 (not used) |

---

## Tech Stack

| Layer | C++ | MicroPython |
|-------|-----|-------------|
| Framework | Arduino (ESP-IDF) | MicroPython 1.27.0 |
| IDE | PlatformIO + VS Code | Any text editor + mpremote |
| Camera Driver | esp_camera (built-in) | micropython-camera-API |
| LED Control | Adafruit NeoPixel | neopixel (built-in) |
| Web Server | esp_http_server | socket (built-in) |
| Stream | MJPEG multipart (real-time) | Periodic JPEG capture |
| Detection Speed | ~3 fps | ~5 fps |
| Detection Resolution | 320x240 (every 2nd pixel) | 320x240 (every 3rd pixel) |

---

## Troubleshooting

**Board not detected on COM port:**
- Try a different USB cable (some are charge-only)
- Hold the **BOOT** button while plugging in USB, then release

**Camera init failed:**
- Check the ribbon cable is fully seated and the connector latch is closed
- Ensure PSRAM is enabled in build flags

**WiFi won't connect:**
- Verify SSID and password (case-sensitive)
- ESP32-S3 supports 2.4 GHz WiFi only, not 5 GHz
- Move the board closer to the router

**Serial Monitor shows only bootloader output:**
- Make sure `ARDUINO_USB_CDC_ON_BOOT=0` is set in platformio.ini
- Try pressing **RST** after opening the Serial Monitor

**MicroPython SyntaxError on boot:**
- Files may have a UTF-8 BOM -- re-save without BOM or strip it:
  ```python
  [System.IO.File]::WriteAllText("main.py", [System.IO.File]::ReadAllText("main.py"), (New-Object System.Text.UTF8Encoding $false))
  ```﻿