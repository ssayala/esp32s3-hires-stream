#include <Arduino.h>
#include <WiFi.h>
#include <esp_camera.h>
#include <esp_http_server.h>

#include "secrets.h"  // defines WIFI_SSID / WIFI_PASS; copy secrets.h.example



// Freenove ESP32-S3-WROOM (FNK0084) camera pinout.
constexpr int PIN_D0      = 11;
constexpr int PIN_D1      = 9;
constexpr int PIN_D2      = 8;
constexpr int PIN_D3      = 10;
constexpr int PIN_D4      = 12;
constexpr int PIN_D5      = 18;
constexpr int PIN_D6      = 17;
constexpr int PIN_D7      = 16;
constexpr int PIN_XCLK    = 15;
constexpr int PIN_PCLK    = 13;
constexpr int PIN_VSYNC   = 6;
constexpr int PIN_HREF    = 7;
constexpr int PIN_SIOD    = 4;
constexpr int PIN_SIOC    = 5;
constexpr int PIN_PWDN    = -1;
constexpr int PIN_RESET   = -1;

// UXGA (1600x1200) at quality 12 fits the ESP32-S3 Wi-Fi budget
// (~2-4 MB/s); QXGA at quality 10 saturated the link and stalled.
constexpr framesize_t STREAM_FRAMESIZE   = FRAMESIZE_UXGA;
constexpr int         STREAM_JPEG_QUALITY = 12;  // 0=best, 63=worst

static httpd_handle_t server = nullptr;

static const char INDEX_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html><head><title>esp32-cam</title>
<style>html,body{margin:0;background:#111}img{display:block;width:100vw;height:auto}</style>
</head><body><img src="/stream"></body></html>
)HTML";

static esp_err_t indexHandler(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t streamHandler(httpd_req_t* req) {
    static const char* BOUNDARY = "esp32cam";
    static const char* STREAM_CONTENT_TYPE =
        "multipart/x-mixed-replace;boundary=esp32cam";
    static const char* PART_HEADER =
        "--esp32cam\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";
    (void)BOUNDARY;

    if (httpd_resp_set_type(req, STREAM_CONTENT_TYPE) != ESP_OK) return ESP_FAIL;
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    char partHeader[80];
    while (true) {
        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        int n = snprintf(partHeader, sizeof(partHeader), PART_HEADER, (unsigned)fb->len);
        esp_err_t r = httpd_resp_send_chunk(req, partHeader, n);
        if (r == ESP_OK) r = httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);
        if (r == ESP_OK) r = httpd_resp_send_chunk(req, "\r\n", 2);
        esp_camera_fb_return(fb);
        if (r != ESP_OK) break;  // client disconnected
    }
    return ESP_OK;
}

static void startServer() {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;
    cfg.ctrl_port   = 32768;
    cfg.stack_size  = 8192;

    if (httpd_start(&server, &cfg) != ESP_OK) {
        Serial.println("httpd_start failed");
        return;
    }

    httpd_uri_t index_uri  = { "/",       HTTP_GET, indexHandler,  nullptr };
    httpd_uri_t stream_uri = { "/stream", HTTP_GET, streamHandler, nullptr };
    httpd_register_uri_handler(server, &index_uri);
    httpd_register_uri_handler(server, &stream_uri);
}

static bool initCamera() {
    camera_config_t c = {};
    c.pin_pwdn     = PIN_PWDN;
    c.pin_reset    = PIN_RESET;
    c.pin_xclk     = PIN_XCLK;
    c.pin_sccb_sda = PIN_SIOD;
    c.pin_sccb_scl = PIN_SIOC;
    c.pin_d0 = PIN_D0; c.pin_d1 = PIN_D1; c.pin_d2 = PIN_D2; c.pin_d3 = PIN_D3;
    c.pin_d4 = PIN_D4; c.pin_d5 = PIN_D5; c.pin_d6 = PIN_D6; c.pin_d7 = PIN_D7;
    c.pin_vsync    = PIN_VSYNC;
    c.pin_href     = PIN_HREF;
    c.pin_pclk     = PIN_PCLK;

    c.xclk_freq_hz = 20000000;
    c.ledc_timer   = LEDC_TIMER_0;
    c.ledc_channel = LEDC_CHANNEL_0;
    c.pixel_format = PIXFORMAT_JPEG;
    c.frame_size   = STREAM_FRAMESIZE;
    c.jpeg_quality = STREAM_JPEG_QUALITY;
    c.fb_count     = 2;
    c.fb_location  = CAMERA_FB_IN_PSRAM;
    c.grab_mode    = CAMERA_GRAB_LATEST;

    esp_err_t err = esp_camera_init(&c);
    if (err != ESP_OK) {
        Serial.printf("esp_camera_init failed: 0x%x\n", err);
        return false;
    }

    // OV3660 low-light tuning: BSI sensor tolerates higher gain cleanly,
    // and aec2/lenc are OV3660-only knobs that help in dim rooms.
    sensor_t* s = esp_camera_sensor_get();
    if (s) {
        s->set_gainceiling(s, GAINCEILING_8X);
        s->set_aec2(s, 1);
        s->set_lenc(s, 1);
        s->set_bpc(s, 1);
        s->set_wpc(s, 1);
        s->set_dcw(s, 1);
    }
    return true;
}

static void connectWifi() {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);  // keep latency down; the stream is always active
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.printf("Connecting to \"%s\"", WIFI_SSID);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print('.');
    }
    Serial.printf("\nIP: http://%s/\n", WiFi.localIP().toString().c_str());
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    if (!initCamera()) {
        while (true) delay(1000);
    }
    connectWifi();
    startServer();
}

void loop() {
    delay(1000);
}
