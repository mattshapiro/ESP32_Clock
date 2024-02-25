/*
  Matt Shapiro
  Send text to a MAX7219 LED display via ESP32-hosted webpage over wifi
  
  This sketch is based on the WebSerial library example: ESP32_Demo
  https://github.com/ayushsharma82/WebSerial
*/

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebSerial.h>

#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

#include "time.h"

// Uncomment according to your hardware type
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
//#define HARDWARE_TYPE MD_MAX72XX::GENERIC_HW

// Defining size, and output pins
#define MAX_DEVICES 4
#define CS_PIN 5

#define MAX_MSG_LEN 256

#define CST_SECS -21600
#define CST_DAYLIGHT 0
#define CDT_SECS -21600
#define CDT_DAYLIGHT 3600

char * msgbuffer;

#define LED 2

enum {
  CLOCK_MODE = 0,
  MESSAGE_MODE
} DisplayMode;

AsyncWebServer server(80);

const char* ssid = "ESP32-MsgBanner"; // Your WiFi SSID
const char* password = "0987654321";  // Your WiFi Password
const char* externalSSID = "Castle Clementine";         
const char* externalPassword = "Kr15t1na";
struct tm startTime;
long lastMillis;
int hour, minute, sec;
bool isAm = true;
int displayMode;

MD_Parola Display = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

void recvMsg(uint8_t *data, size_t len){

  if(displayMode == CLOCK_MODE) displayMode = MESSAGE_MODE;

  WebSerial.println("Received Data...");
  String d = "";
  len = (len < 256) ? len : 256;
  for(int i=0; i < len; i++){
    d += char(data[i]);
  }
  WebSerial.println(d);
  Display.displayClear();
  strcpy(msgbuffer, d.c_str());
  Display.displayScroll(msgbuffer,  PA_RIGHT, PA_SCROLL_LEFT, 150);
}

void initializeClock() {
  // Connect to Wi-Fi
  Serial.print("Connecting to ");
  Serial.println(externalSSID);
  WiFi.begin(externalSSID, externalPassword);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");

  // Init and get the time
  configTime(CST_SECS, CST_DAYLIGHT, "pool.ntp.org");

  if(!getLocalTime(&startTime)){
    Serial.println("Failed to obtain time");
  } else {
    hour = startTime.tm_hour;
    if(hour > 12) {
      hour = hour - 12;
      isAm = false;
    }
    minute = startTime.tm_min;
    sec = startTime.tm_sec;
    Serial.println("time got");
    Serial.println(&startTime, "%A, %B %d %Y %H:%M:%S");
    lastMillis = millis();
  }

  //disconnect WiFi as it's no longer needed
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

void setupServer() {
  pinMode(LED, OUTPUT);
  
  WiFi.softAP(ssid, password);

  strcpy(msgbuffer, WiFi.softAPIP().toString().c_str());

  Serial.print("IP Address: ");
  Serial.println(msgbuffer);

  // WebSerial is accessible at "<IP Address>/webserial" in browser
  WebSerial.begin(&server);
  WebSerial.msgCallback(recvMsg);
  server.begin();
}

void setup() {
  Serial.begin(115200);

  displayMode = CLOCK_MODE;

  Display.begin();
  Display.setIntensity(0);
  Display.displayClear();

  msgbuffer = (char*)malloc(MAX_MSG_LEN*sizeof(char));
  strcpy(msgbuffer,(const char*) "00");
  initializeClock();
  updateTime();
  //delay(1500);
  setupServer();
}

void loop() {
  if(displayMode == MESSAGE_MODE && Display.displayAnimate()) {
    Display.displayScroll(msgbuffer,  PA_RIGHT, PA_SCROLL_LEFT, 100);
    Display.displayReset();
  }
  updateTime();
}

void updateTime() {
  long delta = millis() - lastMillis;
  char h[3], m[3], s[3];
  if(delta >= 1000) {
    lastMillis= millis();
    long remainder = delta - 1000;
    sec += delta / 1000;
    if(sec >= 60) {
      sec = sec - 60;
      minute++;
      if(minute == 60) {
        minute = 0;
        hour++;
        if(hour == 13) {
          hour = 1;
          isAm = !isAm;
        }
      }
    }

    if(displayMode == MESSAGE_MODE) return;

    itoa(sec, s, 10);
    itoa(minute, m, 10);
    itoa(hour, h, 10);

    strcpy(msgbuffer, h);

#define BLINKER
#ifdef BLINKER
    if(sec % 2 == 0)
#endif
      strcat(msgbuffer,":");
#ifdef BLINKER
    else
      strcat(msgbuffer," ");
#endif
    if(minute < 10) {
      strcat(msgbuffer, "0");
    }
    strcat(msgbuffer, m);
#ifdef DISPLAY_SECONDS
    /* second display */
    strcat(msgbuffer,".");
    if(sec < 10) {
      strcat(msgbuffer, "0");
    } 
    strcat(msgbuffer, s);
#endif
#ifdef DISPLAY_AMPM
    strcat(msgbuffer, isAm ? "AM" : "PM");
#endif
#ifdef DISPLAY_AP
    strcat(msgbuffer, isAm ? "A" : "P");
#endif
    //static text display is oogly
    textPosition_t alignment = isAm ? PA_LEFT : PA_RIGHT;
    Display.setTextAlignment(alignment);
    Display.print(msgbuffer);
  }
}
