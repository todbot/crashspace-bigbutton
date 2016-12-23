/**
 *  CrashButtonESP
 *
 *  Tod E. Kurt / http://todbot.com/
 *
 **/

#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <Ticker.h>

//#define FASTLED_ALLOW_INTERRUPTS 0
#include <FastLED.h>
FASTLED_USING_NAMESPACE

#include <ArduinoJson.h>

#define NUM_LEDS 17  // first one is "sacrificial" neopixel acting as level converter
//#define NUM_LEDS 13  // first one is "sacrificial" neopixel acting as level converter
//#define LEDPIN D4  // (pin D4 on NodeMCU board)
#define LEDPIN 4  // (pin D4 on NodeMCU board) (FastLED does not like 'D4')
//#define LEDPIN D1  // (pin D1 on NodeMCU board)
#define BUTTONPIN D1

#define BRIGHTNESS 150

const char* wifiSSID = "todbot";
const char* wifiPasswd = " ";

#define BUTTON_BASEURL "http://crashspacela.com/sign2/?output=jsonmin"
#define BUTTON_ID      "espbutton1"
#define BUTTON_MSG     "hi+tod"
#define BUTTON_MINS    "30"
#define BUTTON_DEBUG   "&debug=1"
// we want "http://crashspacela.com/sign2/?output=jsonmin&id=espbutton&msg=hi+tod&diff_mins_max=15&debug=1";

const char* httpurl_stat  = BUTTON_BASEURL;  
const char* httpurl_press = BUTTON_BASEURL "&id=" BUTTON_ID "&msg=" BUTTON_MSG "&diff_mins_max=" BUTTON_MINS BUTTON_DEBUG;

//const char* httpsurl = "https://crashspacela.com/sign2/?output=jsonmin";
//const char* httpsfingerprint = "78 42 D1 58 CC A4 1D C5 CA 1F F2 FE C5 DA 68 BA A8 D9 85 CD";
// SSL Certificate finngerprint for the host

Ticker ledticker;

ESP8266WiFiMulti WiFiMulti;

typedef enum ButtonModes {
    MODE_UNKNOWN = 0,
    MODE_STARTUP,     // not joined AP yet, 
    MODE_CLOSED,      // "closed" button hasn't been pressed in a while, color: yellow 
    MODE_PRESSED,     // user pressed button, but before app has fetched state
    MODE_OPEN,        // shows current open time 
    MODE_ERROR        // error of some kind, flash error, color: red
} ButtonMode;

typedef enum LedModes {
    MODE_OFF = 0,
    MODE_PRESET,
    MODE_SOLID,
    MODE_SINELON,
    MODE_BREATHE,
    MODE_SECTOR,
    MODE_SECTORBREATHE
} LedMode;

ButtonMode buttonMode = MODE_STARTUP;
LedMode ledMode = MODE_OFF;

uint8_t ledHue = 0; // rotating "base color" used by many of the patterns
int ledSpeed = 100;
int ledCnt = NUM_LEDS;
int ledRangeL = 0;
int ledRangeH = 255;

CRGB leds[NUM_LEDS];

const int fetchMillis = 3000;
uint32_t lastFetchMillis = 0;

const uint32_t buttonMillis = 60 * 1000; // millisecs between valid presses
uint32_t lastButtonTime;

const uint8_t ledUpdateMillis = 50;

bool doPress = false;

//
void setup()
{
    Serial.begin(115200);
//    Serial.setDebugOutput(true);
    WiFi.disconnect(true);   // delete old config
//    WiFi.setAutoConnect( false );
 
    Serial.println("\nCrashButtonESP: " BUTTON_ID );
    delay(1500);    
    Serial.println("WiFi config:");
    WiFi.printDiag(Serial);
    
    pinMode( LED_BUILTIN, OUTPUT);
    digitalWrite( LED_BUILTIN, LOW); // on
    pinMode( BUTTONPIN, INPUT_PULLUP);
    
    FastLED.addLeds<WS2812, LEDPIN, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(BRIGHTNESS);
    fill_solid(leds, NUM_LEDS, CRGB(0, 255, 0));
    FastLED.show();
    
    for (uint8_t t = 4; t > 0; t--) {
        Serial.printf("[setupa] waiting %d...\n", t);
        Serial.flush();
        blinkBuiltIn( 1, 100);
        ledCnt = ledCnt - (ledCnt / 3);
        delay(800);
    }
    
    WiFiMulti.addAP(wifiSSID, wifiPasswd);
    
    digitalWrite( LED_BUILTIN, HIGH); // off
    Serial.println("[setup] done");
    
    ledticker.attach_ms( ledUpdateMillis, ledUpdate );
    
}

//
void loop()
{
    // wait for WiFi connection
    int rc = WiFiMulti.run();
    if ( (rc == WL_CONNECTED) ) {
        fetchJson();
    } else {
        buttonMode = MODE_ERROR;
        Serial.print("WiFi not connected: "); Serial.println(rc);
        WiFi.printDiag(Serial);
        blinkBuiltIn( 2, 50);
        delay(100);
    }

    if( doPress ) { Serial.println("PRESS"); }
//    if( doLedShow ) { FastLED.show(); doLedShow = false; } 
    
    //delay(50); // allow WiFi stack to run and rate-limit continuous button presses
}

