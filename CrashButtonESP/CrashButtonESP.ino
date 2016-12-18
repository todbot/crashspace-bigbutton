/**
   CrashButtonESP


*/

#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <Ticker.h>

#include <ArduinoJson.h>
#include <FastLED.h>

#define NUM_LEDS 5
#define LEDPIN D1  // (pin D1 on NodeMCU board)
#define BUTTONPIN D2

#define BRIGHTNESS 50

const char* wifiSSID = "todbot";
const char* wifiPasswd = "";

#define mins_max 5

const char* httpurl_stat   = "http://crashspacela.com/sign2/?output=jsonmin";
const char* httpurl_press  = "http://crashspacela.com/sign2/?output=jsonmin&id=espbutton&msg=hi+tod&diff_mins_max=15&debug=1";
// SSL Certificate finngerprint for the host
const char* httpsurl = "https://crashspacela.com/sign2/?output=jsonmin";
const char* httpsfingerprint = "78 42 D1 58 CC A4 1D C5 CA 1F F2 FE C5 DA 68 BA A8 D9 85 CD";

Ticker ledticker;

ESP8266WiFiMulti WiFiMulti;


enum ledmodes {
  MODE_OFF = 0,
  MODE_PRESET,
  MODE_SOLID,
  MODE_SINELON,
  MODE_BREATHE,
  MODE_SECTOR
};

int ledMode = MODE_OFF;
uint8_t ledHue = 0; // rotating "base color" used by many of the patterns
int ledcnt = 0;

CRGB leds[NUM_LEDS];

const int fetchMillis = 5000;
uint32_t lastFetchMillis = 0;

const uint32_t buttonMillis = 60 * 1000; // millisecs between valid presses
uint32_t lastButtonTime;

const uint8_t ledUpdateMillis = 50;


bool doPress = false;

//
void setup()
{
    Serial.begin(115200);
    // Serial.setDebugOutput(true);
    Serial.println();
    
    pinMode( LED_BUILTIN, OUTPUT);
    digitalWrite( LED_BUILTIN, LOW); // on
    pinMode( BUTTONPIN, INPUT_PULLUP);
    
    FastLED.addLeds<WS2812, LEDPIN, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(BRIGHTNESS);
    fill_solid(leds, NUM_LEDS, CRGB(0, 255, 0));
    FastLED.show();
    
    for (uint8_t t = 4; t > 0; t--) {
        Serial.printf("[setup] waiting %d...\n", t);
        Serial.flush();
        blinkBuiltIn( 1, 100);
        delay(800);
    }
    
    WiFiMulti.addAP(wifiSSID, wifiPasswd);
    
    ledticker.attach_ms( ledUpdateMillis, ledUpdate );    
    
    digitalWrite( LED_BUILTIN, HIGH); // off
    Serial.println("[setup] done");
}


//
void loop()
{
  buttonCheck();

  // wait for WiFi connection
  if ( (WiFiMulti.run() == WL_CONNECTED) ) {
    fetchJson();
  } else {
    blinkBuiltIn( 2, 50);
  }

}

// called periodically from Ticker
void ledUpdate()
{
//  uint32_t now = millis();
//  if ( (now - lastLedUpdateMillis) < ledUpdateMillis ) {
//    return;
//  }
//  lastLedUpdateMillis = now;

  if ( ledMode == MODE_OFF ) {
    fadeToBlackBy( leds, NUM_LEDS, 255);
  }
  else if ( ledMode == MODE_SOLID ) {
    fill_solid( leds, NUM_LEDS, CHSV(ledHue, 255, 255) );
  }
  else if ( ledMode == MODE_PRESET ) {
    // do nothing
  }
  else if ( ledMode == MODE_SECTOR ) {
    for ( int i = 0; i < NUM_LEDS; i++) {
      leds[i] = 0;
    }
    for ( int i = 0; i < ledcnt; i++) {
      leds[i] = CHSV(ledHue, 255, 255);
    }
  }
  else if ( ledMode == MODE_SINELON ) {
    // a colored dot sweeping back and forth, with fading trails
    fadeToBlackBy( leds, NUM_LEDS, 10);
    int pos = beatsin16(13, 0, NUM_LEDS);
    leds[pos] += CHSV( ledHue, 255, 255);
  }
  else if ( ledMode == MODE_BREATHE ) {
    //        float breathe = (exp(sin(millis()/2000.0*PI)) - 0.36787944)*108.0;
    int breathe = beatsin8( 15, 0, 255);
    fill_solid( leds, NUM_LEDS, CHSV(ledHue, 255, breathe) );
  }
  FastLED.show();
}


void buttonCheck()
{
  int b = digitalRead( BUTTONPIN );
  if ( b == HIGH  ) { // not pressed
    return;
  }
  Serial.println("PRESS");
    
  ledHue = 128; // cyan
  ledMode = MODE_SOLID; // fixme

  uint32_t now = millis();   
  if ( (now < buttonMillis) || ((now - lastButtonTime) > buttonMillis) ) {
      Serial.println("PRESS for reals");
      lastButtonTime = millis();
      doPress = true;
      lastFetchMillis -= fetchMillis; // do fetch now
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

  uint32_t freesize = ESP.getFreeHeap();
  uint32_t chipId = ESP.getChipId();

  digitalWrite(LED_BUILTIN, LOW);
  Serial.print("[http] begin...");
  Serial.print( chipId ); Serial.print(": ");
  Serial.println( freesize );

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
    Serial.print("[http] GET... code: "); Serial.println(httpCode);

    // file found at server
    if ( httpCode == HTTP_CODE_OK ) {
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
    ledcnt =  1 + NUM_LEDS * minutes_left / 60;
    Serial.print("ledcnt:"); Serial.println(ledcnt);
    ledHue = 128; // aqua
    ledMode = MODE_SECTOR;
  }
  else {
    ledHue = 64; // yellow
    ledMode = MODE_BREATHE;
  }

}

// no float support in printf, so here's this
int getDecimal(float val)
{
  return abs((int)((val - (int)val) * 100.0));
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
