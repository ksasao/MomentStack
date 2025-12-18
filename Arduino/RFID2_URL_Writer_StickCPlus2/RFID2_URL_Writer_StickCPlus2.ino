// RFID URL Writer for M5Stack RFID 2 Unit (WS1850S/MFRC522 I2C) by ksasao
// M5 StickC Plus2
// Writes a Tag single URI record (https://m5stack.com/) to an NFC formatted tag. Note this erases all existing records.
// forked from NDEF Library for Arduino by TheNitek https://github.com/TheNitek/NDEF (BSD License)
//             RFID_RC522 by M5Stack https://github.com/m5stack/M5Stack/tree/master/examples/Unit/RFID_RC522 (MIT license)
// (Board) M5Stack 2.1.3 でビルドを確認
// PlusからPlus2への移植は https://lang-ship.com/blog/work/m5stack-m5stickc-plus2-2/ が大変参考になりました
#include <M5GFX.h>
#include <M5StickCPlus2.h>
#include <Preferences.h>
#include "MFRC522_I2C.h"
#include "NfcAdapter.h"

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <sntp.h>
#include "index.html.h"

#define NTP_TIMEZONE  "JST-9"           // https://github.com/esp8266/Arduino/blob/master/cores/esp8266/TZ.h
#define NTP_SERVER1   "0.pool.ntp.org"
#define NTP_SERVER2   "1.pool.ntp.org"
#define NTP_SERVER3   "2.pool.ntp.org"

const char* ssid = "your-ssid";
const char* password = "your-password";
WebServer server(80);
char localUrl[256];

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 9 * 3600; // GMT+9 for Japan Standard Time 
const int daylightOffset_sec = 0;

String posString = "@35.681684,139.786917,17z";
String textString = "Hello";

Preferences preferences;

MFRC522 mfrc522(0x28); // Create MFRC522 instance
char str[256];
char date_str[64];
char time_str[64];
char label_str[64];
M5GFX display;

int reqCount = 0;

void handleRoot() {
    if (server.method() == HTTP_GET) {
       posString = server.arg("p");
       textString = server.arg("t");
       Serial.println(posString);
       Serial.println(textString);
       
       String mes = html;
       server.send(200, "text/html", mes);

       if(reqCount>0){
         // 地図情報の更新
         preferences.begin("mapcard", false);
         preferences.putString("pos",posString);
         preferences.putString("text",textString);
         preferences.end();
         drawText((char*)textString.c_str());
         delay(2000);
         ESP.restart();
       }
       reqCount++;
    }
}

void handleNotFound() {
 String message = "404 File Not Found.\n\n";
 server.send(404, "text/plain", message);
}

void updateTime(){
  struct tm timeinfo; 
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return; 
  } 
  m5::rtc_time_t TimeStruct; 
  TimeStruct.hours = timeinfo.tm_hour;
  TimeStruct.minutes = timeinfo.tm_min;
  TimeStruct.seconds = timeinfo.tm_sec;
  M5.Rtc.setTime(&TimeStruct);
  m5::rtc_date_t DateStruct;
  DateStruct.month = timeinfo.tm_mon + 1;
  DateStruct.date = timeinfo.tm_mday;
  DateStruct.year = timeinfo.tm_year + 1900;
  M5.Rtc.setDate(&DateStruct);
  getDate(date_str);
  getTime(time_str);
}
void startWebServer(){
  drawText("Config Mode");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      M5.Lcd.print(".");
  }
  Serial.print("WiFi connected.");
  // Obtain time from NTP 
  
  configTzTime(NTP_TIMEZONE, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);
  while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {
    // NTP同期待ち
    M5.Log.print(".");
    delay(1000);
  }
  M5.Log.println("NTP Connected.");
  updateTime();
  M5.Log.println("RTC updated");

  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();

  M5.Log.println("Web server started");

  sprintf(localUrl,"http://%s/?p=%s&t=%s&edit=t",WiFi.localIP().toString().c_str(),posString.c_str(),textString.c_str());
  M5.Display.clear(0);
  M5.Display.qrcode(localUrl,10,8,120,2);
}