// Convert app state to LED commands
void buttonModeToLedMode() 
{
    if( buttonMode == MODE_STARTUP ) { 
        ledHue = 192; 
        ledMode = MODE_BREATHE;
//        ledCnt = NUM_LEDS;
        ledSpeed = 100;
        ledRangeL = 0;
        ledRangeH = 255;
    }
    else if( buttonMode == MODE_CLOSED ) {
        ledHue = 64; // yellow
        ledMode = MODE_BREATHE;
        ledCnt = NUM_LEDS;
        ledSpeed = 15;
        ledRangeL = 100;
        ledRangeH = 255;        
    }
    else if( buttonMode == MODE_PRESSED ) { 
        ledMode = MODE_SOLID; // fixme
        ledHue = 128; // cyan
        ledCnt = NUM_LEDS;
    }
    else if( buttonMode == MODE_OPEN ) {
        ledHue = 128; // aqua
        ledMode = MODE_SECTORBREATHE;
        // ledCnt set elsewhere
        ledSpeed = 15;
        ledRangeL = 100;
        ledRangeH = 255;        
    }
    else if( buttonMode == MODE_ERROR ) {
        ledMode = MODE_BREATHE;
        ledHue = 0;
        ledCnt = NUM_LEDS;
        ledSpeed = 120;
        ledRangeL = 0;
        ledRangeH = 155;
    }
}

// called periodically from Ticker
void ledUpdate()
{
    buttonCheck();
    
    buttonModeToLedMode();
    
    if( ledMode == MODE_OFF ) {
        fadeToBlackBy( leds, NUM_LEDS, 255);
    }
    else if( ledMode == MODE_SOLID ) {
        fill_solid( leds, ledCnt, CHSV(ledHue, 255, 255) );
    }
    else if( ledMode == MODE_SECTOR ) {
        fill_solid( leds, NUM_LEDS, 0 );
        fill_solid( leds, ledCnt, CHSV(ledHue, 255, 255) );
    }
    else if( ledMode == MODE_BREATHE ) {
        int breathe = beatsin8( ledSpeed, ledRangeL, ledRangeH);
        fill_solid( leds, NUM_LEDS, 0 );
        fill_solid( leds, ledCnt, CHSV(ledHue, 255, breathe) );
    }
    else if( ledMode == MODE_SECTORBREATHE ) { 
        int breathe = beatsin8( ledSpeed, ledRangeL, ledRangeH);        
        fill_solid( leds, NUM_LEDS, 0 );
        fill_solid( leds, ledCnt, CHSV(ledHue, 255, breathe) );
        if( ledCnt < NUM_LEDS ) { 
            leds[ledCnt] = CHSV(ledHue, 120, breathe); //CRGB(breathe,breathe,breathe);
        }
    }
    else if ( ledMode == MODE_SINELON ) {
        // a colored dot sweeping back and forth, with fading trails
        fadeToBlackBy( leds, NUM_LEDS, 10);
        int pos = beatsin16(13, 0, NUM_LEDS);
        leds[pos] += CHSV( ledHue, 255, 255);
    }
    else if( ledMode == MODE_PRESET ) {
        // do nothing
    }

    FastLED.show();
}


void buttonCheck()
{
    int b = digitalRead( BUTTONPIN );
    if ( b == HIGH  ) { // not pressed
        return;
    }
    //Serial.println("PRESS");

    buttonMode = MODE_PRESSED;

    uint32_t now = millis();   
    if ( (now < buttonMillis) || ((now - lastButtonTime) > buttonMillis) ) {
//    if ( (now - lastButtonTime) > buttonMillis) ) {
        //Serial.println("PRESS for reals");
        lastButtonTime = millis();
        doPress = true;
        lastFetchMillis -= fetchMillis; // signal: do fetch now
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
    
    Serial.print("[http] begin @"); Serial.print(millis());
    Serial.print(" id:"); Serial.print( chipId, HEX ); 
    Serial.print(" freesize:"); Serial.print( freesize );
    Serial.printf(" IP:%s SSID:%s RSSI:%d dBm\n", WiFi.localIP().toString().c_str(), WiFi.SSID().c_str(), WiFi.RSSI());

    HTTPClient http;

    //http.begin(httpsurl, httpsfingerprint);   // configure traged server and url
    const char* httpurl = (doPress) ? httpurl_press : httpurl_stat;
    http.begin(httpurl);

    Serial.print("[http] GET "); Serial.println(httpurl);
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
        buttonMode = MODE_ERROR;
    }

    http.end();

    digitalWrite(LED_BUILTIN, HIGH); // off
    doPress = false;
  
    delay(100); // FIXME: why is this here?
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
        // FIXME: add error handling
        return false;
    }

    bool is_open = root["is_open"];
    double minutes_left = root["minutes_left"];

//    Serial.printf("is_open:%d", is_open); // NOTE: the below fails for -0.99 to -0.01
//    Serial.printf(", minutes_left: %d.%d\n", (int)minutes_left, getDecimal(minutes_left));

    if ( minutes_left > 0 ) {
        minutes_left = (minutes_left > 60) ? 60 : minutes_left;
        ledCnt =   NUM_LEDS * minutes_left / 60;  // ranges from 0 - NUM_LEDS
//        Serial.print("ledCnt:"); Serial.println(ledCnt);
        buttonMode = MODE_OPEN;
    }
    else {
        buttonMode = MODE_CLOSED;
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
  static const uint8_t D4   = 2;  NeoEsp8266Uart800KbpsMethod
  static const uint8_t D5   = 14;
  static const uint8_t D6   = 12;
  static const uint8_t D7   = 13;
  static const uint8_t D8   = 15;
  static const uint8_t D9   = 3;  NeoEsp8266Dma800KbpsMethod  // default, also UART RX
  static const uint8_t D10  = 1;


*/
