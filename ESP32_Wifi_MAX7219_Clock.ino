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
  MESSAGE_MODE,
  SSID_MODE,
  PASSWORD_MODE
} DisplayMode;

AsyncWebServer server(80);

const char* ssid = "ESP32-MsgBanner"; // Your WiFi SSID
const char* password = "0987654321";  // Your WiFi Password
char externalSSID[256];         
char externalPassword[64];
struct tm startTime;
long lastMillis;
int hour, minute, sec;
bool isAm = true,
  clockInit = false;
int displayMode;

MD_Parola Display = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

void recvMsg(uint8_t *data, size_t len){
  
  WebSerial.println("Received Data...");
  String d = "";
  bool clockCmdReveived = false;
  len = (len < 256) ? len : 256;
  for(int i=0; i < len; i++){
    d += char(data[i]);
  }

  clockCmdReveived = (strcmp(d.c_str(), "CLOCK") == 0);

  if(clockCmdReveived) {
    WebSerial.println("Enter SSID");
    displayMode = SSID_MODE;
  } else if (displayMode == SSID_MODE) {
    WebSerial.println("Enter Password");
    displayMode = PASSWORD_MODE;
    strcpy(externalSSID, d.c_str());
  } else if (displayMode == PASSWORD_MODE) {
    WebSerial.println("Syncing Clock...");
    strcpy(externalPassword, d.c_str());
    displayMode = CLOCK_MODE;
    //disconnect WiFi for banner messages
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(1000);
    initializeClock();
  } else {
    displayMode = MESSAGE_MODE;
    WebSerial.println(d);
    Display.displayClear();
    strcpy(msgbuffer, d.c_str());
    Display.displayScroll(msgbuffer,  PA_RIGHT, PA_SCROLL_LEFT, 150);
  }
}

void initializeClock() {
  // Connect to Wi-Fi
  Serial.print("Connecting to ");
  Serial.println(externalSSID);
  Serial.print("Password: ");
  Serial.println(externalPassword);
  WiFi.begin(externalSSID, externalPassword);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 50) {
    delay(500);
    Serial.print(".");
    retry++;
  }

  if(retry >= 50) {
    Serial.println("Wifi Connection error check credentials");
    Display.print("Wifi Err");
    //disconnect WiFi as it's no longer needed
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    setupServer();
    return;
  }

  Serial.println("");
  Serial.println("WiFi connected.");

  // Init and get the time
  configTime(CST_SECS, CST_DAYLIGHT, "pool.ntp.org");

  if(!getLocalTime(&startTime)){
    Serial.println("Failed to obtain time");
    Display.print("Sync Err");
  } else {
    hour = startTime.tm_hour;
    if(hour >= 12) {
      isAm = false;
      if(hour > 12) {
        hour = hour - 12;
      }
    }
    minute = startTime.tm_min;
    sec = startTime.tm_sec;
    Serial.println("time got");
    Serial.println(&startTime, "%A, %B %d %Y %H:%M:%S");
    lastMillis = millis();
    clockInit = true;
  }
  //disconnect WiFi as it's no longer needed
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  setupServer(); // resume comms for banner message
}

void setupServer() {
  pinMode(LED, OUTPUT);
  
  WiFi.softAP(ssid, password);

  if(displayMode != CLOCK_MODE) {
    strcpy(msgbuffer, "SSID: ");
    strcat(msgbuffer, ssid);
    strcat(msgbuffer, " ... Pwd: ");
    strcat(msgbuffer, password);
    strcat(msgbuffer, " ... Site: ");
    strcat(msgbuffer, WiFi.softAPIP().toString().c_str());
    strcat(msgbuffer, "/webserial");
  }

  Serial.print("IP Address: ");
  Serial.println(msgbuffer);

  // WebSerial is accessible at "<IP Address>/webserial" in browser
  WebSerial.begin(&server);
  WebSerial.msgCallback(recvMsg);
  server.begin();
}

void setup() {
  Serial.begin(115200);

  displayMode = MESSAGE_MODE;

  Display.begin();
  Display.setIntensity(0);
  Display.displayClear();

  msgbuffer = (char*)malloc(MAX_MSG_LEN*sizeof(char));
  setupServer();
}

void loop() {
  if(displayMode == MESSAGE_MODE && Display.displayAnimate()) {
    Display.displayScroll(msgbuffer,  PA_RIGHT, PA_SCROLL_LEFT, 100);
    Display.displayReset();
  }
  if(clockInit) updateTime();
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
        if(hour == 12) {
          isAm = !isAm;
        }
        if(hour == 13) {
          hour = 1;
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
    //static text display is oogly
    textPosition_t alignment = isAm ? PA_LEFT : PA_RIGHT;
    Display.setTextAlignment(alignment);
    Display.print(msgbuffer);
  }
}
