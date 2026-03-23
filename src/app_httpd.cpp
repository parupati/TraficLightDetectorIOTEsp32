#include <Arduino.h>
#include "esp_http_server.h"
#include "esp_camera.h"

static httpd_handle_t stream_httpd = NULL;
static httpd_handle_t camera_httpd = NULL;

// Shared detection state (updated from main loop)
volatile int g_detected = 0;      // 0=none, 1=red, 2=green
volatile int g_red_count = 0;
volatile int g_green_count = 0;
volatile int g_total_pixels = 0;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %d.%06d\r\n\r\n";

static const char index_html[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Signal Light Detector</title>
  <style>
    *{box-sizing:border-box}
    body{margin:0;background:#0d1117;color:#e6edf3;font-family:'Segoe UI',Arial,sans-serif;display:flex;flex-direction:column;align-items:center;min-height:100vh}
    h1{margin:18px 0 4px;font-size:1.3em;color:#58a6ff}
    .sub{font-size:0.8em;color:#8b949e;margin-bottom:14px}

    .main{display:flex;flex-wrap:wrap;justify-content:center;gap:20px;padding:0 16px;width:100%;max-width:1000px}
    .cam-box{flex:1;min-width:300px;max-width:640px}
    .cam-box img{width:100%;border:2px solid #30363d;border-radius:10px}

    .panel{flex:0 0 260px;display:flex;flex-direction:column;gap:14px}

    .signal-display{background:#161b22;border:2px solid #30363d;border-radius:14px;padding:20px;text-align:center}
    .signal-light{width:100px;height:100px;border-radius:50%;margin:10px auto;border:4px solid #30363d;transition:all 0.3s}
    .signal-light.red{background:radial-gradient(circle,#ff4444 30%,#cc0000 100%);box-shadow:0 0 30px #ff0000aa;border-color:#ff4444}
    .signal-light.green{background:radial-gradient(circle,#44ff44 30%,#00cc00 100%);box-shadow:0 0 30px #00ff00aa;border-color:#44ff44}
    .signal-light.none{background:radial-gradient(circle,#333 30%,#1a1a1a 100%);box-shadow:none}
    .signal-label{font-size:1.4em;font-weight:700;margin-top:8px;text-transform:uppercase;letter-spacing:2px}
    .signal-label.red{color:#ff4444}
    .signal-label.green{color:#44ff44}
    .signal-label.none{color:#8b949e}

    .stats{background:#161b22;border:2px solid #30363d;border-radius:14px;padding:16px}
    .stats h3{margin:0 0 10px;font-size:0.95em;color:#58a6ff}
    .stat-row{display:flex;justify-content:space-between;padding:4px 0;font-size:0.85em}
    .stat-row .label{color:#8b949e}
    .stat-row .val{font-weight:600}
    .stat-row .val.red{color:#ff4444}
    .stat-row .val.green{color:#44ff44}

    .bar-container{height:8px;background:#21262d;border-radius:4px;margin-top:6px;overflow:hidden;display:flex}
    .bar-red{background:#ff4444;transition:width 0.3s}
    .bar-green{background:#44ff44;transition:width 0.3s}

    @media(max-width:680px){.panel{flex:1 1 100%}}
  </style>
</head>
<body>
  <h1>Signal Light Detector</h1>
  <p class="sub">Freenove ESP32-S3 &mdash; Real-time Red/Green Detection</p>
  <div class="main">
    <div class="cam-box">
      <img id="stream" src="">
    </div>
    <div class="panel">
      <div class="signal-display">
        <div class="signal-light none" id="light"></div>
        <div class="signal-label none" id="label">SCANNING...</div>
      </div>
      <div class="stats">
        <h3>Detection Stats</h3>
        <div class="stat-row"><span class="label">Red pixels</span><span class="val red" id="rcount">0</span></div>
        <div class="stat-row"><span class="label">Green pixels</span><span class="val green" id="gcount">0</span></div>
        <div class="stat-row"><span class="label">Sampled</span><span class="val" id="tcount">0</span></div>
        <div class="bar-container">
          <div class="bar-red" id="rbar" style="width:0%"></div>
          <div class="bar-green" id="gbar" style="width:0%"></div>
        </div>
      </div>
    </div>
  </div>
  <script>
    window.onload = function(){
      document.getElementById('stream').src = window.location.protocol+'//'+window.location.hostname+':81/stream';
      setInterval(pollStatus, 500);
    }
    function pollStatus(){
      fetch('/status').then(r=>r.json()).then(d=>{
        var light=document.getElementById('light');
        var label=document.getElementById('label');
        light.className='signal-light '+(d.detected==1?'red':d.detected==2?'green':'none');
        label.className='signal-label '+(d.detected==1?'red':d.detected==2?'green':'none');
        label.textContent=d.detected==1?'RED SIGNAL':d.detected==2?'GREEN SIGNAL':'NO SIGNAL';
        document.getElementById('rcount').textContent=d.red;
        document.getElementById('gcount').textContent=d.green;
        document.getElementById('tcount').textContent=d.total;
        var sum=d.red+d.green||1;
        document.getElementById('rbar').style.width=(d.red/sum*100)+'%';
        document.getElementById('gbar').style.width=(d.green/sum*100)+'%';
      }).catch(()=>{});
    }
  </script>
</body>
</html>
)rawliteral";

static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, index_html, strlen(index_html));
}

static esp_err_t status_handler(httpd_req_t *req) {
    char buf[128];
    snprintf(buf, sizeof(buf),
        "{\"detected\":%d,\"red\":%d,\"green\":%d,\"total\":%d}",
        g_detected, g_red_count, g_green_count, g_total_pixels);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, buf, strlen(buf));
}

static esp_err_t capture_handler(httpd_req_t *req) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    esp_camera_fb_return(fb);
    return res;
}

static esp_err_t stream_handler(httpd_req_t *req) {
    esp_err_t res = ESP_OK;
    char part_buf[128];

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK) return res;

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "X-Framerate", "60");

    while (true) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) { res = ESP_FAIL; break; }

        struct timeval tv;
        gettimeofday(&tv, NULL);

        size_t hlen = snprintf(part_buf, sizeof(part_buf), _STREAM_PART, fb->len, (int)tv.tv_sec, (int)tv.tv_usec);
        res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        if (res == ESP_OK) res = httpd_resp_send_chunk(req, part_buf, hlen);
        if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);

        esp_camera_fb_return(fb);
        if (res != ESP_OK) break;
    }
    return res;
}

void startCameraServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 6;
    config.server_port = 80;

    httpd_uri_t index_uri   = {.uri = "/",        .method = HTTP_GET, .handler = index_handler};
    httpd_uri_t capture_uri = {.uri = "/capture",  .method = HTTP_GET, .handler = capture_handler};
    httpd_uri_t status_uri  = {.uri = "/status",   .method = HTTP_GET, .handler = status_handler};
    httpd_uri_t stream_uri  = {.uri = "/stream",   .method = HTTP_GET, .handler = stream_handler};

    if (httpd_start(&camera_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(camera_httpd, &index_uri);
        httpd_register_uri_handler(camera_httpd, &capture_uri);
        httpd_register_uri_handler(camera_httpd, &status_uri);
        Serial.printf("Web server started on port %d\n", config.server_port);
    }

    config.server_port = 81;
    config.ctrl_port += 1;
    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &stream_uri);
        Serial.printf("Stream server started on port %d\n", config.server_port);
    }
}
