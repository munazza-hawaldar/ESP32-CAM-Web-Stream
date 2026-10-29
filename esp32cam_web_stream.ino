/*
  ESP32-CAM Live Stream + Photo Capture + Flash Control
  Board: AI Thinker ESP32-CAM (with ESP32-CAM-MB programmer)

  Features:
  - Live MJPEG video stream in the browser
  - "Take Photo" button (preview + download)
  - Flash LED ON/OFF button

  Setup: change WIFI_SSID and WIFI_PASSWORD below, then upload.
  Open Serial Monitor at 115200 baud to see the IP address.
*/

#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

// ===== WiFi details (CHANGE THESE) =====
const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// ===== AI Thinker ESP32-CAM pins =====
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#define FLASH_LED_PIN      4   // onboard white flash LED

httpd_handle_t web_httpd    = NULL;  // port 80: page, capture, flash
httpd_handle_t stream_httpd = NULL;  // port 81: video stream

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* STREAM_BOUNDARY     = "\r\n--" PART_BOUNDARY "\r\n";
static const char* STREAM_PART         = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

bool flashOn = false;

// ===== Web page =====
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32-CAM Live</title>
  <style>
    body { font-family: Arial, sans-serif; background:#0f172a; color:#e2e8f0; margin:0; padding:16px; text-align:center; }
    h1 { font-size:20px; margin:8px 0 16px; }
    img { max-width:100%; border-radius:8px; background:#1e293b; }
    .btns { margin:16px 0; }
    button, a.dl { background:#2563eb; color:#fff; border:0; padding:10px 18px; margin:4px; border-radius:6px; font-size:15px; cursor:pointer; text-decoration:none; display:inline-block; }
    button.off { background:#475569; }
    #photoBox { display:none; margin-top:16px; }
  </style>
</head>
<body>
  <h1>ESP32-CAM Live Stream</h1>
  <img id="stream" alt="Live stream">
  <div class="btns">
    <button onclick="takePhoto()">Take Photo</button>
    <button id="flashBtn" class="off" onclick="toggleFlash()">Flash: OFF</button>
  </div>
  <div id="photoBox">
    <h3>Captured photo</h3>
    <img id="photo" alt="Captured photo"><br>
    <a id="dl" class="dl" download="esp32cam_photo.jpg">Download</a>
  </div>

  <script>
    document.getElementById('stream').src = 'http://' + location.hostname + ':81/stream';

    let flash = false;
    function toggleFlash() {
      flash = !flash;
      fetch('/flash?on=' + (flash ? 1 : 0)).then(() => {
        const b = document.getElementById('flashBtn');
        b.textContent = 'Flash: ' + (flash ? 'ON' : 'OFF');
        b.className = flash ? '' : 'off';
      });
    }

    function takePhoto() {
      fetch('/capture?t=' + Date.now())
        .then(r => r.blob())
        .then(blob => {
          const url = URL.createObjectURL(blob);
          document.getElementById('photo').src = url;
          document.getElementById('dl').href = url;
          document.getElementById('photoBox').style.display = 'block';
        });
    }
  </script>
</body>
</html>
)rawliteral";

// ===== Handlers =====
static esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t capture_handler(httpd_req_t *req) {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=photo.jpg");
  esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  return res;
}

static esp_err_t flash_handler(httpd_req_t *req) {
  char query[16];
  char value[4];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK &&
      httpd_query_key_value(query, "on", value, sizeof(value)) == ESP_OK) {
    flashOn = (value[0] == '1');
    digitalWrite(FLASH_LED_PIN, flashOn ? HIGH : LOW);
  }
  httpd_resp_set_type(req, "text/plain");
  return httpd_resp_send(req, flashOn ? "ON" : "OFF", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t stream_handler(httpd_req_t *req) {
  esp_err_t res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  char part_buf[64];
  while (true) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      res = ESP_FAIL;
    } else {
      size_t hlen = snprintf(part_buf, sizeof(part_buf), STREAM_PART, fb->len);
      res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
      if (res == ESP_OK) res = httpd_resp_send_chunk(req, part_buf, hlen);
      if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
      esp_camera_fb_return(fb);
    }
    if (res != ESP_OK) break;  // client disconnected
  }
  return res;
}

void startServers() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;

  httpd_uri_t index_uri   = { "/",        HTTP_GET, index_handler,   NULL };
  httpd_uri_t capture_uri = { "/capture", HTTP_GET, capture_handler, NULL };
  httpd_uri_t flash_uri   = { "/flash",   HTTP_GET, flash_handler,   NULL };

  if (httpd_start(&web_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(web_httpd, &index_uri);
    httpd_register_uri_handler(web_httpd, &capture_uri);
    httpd_register_uri_handler(web_httpd, &flash_uri);
  }

  config.server_port = 81;
  config.ctrl_port   = 32769;  // must differ from the first server

  httpd_uri_t stream_uri = { "/stream", HTTP_GET, stream_handler, NULL };
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk  = XCLK_GPIO_NUM;
  config.pin_pclk  = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href  = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn  = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size   = FRAMESIZE_VGA;  // 640x480, smooth stream
    config.jpeg_quality = 12;
    config.fb_count     = 2;
  } else {
    config.frame_size   = FRAMESIZE_QVGA;
    config.jpeg_quality = 15;
    config.fb_count     = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);          // disable WiFi power-save for a stable stream
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Open in browser: http://");
  Serial.println(WiFi.localIP());

  startServers();
}

void loop() {
  // The web servers run in their own tasks; here we only watch the WiFi link.
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost, reconnecting...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    delay(5000);
  }
  delay(5000);
}
