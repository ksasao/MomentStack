// RFID URL Writer for M5Stack RFID 2 Unit (WS1850S/MFRC522 I2C) by ksasao
// M5 StickC Plus2
// Writes a Tag single URI record (https://m5stack.com/) to an NFC formatted tag. Note this erases all existing records.
// forked from NDEF Library for Arduino by TheNitek https://github.com/TheNitek/NDEF (BSD License)
//             RFID_RC522 by M5Stack https://github.com/m5stack/M5Stack/tree/master/examples/Unit/RFID_RC522 (MIT license)
// (Board) M5Stack 3.2.5 でビルドを確認
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
#include <math.h>
#include <ctype.h>
#include <string.h>

#define NTP_TIMEZONE  "JST-9"           // https://github.com/esp8266/Arduino/blob/master/cores/esp8266/TZ.h
#define NTP_SERVER1   "0.pool.ntp.org"
#define NTP_SERVER2   "1.pool.ntp.org"
#define NTP_SERVER3   "2.pool.ntp.org"

const char* ssid = "0000C30500";
const char* password = "pepperpepper";
const char* kConfigPageUrl = "https://ksasao.github.io/MomentStack/";
WebServer server(80);
char localUrl[256];

String posString = "@35.681684,139.786917,17z";
String textString = "Hello";

Preferences preferences;

MFRC522 mfrc522(0x28); // Create MFRC522 instance
char str[512];
char date_str[64];
char time_str[64];
char label_str[64];
M5GFX display;

long lastUpdate = millis();
bool restartScheduled = false;
unsigned long restartDeadline = 0;

String htmlEscape(const String &src) {
  String escaped;
  escaped.reserve(src.length());
  for (size_t i = 0; i < src.length(); ++i) {
    const char c = src[i];
    switch (c) {
      case '&':
        escaped += "&amp;";
        break;
      case '<':
        escaped += "&lt;";
        break;
      case '>':
        escaped += "&gt;";
        break;
      case '\"':
        escaped += "&quot;";
        break;
      case '\'':
        escaped += "&#39;";
        break;
      default:
        escaped += c;
        break;
    }
  }
  return escaped;
}

String jsEscape(const String &src) {
  String escaped;
  escaped.reserve(src.length() * 2);
  for (size_t i = 0; i < src.length(); ++i) {
    const unsigned char c = static_cast<unsigned char>(src[i]);
    switch (c) {
      case '\\':
        escaped += "\\\\";
        break;
      case '\'':
        escaped += "\\'";
        break;
      case '\"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      case '\b':
        escaped += "\\b";
        break;
      case '\f':
        escaped += "\\f";
        break;
      case '<':
        escaped += "\\x3C";
        break;
      case '>':
        escaped += "\\x3E";
        break;
      default:
        if (c < 0x20 || c == 0x7F) {
          char buf[8];
          snprintf(buf, sizeof(buf), "\\x%02X", c);
          escaped += buf;
        } else {
          escaped += static_cast<char>(c);
        }
        break;
    }
  }
  return escaped;
}

String urlEncode(const String &src) {
  String encoded;
  char buf[4];
  for (size_t i = 0; i < src.length(); ++i) {
    char c = src[i];
    if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else {
      snprintf(buf, sizeof(buf), "%%%02X", static_cast<unsigned char>(c));
      encoded += buf;
    }
  }
  return encoded;
}

bool urlEncodeToBuffer(const char *src, char *dest, size_t destSize) {
  if (!src || !dest || destSize == 0) {
    return false;
  }
  size_t out = 0;
  while (*src) {
    unsigned char c = static_cast<unsigned char>(*src);
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      if (out + 1 >= destSize) {
        return false;
      }
      dest[out++] = static_cast<char>(c);
    } else {
      if (out + 3 >= destSize) {
        return false;
      }
      static const char kHex[] = "0123456789ABCDEF";
      dest[out++] = '%';
      dest[out++] = kHex[(c >> 4) & 0xF];
      dest[out++] = kHex[c & 0xF];
    }
    ++src;
  }
  if (out >= destSize) {
    return false;
  }
  dest[out] = '\0';
  return true;
}

String composePosString(double lat, double lng, double zoom) {
  String result = "@";
  result += String(lat, 6);
  result += ",";
  result += String(lng, 6);
  result += ",";
  result += String(zoom, 0);
  result += "z";
  return result;
}

