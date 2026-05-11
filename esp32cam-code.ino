/************ BLYNK CONFIG ************/
#define BLYNK_TEMPLATE_ID "paste your BLYNK_TEMPLATE_ID blynk  here"
#define BLYNK_TEMPLATE_NAME "IOT SMART ROOM MONITOR"
#define BLYNK_AUTH_TOKEN "paste your BLYNK_AUTH_TOKEN  here"

/************ LIBRARIES ************/
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <WiFiClientSecure.h>
#include <esp_camera.h>
#include <WebServer.h>
#include "DHT.h"

/************ WIFI ************/
char ssid[] = "YOUR wifi ID";
char pass[] = "YOUR WIFI PASSWORD";

/************ TELEGRAM ************/
#define BOT_TOKEN "Paste your BOT_TOKEN here"
#define CHAT_ID "Paste your CHAT_ID here"

/************ SENSOR PINS ************/
#define DHT_PIN 14
#define MQ2_PIN 13
#define FLAME_PIN 12
#define PIR_PIN 2
#define BUILTIN_LED 33

#define DHTTYPE DHT22
DHT dht(DHT_PIN, DHTTYPE);

/************ CAMERA PINS ************/
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

WebServer server(80);

unsigned long startupTime;
bool alertActive = false;

/************ CAMERA INIT ************/
void initCamera() {

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 15;
    config.fb_count = 1;
  }

  esp_camera_init(&config);
}

/************ STREAM SERVER ************/
void handleRoot() {
  server.send(200, "text/html",
              "<html><body><h2>Live Stream</h2><img src=\"/stream\"></body></html>");
}

void handleStream() {
  WiFiClient client = server.client();
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.sendContent(response);

  while (client.connected()) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) break;

    server.sendContent("--frame\r\n");
    server.sendContent("Content-Type: image/jpeg\r\n\r\n");
    server.sendContent((char*)fb->buf, fb->len);
    server.sendContent("\r\n");
    esp_camera_fb_return(fb);
  }
}

/************ TELEGRAM TEXT (POST FIXED) ************/
void sendTelegramMessage(String message) {

  WiFiClientSecure client;
  client.setInsecure();

  if (client.connect("api.telegram.org", 443)) {

    String payload = "chat_id=" + String(CHAT_ID) +
                     "&text=" + message;

    client.println("POST /bot" + String(BOT_TOKEN) + "/sendMessage HTTP/1.1");
    client.println("Host: api.telegram.org");
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.print("Content-Length: ");
    client.println(payload.length());
    client.println();
    client.print(payload);
  }
}

/************ TELEGRAM PHOTO ************/
void sendPhotoTelegram() {

  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) return;

  String boundary = "ESP32";
  String head = "--" + boundary + "\r\nContent-Disposition: form-data; name=\"chat_id\";\r\n\r\n" + String(CHAT_ID) + "\r\n";
  head += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"image.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
  String tail = "\r\n--" + boundary + "--\r\n";

  WiFiClientSecure client;
  client.setInsecure();

  if (client.connect("api.telegram.org", 443)) {
    client.println("POST /bot" + String(BOT_TOKEN) + "/sendPhoto HTTP/1.1");
    client.println("Host: api.telegram.org");
    client.println("Content-Type: multipart/form-data; boundary=" + boundary);
    client.print("Content-Length: ");
    client.println(head.length() + fb->len + tail.length());
    client.println();
    client.print(head);
    client.write(fb->buf, fb->len);
    client.print(tail);
  }

  esp_camera_fb_return(fb);
}

/************ SETUP ************/
void setup() {

  Serial.begin(115200);

  pinMode(MQ2_PIN, INPUT);
  pinMode(FLAME_PIN, INPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, HIGH);

  dht.begin();

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  initCamera();

  server.on("/", handleRoot);
  server.on("/stream", HTTP_GET, handleStream);
  server.begin();

  startupTime = millis();
}

/************ LOOP ************/
void loop() {

  Blynk.run();
  server.handleClient();

  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  if (!isnan(temp)) Blynk.virtualWrite(V0, temp);
  if (!isnan(hum)) Blynk.virtualWrite(V1, hum);

  bool gas = digitalRead(MQ2_PIN) == LOW;
  bool flame = digitalRead(FLAME_PIN) == LOW;
  bool motion = digitalRead(PIR_PIN) == HIGH;

  Blynk.virtualWrite(V2, gas);
  Blynk.virtualWrite(V3, flame);
  Blynk.virtualWrite(V4, motion);

  bool anyAlert = gas || flame || motion;

  if (millis() - startupTime > 60000) {

    if (anyAlert && !alertActive) {

      alertActive = true;
      digitalWrite(BUILTIN_LED, LOW);

      String ip = WiFi.localIP().toString();
      String message = "ALERT DETECTED!\nLive Stream:\nhttp://" + ip;

      sendTelegramMessage(message);
      sendPhotoTelegram();
    }

    if (!anyAlert) {
      alertActive = false;
      digitalWrite(BUILTIN_LED, HIGH);
    }
  }
}

