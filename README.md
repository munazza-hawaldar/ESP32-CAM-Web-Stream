# ESP32-CAM Web Stream

A WiFi camera built on the AI Thinker ESP32-CAM. It serves a small web page that shows a **live video stream**, lets you **capture and download photos**, and **controls the onboard flash LED** from any browser on the same network.

![ESP32-CAM web page result](images/web-stream.jpg)

*Photo captured through the web page (face blurred for privacy).* Full write-up: [PROJECT_REPORT.md](PROJECT_REPORT.md)

## Features

- Live MJPEG video stream in the browser
- One-click photo capture with preview and download
- Flash LED ON/OFF control from the web page
- Automatic WiFi reconnect and WiFi power-save disabled for a steadier stream
- No extra components needed, just the ESP32-CAM board

## Hardware

| Component | Details |
|---|---|
| Board | AI Thinker ESP32-CAM (OV2640 camera) |
| Programmer | ESP32-CAM-MB (CH340 USB-to-serial base board) |
| Cable | Micro-USB data cable |
| Power | 5V from a laptop USB port or a 5V 2A adapter |

## Software

- Arduino IDE with the **ESP32 board package** (Espressif) installed
- Libraries: only the ones bundled with the ESP32 package (`esp_camera`, `WiFi`, `esp_http_server`)

## How to run

1. Clone or download this repository and open `esp32cam_web_stream.ino` in Arduino IDE.
2. Edit the WiFi details near the top of the sketch:
   ```cpp
   const char* WIFI_SSID     = "YOUR_WIFI_NAME";
   const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
   ```
   Use a **2.4 GHz** network. The ESP32 does not support 5 GHz.
3. Select **Tools > Board > AI Thinker ESP32-CAM** and the correct COM port.
4. Mount the ESP32-CAM on the ESP32-CAM-MB board, connect it to your computer and click **Upload**.
5. Open **Serial Monitor** at **115200 baud** and press the RST button on the board.
6. Open the IP address printed there (for example `http://192.168.1.x`) in a browser on the same WiFi network.

## Endpoints

| URL | Purpose |
|---|---|
| `http://<ip>/` | Web page with stream, capture and flash controls |
| `http://<ip>/capture` | Returns a single JPEG photo |
| `http://<ip>/flash?on=1` or `on=0` | Turns the flash LED on or off |
| `http://<ip>:81/stream` | MJPEG video stream |

## How it works

- The camera is initialised in JPEG mode (VGA 640x480 when PSRAM is available, QVGA otherwise).
- Two HTTP servers run on the ESP32: port 80 for the page, capture and flash, and port 81 for the continuous stream, so a long-running stream does not block the other requests.
- The flash LED is driven directly from GPIO 4.

## Troubleshooting

- **Upload fails:** check that the board is seated properly on the MB board and try a different data cable.
- **Camera init failed:** reseat the camera ribbon cable and make sure its latch is closed.
- **Random resets or WiFi drops:** power the board from a direct USB port or a 5V 2A adapter.
- **Only dots in Serial Monitor:** recheck the WiFi name and password, and make sure the network is 2.4 GHz.
- **Page does not open:** make sure your computer and the ESP32-CAM are on the same network.

## Possible improvements

- Motion detection with Telegram photo alerts
- Face detection and recognition
- Adding a DHT11 sensor to show temperature and humidity on the page

## Author

**Munazza Hawaldar**
Diploma in Electronics & Telecommunication Engineering
[LinkedIn](https://www.linkedin.com/in/munazza-hawaldar-644b0041a/) | [GitHub](https://github.com/munazza-hawaldar)