NfcAdapter nfc = NfcAdapter(&mfrc522);

void drawText(char* line0,char* line1,char* line2){
  M5.Log.println(line0);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextWrap(true);
  M5.Display.setTextDatum(textdatum_t::top_left);
  M5.Display.setTextSize(0.75,1);
  M5.Display.setFont(&fonts::lgfxJapanGothic_40);

  M5.Display.clear();
  M5.Display.setCursor(0,0);
  M5.Display.println(line0);
  M5.Display.println(line1);
  M5.Display.println(line2);
}
void drawText(char* text){
  drawText(text,"","");
}
void getBattery(char* str){
  sprintf(str,"%d %%",M5.Power.getBatteryLevel());
}
void getDate(char *str){
      // Date
      struct m5::rtc_date_t rtc_date;
      M5.Rtc.getDate(&rtc_date);
      int year  = rtc_date.year;    // 年
      int month = rtc_date.month;   // 月
      int date  = rtc_date.date;    // 日
      sprintf(str,"%d-%02d-%02d",year,month,date);
}
void getTime(char *str){
      struct m5::rtc_time_t rtc_time;
      M5.Rtc.getTime(&rtc_time);
      int hours   = rtc_time.hours;   // 時
      int minute = rtc_time.minutes; // 分
      //int second = rtc_time.Seconds; // 秒
      sprintf(str, "%02d:%02d",hours,minute);
}

long lastUpdate = millis();

void loadPreference(){
  preferences.begin("mapcard", false);
  posString = preferences.getString("pos",posString);
  textString = preferences.getString("text",textString);
  preferences.end();
  Serial.println(posString);
  Serial.println(textString);
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.delay(500);
  loadPreference();
  Wire.begin();

  M5.Log.println("NDEF writer\nPlace a formatted Mifare Classic or Ultralight NFC tag on the reader.");
  //setDate();
  mfrc522.PCD_Init(); // Init MFRC522
  nfc.begin();

  M5.Display.init();
  M5.Display.setRotation(1);
  getDate(date_str);
  getTime(time_str);
  getBattery(str);

  drawText(date_str,time_str,str);
}

int tagCheckCount = 0;
int state = 0;

void nfcWriter(void){
    long now = millis();
    if (now - lastUpdate > 30 * 1000){
      M5.Power.powerOff();
    }
    // RFIDタグへのアクセス頻度を減らす
    if(tagCheckCount<10){
      tagCheckCount++;
      return;
    }
    tagCheckCount = 0;
    
    bool tagPresent = nfc.tagPresent();
    if (tagPresent) {
      lastUpdate = now;
      // Show Nfc Tag type
      byte piccType = mfrc522.PICC_GetType((&mfrc522.uid)->sak);
      Serial.print("PICC type: ");
      Serial.println(mfrc522.PICC_GetTypeName(piccType));

      // Show Uid
      NfcTag tag = nfc.read();
      Serial.print("UID      : ");
      Serial.println(tag.getUidString());

      String url_pre = String("https://gist.githack.com/ksasao/bc5fc05d0676f3ec07cf8666e8236c8f/raw/?") 
        + String("p=") + posString + String("&t=");
      getDate(date_str);
      getTime(time_str);
      drawText(date_str,time_str,(char*)textString.c_str());
      snprintf(str, sizeof(str), "%s%s %s<br>%s", url_pre.c_str(), date_str, time_str, textString.c_str());
      Serial.printf("Writing record to NFC tag: %s\n",str);
      NdefMessage message = NdefMessage();
      message.addUriRecord(str);
      bool success = nfc.write(message);
      if (success) {
        M5.Speaker.tone(4000,50);
        drawText("Success. ");
        delay(2000);
      } else {
        M5.Speaker.tone(4000,500);
        drawText("Failed.");
      }
    }
}


void loop() {
    M5.update();
    if (M5.BtnA.wasReleased()) {
        state = 1;
        startWebServer();
    } 

    switch(state){
      case 1:
        server.handleClient();
        break;
      default:
        nfcWriter();
        break;
    }
    delay(100);
}
