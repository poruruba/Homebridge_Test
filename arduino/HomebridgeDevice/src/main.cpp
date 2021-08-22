#include <M5StickC.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#define LGFX_AUTODETECT
#include <LovyanGFX.hpp>

const char *wifi_ssid = "【WiFiアクセスポイントのSSID】"; // WiFiアクセスポイントのSSID
const char *wifi_password = "【WiFiアクセスポイントのパスワード】"; // WiFiアクセスポイントのパスワード
const char *MQTT_BROKER_URL = "【MQTTブローカーのホスト名】";
const char *MQTT_CLIENT_NAME = "Homebridge"; // MQTTサーバ接続時のクライアント名

#define PIN_LED 10
#define PIN_RGB 32
#define NUM_PIXEL 3
#define PIN_PIR 36
#define RGB_BRIGHTNESS  64
#define MQTT_BROKER_PORT 1883 // MQTTサーバのポート番号(TCP接続)
#define LCD_BRIGHTNESS 64     // LCDのバックライトの輝度(0～255)
#define MQTT_BUFFER_SIZE 255  // MQTT送受信のバッファサイズ

// 扱うトピック名
const char *MQTT_TOPIC_KEY_STARTUP = "key/startup";
const char *MQTT_TOPIC_KEY_SETLOCK = "key/setLockTargetState";
const char *MQTT_TOPIC_KEY_GETLOCK = "key/getLockCurrentState";
const char *MQTT_TOPIC_SWITCH_STARTUP = "switch/startup";
const char *MQTT_TOPIC_SWITCH_GETON = "switch/getOn";
const char *MQTT_TOPIC_SWITCH_SETON = "switch/setOn";
const char *MQTT_TOPIC_LIGHT_STARTUP = "light/startup";
const char *MQTT_TOPIC_LIGHT_SETON = "light/setOn";
const char *MQTT_TOPIC_LIGHT_GETON = "light/getOn";
const char *MQTT_TOPIC_LIGHT_SETHSV = "light/setHSV";
const char *MQTT_TOPIC_LIGHT_GETHSV = "light/getHSV";
const char *MQTT_TOPIC_GetMotionDetected = "motion/getMotionDetected";

// Subscribe対象のトピック
const char *topic_subscribe_list[] = {
  MQTT_TOPIC_KEY_STARTUP,
  MQTT_TOPIC_KEY_SETLOCK,
  MQTT_TOPIC_SWITCH_STARTUP,
  MQTT_TOPIC_SWITCH_SETON,
  MQTT_TOPIC_LIGHT_STARTUP,
  MQTT_TOPIC_LIGHT_SETON,
  MQTT_TOPIC_LIGHT_SETHSV
};

// LCDに表示する画像ファイル
extern const uint8_t lock_png_start[] asm("_binary_data_lock_png_start");
extern const uint8_t lock_png_end[] asm("_binary_data_lock_png_end");
extern const uint8_t unlock_png_start[] asm("_binary_data_unlock_png_start");
extern const uint8_t unlock_png_end[] asm("_binary_data_unlock_png_end");

// グローバル変数
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUM_PIXEL, PIN_RGB, NEO_GRB + NEO_KHZ800);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
static LGFX lcd; // for LovyanGFX

// 関数宣言
void wifi_connect(const char *ssid, const char *password);
long parse_uint16Array(const char *p_message, unsigned int length, uint16_t *p_array, int size);

// デバイスの状態を保持する変数
bool status_light_ON = false;
float status_light_hue = 1.0;
float status_light_saturation = 1.0;
float status_light_value = 1.0;
bool status_switch_ON = false;
bool status_switch_readonly = true;
int status_pir = 0;
bool status_key_lock = true;
bool status_key_readonly = false;

// デバイスの状態を処理する関数
void switch_viewUpdate(void){
  digitalWrite(PIN_LED, status_switch_ON ? LOW : HIGH);
}

void key_viewUpdate(void){
  if (status_key_lock )
    lcd.drawPng(lock_png_start, lock_png_end - lock_png_start);
  else
    lcd.drawPng(unlock_png_start, unlock_png_end - unlock_png_start);
}

void light_viewUpdate(void){
  uint32_t hsv;
  if( status_light_ON )
    hsv = pixels.ColorHSV(status_light_hue * 65535, status_light_saturation * 255, status_light_value * 255);
  else
    hsv = pixels.ColorHSV(0.0, 0.0, 0.0);

  for( int i = 0 ; i < NUM_PIXEL ; i++ )
    pixels.setPixelColor(i, hsv);
  pixels.show();
}

void light_publishOnOff(void){
  mqttClient.publish(MQTT_TOPIC_LIGHT_GETON, status_light_ON ? "true" : "false");
}

void light_publish(void){
  String hsv_str = "";
  hsv_str += round(status_light_hue * 360);
  hsv_str += ",";
  hsv_str += round(status_light_saturation * 100);
  hsv_str += ",";
  hsv_str += round(status_light_value * 100);
  mqttClient.publish(MQTT_TOPIC_LIGHT_GETHSV, hsv_str.c_str());
}

void key_publish(void){
  mqttClient.publish(MQTT_TOPIC_KEY_GETLOCK, status_key_lock ? "S" : "U");
}

void switch_publishOnOff(void){
  mqttClient.publish(MQTT_TOPIC_SWITCH_GETON, status_switch_ON ? "true" : "false");
}

void motion_notify(void){
  mqttClient.publish(MQTT_TOPIC_GetMotionDetected, (status_pir != 0) ? "true" : "false");
}