void handleLocationPost() {
  if (!server.hasArg("lat") || !server.hasArg("lng") || !server.hasArg("zoom")) {
    server.send(400, "text/plain", "Missing parameters");
    return;
  }

  double lat = server.arg("lat").toDouble();
  double lng = server.arg("lng").toDouble();
  double zoom = server.arg("zoom").toDouble();
  
  // より厳格な検証
  if (isnan(lat) || isnan(lng) || isnan(zoom) || 
      isinf(lat) || isinf(lng) || isinf(zoom)) {
    server.send(400, "text/plain", "Invalid numeric values");
    return;
  }
  
  if (lat < -90 || lat > 90 || lng < -180 || lng > 180) {
    server.send(400, "text/plain", "Invalid coordinates");
    return;
  }
  
  // ズーム値の範囲チェック
  if (zoom < 0 || zoom > 20) {
    server.send(400, "text/plain", "Invalid zoom level");
    return;
  }

  String newText = server.hasArg("text") ? server.arg("text") : textString;
  // 入力長制限: 64文字まで
  if (newText.length() > 64) {
    newText = newText.substring(0, 64);
  }
  
  // 精度を制限（6桁まで）
  lat = round(lat * 1000000.0) / 1000000.0;
  lng = round(lng * 1000000.0) / 1000000.0;
  zoom = round(zoom);
  
  posString = composePosString(lat, lng, zoom);
  textString = newText;

  preferences.begin("mapcard", false);
  preferences.putString("pos", posString);
  preferences.putString("text", textString);
  preferences.end();

  // Preferenceに保存された値を使ってURLを構築
  String targetUrl = String(kConfigPageUrl);
  targetUrl += (targetUrl.indexOf('?') >= 0 ? '&' : '?');
  targetUrl += "p=" + urlEncode(posString);
  targetUrl += "&t=" + urlEncode(textString);

  server.sendHeader("Connection", "close");
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Content-Security-Policy", "default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'");

  String linkHtml = htmlEscape(targetUrl);
  String jsTarget = jsEscape(targetUrl);

  String redirectBody = "<!DOCTYPE html><html><head><meta charset=\"utf-8\">";
  redirectBody += "<title>Redirecting...</title></head><body>";
  redirectBody += "<p>Setting saved. Redirecting to <a href=\"";
  redirectBody += linkHtml;
  redirectBody += "\">updated map</a>...</p>";
  redirectBody += "<script>";
  redirectBody += "setTimeout(function(){";
  redirectBody += "try{window.top.location.replace('";
  redirectBody += jsTarget;
  redirectBody += "');}catch(e){window.location.href='";
  redirectBody += jsTarget;
  redirectBody += "';}";
  redirectBody += "},1000);";
  redirectBody += "</script></body></html>";

  server.send(200, "text/html", redirectBody);

  M5.Log.println("Scheduling restart after config update (3s window)");
  restartScheduled = true;
  restartDeadline = millis() + 3000;
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

  server.on("/api/location", HTTP_GET, handleLocationPost);
  server.onNotFound(handleNotFound);
  server.begin();

  M5.Log.println("Web server started");

  String qrTarget = String(kConfigPageUrl);
  qrTarget += (qrTarget.indexOf('?') >= 0 ? '&' : '?');
  qrTarget += "p=" + urlEncode(posString);
  qrTarget += "&t=" + urlEncode(textString);
  qrTarget += "&d=";
  qrTarget += urlEncode(WiFi.localIP().toString());
  qrTarget.toCharArray(localUrl, sizeof(localUrl));

  M5.Display.clear(0);
  M5.Display.qrcode(localUrl,10,8,120,2);

}


NfcAdapter nfc = NfcAdapter(&mfrc522);

void logUid(const MFRC522::Uid &uid) {
  Serial.print("UID      :");
  for (byte i = 0; i < uid.size; i++) {
    if (uid.uidByte[i] < 0x10) {
      Serial.print(" 0");
    } else {
      Serial.print(" ");
    }
    Serial.print(uid.uidByte[i], HEX);
  }
  Serial.println();
}

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

void loadPreference(){
  preferences.begin("mapcard", false);
  posString = preferences.getString("pos",posString);
  textString = preferences.getString("text",textString);
  preferences.end();
  M5.Log.println(posString.c_str());
  M5.Log.println(textString.c_str());
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.delay(500);
  Wire.begin();
  Serial.begin(115200);
  loadPreference();
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
      logUid(mfrc522.uid);

      getDate(date_str);
      getTime(time_str);
      drawText(date_str,time_str,(char*)textString.c_str());

      char encodedPos[128];
      char tagComment[192];
      char encodedComment[256];

      snprintf(tagComment, sizeof(tagComment), "%s %s\n%s", date_str, time_str, textString.c_str());

      if (!urlEncodeToBuffer(posString.c_str(), encodedPos, sizeof(encodedPos)) ||
          !urlEncodeToBuffer(tagComment, encodedComment, sizeof(encodedComment))) {
        M5.Log.println("URL encode buffer overflow");
        M5.Speaker.tone(4000,500);
        drawText("URL too long");
        return;
      }

      const char *baseUrl = kConfigPageUrl;
      const char separator = strchr(baseUrl, '?') ? '&' : '?';
      int written = snprintf(str, sizeof(str), "%s%c" "p=%s&t=%s", baseUrl, separator, encodedPos, encodedComment);
      if (written < 0 || written >= static_cast<int>(sizeof(str))) {
        M5.Log.println("URL build failed");
        M5.Speaker.tone(4000,500);
        drawText("URL too long");
        return;
      }

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
    if (restartScheduled && (long)(millis() - restartDeadline) >= 0) {
        restartScheduled = false;
        M5.Log.println("Restarting after config update");
        delay(50);
        ESP.restart();
    }
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
