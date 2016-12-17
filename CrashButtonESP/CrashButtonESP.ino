/**
   BasicHTTPClient.ino

    Created on: 24.05.2015

*/

#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>

#define NUM_LEDS 8
#define LEDPIN D1  // (pin D1 on NodeMCU board)
//#define PIN 5  // (pin D1 on NodeMCU board)
#define BUTTONPIN D2

#define BRIGHTNESS 50

const char* wifiSSID = "todbot-back";
const char* wifiPasswd = "";

const char* httpurl_stat   = "http://crashspacela.com/sign2/?output=jsonmin";
const char* httpurl_press  = "http://crashspacela.com/sign2/?output=jsonmin&id=espbutton&msg=hi+tod&diff_mins_max=5&debug=1";
// SSL Certificate finngerprint for the host
const char* httpsurl = "https://crashspacela.com/sign2/?output=jsonmin";
const char* httpsfingerprint = "78 42 D1 58 CC A4 1D C5 CA 1F F2 FE C5 DA 68 BA A8 D9 85 CD";

const int fetchMillis = 10000;
uint32_t lastFetchMillis = 0;


Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LEDPIN, NEO_GRB + NEO_KHZ800);

ESP8266WiFiMulti WiFiMulti;


#include "./color_funcs.h"


//
void setup()
{
  Serial.begin(115200);
  // Serial.setDebugOutput(true);
  Serial.println();
  
  pinMode( LED_BUILTIN, OUTPUT);
  digitalWrite( LED_BUILTIN, LOW); // on
  pinMode( BUTTONPIN, INPUT_PULLUP);

  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  strip.setBrightness( BRIGHTNESS );
  
  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("[setup] waiting %d...\n", t);
    Serial.flush();
    blinkBuiltIn( 1, 100);
    delay(800);
  }

  WiFiMulti.addAP(wifiSSID, wifiPasswd);

  digitalWrite( LED_BUILTIN, HIGH); // off
  Serial.println("[setup] done");
}

//
void blinkBuiltIn( int times, int msecs )
{
  for ( int i = 0; i < times; i++) {
    digitalWrite( LED_BUILTIN, LOW); // on
    delay(msecs);
    digitalWrite( LED_BUILTIN, HIGH); //off
    delay(msecs);
  }
}

bool doPress = false;
void setPress()
{
    blinkBuiltIn( 5, 100 );
    doPress = true;
    lastFetchMillis -= fetchMillis; // do fetch now
}

//
void loop()
{
    int b = digitalRead( BUTTONPIN);
    if ( b == LOW ) { // press
        setPress();
    }

    // wait for WiFi connection
    if ( (WiFiMulti.run() == WL_CONNECTED) ) {
        fetchJson();
    } else {
        blinkBuiltIn( 2, 100);
    }
}

//
//
void fetchJson()
{
    uint32_t now = millis();
    if ( (now - lastFetchMillis) < fetchMillis ) {
        return;
    }
    lastFetchMillis = now;

    digitalWrite(LED_BUILTIN, LOW);
    Serial.print("[http] begin...\n");

    HTTPClient http;
    //http.begin(httpsurl, httpsfingerprint);   // configure traged server and url
    const char* httpurl = (doPress) ? httpurl_press : httpurl_stat;
    http.begin(httpurl);

    Serial.print("[http] GET...\n");
    // start connection and send HTTP header
    int httpCode = http.GET();

    // httpCode will be negative on error
    if (httpCode > 0) {
        // HTTP header has been sent and Server response header has been handled
        Serial.printf("[http] GET... code: %d\n", httpCode);

        // file found at server
        if( httpCode == HTTP_CODE_OK ) {
            String payload = http.getString();
            handleJson( payload );
        }
    } else {
        Serial.printf("[http] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();

    digitalWrite(LED_BUILTIN, HIGH); // off
    doPress = false;
    delay(100);
}

//
bool handleJson(String jsonstr)
{
    Serial.println(jsonstr);
    
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(jsonstr);
    // Test if parsing succeeds.
    if (!root.success()) {
        Serial.println("parseObject() failed");
        return false;
    }

    bool is_open = root["is_open"];
    double minutes_left = root["minutes_left"];

    Serial.printf("is_open:%d", is_open);
    Serial.printf(", minutes_left: %d.%d\n", (int)minutes_left, getDecimal(minutes_left));

    if ( minutes_left > 0 ) {
        minutes_left = (minutes_left > 60) ? 60 : minutes_left;
        int ledcnt =  1 + strip.numPixels() * minutes_left / 60;
        Serial.print("ledcnt:"); Serial.println(ledcnt);
        for ( int i = 0; i < strip.numPixels(); i++) {
            strip.setPixelColor(i, 0); // off
        }

        for ( int i = 0; i < ledcnt; i++) {
            strip.setPixelColor(i, strip.Color(0, 255, 255) );
        }
        strip.show();
    }
    else {
        colorWipe(strip.Color(255, 255, 0), 100); // yellow
    }

}

// no float support in printf, so here's this
int getDecimal(float val)
{
  return abs((int)((val - (int)val) * 100.0));
}

/*
   NodeMCU has weird pin mapping.
  Pin numbers written on the board itself do not correspond to ESP8266 GPIO pin numbers. We have constants defined to make using this board easier:

  static const uint8_t D0   = 16;
  static const uint8_t D1   = 5;
  static const uint8_t D2   = 4;
  static const uint8_t D3   = 0;
  static const uint8_t D4   = 2;
  static const uint8_t D5   = 14;
  static const uint8_t D6   = 12;
  static const uint8_t D7   = 13;
  static const uint8_t D8   = 15;
  static const uint8_t D9   = 3;
  static const uint8_t D10  = 1;


*/