// MQTTメッセージ受信後の処理
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("mqtt_callback");
  Serial.println(topic);
//  Serial.println((char*)payload);
//  Serial.println(length);

  if( strcmp(topic, MQTT_TOPIC_LIGHT_STARTUP) == 0 ){
    light_publishOnOff();
    light_publish();
  }else
  if( strcmp(topic, MQTT_TOPIC_KEY_STARTUP) == 0 ){
    key_publish();
  }else
  if( strcmp(topic, MQTT_TOPIC_SWITCH_STARTUP) == 0 ){
    switch_publishOnOff();
  }else
  if (strcmp(topic, MQTT_TOPIC_KEY_SETLOCK) == 0 ){
    if( !status_key_readonly )
      status_key_lock = (*payload == 'S') ? true : false;
    key_viewUpdate();
    key_publish();
  }else
  if (strcmp(topic, MQTT_TOPIC_SWITCH_SETON) == 0){
    if( !status_switch_readonly ){
      if (strncmp((char *)payload, "true", 4) == 0)
        status_switch_ON = true;
      else
        status_switch_ON = false;
    }
    switch_publishOnOff();
  }else
  if (strcmp(topic, MQTT_TOPIC_LIGHT_SETON) == 0){
    if (strncmp((char *)payload, "true", 4) == 0)
      status_light_ON = true;
    else
      status_light_ON = false;

    light_viewUpdate();
    light_publishOnOff();
  }
  else if (strcmp(topic, MQTT_TOPIC_LIGHT_SETHSV) == 0){
    uint16_t array[3];
    long ret = parse_uint16Array((const char*)payload, length, array, 3);
    if( ret != 0 ){
      Serial.println("parse_uint16Array error");
      return;
    }

    status_light_hue = array[0] / 360.0;
    status_light_saturation = array[1] / 100.0;
    status_light_value = array[2] / 100.0;

    Serial.printf("h=%f, s=%f, v=%f\n", status_light_hue, status_light_saturation, status_light_value);

    light_viewUpdate();
    light_publish();
  }
}

void setup(){
  // put your setup code here, to run once:
  M5.begin();
  M5.Axp.begin();

  lcd.init(); // M5StickCのLCDの初期化
  lcd.setBrightness(LCD_BRIGHTNESS);
  lcd.setRotation(0);
  lcd.fillScreen(BLACK);

  wifi_connect(wifi_ssid, wifi_password);

  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_PIR, INPUT_PULLUP);
  pixels.begin();
  pixels.setBrightness(RGB_BRIGHTNESS);

  light_viewUpdate();
  key_viewUpdate();
  switch_viewUpdate();

  // バッファサイズの変更
  mqttClient.setBufferSize(MQTT_BUFFER_SIZE);
  // MQTTコールバック関数の設定
  mqttClient.setCallback(mqtt_callback);
  // MQTTブローカに接続
  mqttClient.setServer(MQTT_BROKER_URL, MQTT_BROKER_PORT);
}

void loop() {
  // put your main code here, to run repeatedly:
  M5.update();
  mqttClient.loop();
  // MQTT未接続の場合、再接続
  while (!mqttClient.connected()){
    Serial.println("Mqtt Reconnecting");
    if (mqttClient.connect(MQTT_CLIENT_NAME)){
      for (int i = 0; i < sizeof(topic_subscribe_list) / sizeof(const char *); i++)
        mqttClient.subscribe(topic_subscribe_list[i]);

      switch_publishOnOff();
      key_publish();
      light_publishOnOff();
      light_publish();

      break;
    }

    delay(1000);
  }

  if( M5.BtnA.wasReleased() ){
    Serial.println("BtnA pressed");

    // 鍵の状態のトグル
    status_key_lock = !status_key_lock;

    key_viewUpdate();
    key_publish();

    delay(10);
  }
  if( M5.BtnB.wasReleased() ){
    Serial.println("BtnB pressed");

    // スイッチの状態のトグル
    status_switch_ON = !status_switch_ON;

    switch_viewUpdate();
    switch_publishOnOff();

    delay(10);
  }

  // モーションセンサーの読み出し
  int pir = digitalRead(PIN_PIR);
  if( status_pir != pir ){
    Serial.println("PIR changed");

    // 動き検出状態のトグル
    status_pir = pir;

    motion_notify();
  }

  delay(1);
}

// WiFiアクセスポイントへの接続
void wifi_connect(const char *ssid, const char *password)
{
  Serial.println("");
  Serial.print("WiFi Connenting");
  lcd.println("WiFi Connecting");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    lcd.print(".");
    delay(1000);
  }
  Serial.println("Connected");
  Serial.println(WiFi.localIP());
}

// カンマ(,)区切りの10進数の解析
long parse_uint16Array(const char *p_message, unsigned int length, uint16_t *p_array, int size){
  char temp[6];
  const char *p_str = p_message;
  for( int i = 0 ; i < size - 1 ; i++ ){
    char *ptr = strchr(p_str, ',');
    if (ptr == NULL)
      return -1;

    int len = ptr - p_str;
    if (len > sizeof(temp) - 1)
      return -2;

    memset(temp, '\0', sizeof(temp));
    memmove(temp, p_str, len);
    p_array[i] = atoi(temp);
    ptr++;
    p_str = ptr;
  }
  int len = length - (p_str - (char *)p_message);
  if (len > sizeof(temp) - 1)
    return -3;

  memset(temp, '\0', sizeof(temp));
  memmove(temp, p_str, len);
  p_array[size - 1] = atoi(temp);

  return 0;
}
