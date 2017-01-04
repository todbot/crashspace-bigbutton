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
#define FASTLED_ESP8266_RAW_PIN_ORDER
#include <FastLED.h>
FASTLED_USING_NAMESPACE

#include <ArduinoJson.h>


#define NUM_LEDS (1 + 16) // note: first one is "sacrificial" neopixel acting as level converter
//#define NUM_LEDS 12  // note: first one is "sacrificial" neopixel acting as level converter
#define LEDPIN D4    // (GPIO2, pin D4 on Mini D1 board) 
#define BUTTONPIN D1

#define BRIGHTNESS 150

const char* wifiSSID = "todbot-back";
const char* wifiPasswd = " ";

#define BUTTON_BASEURL "http://crashspacela.com/sign2/?output=jsonmin"
#define BUTTON_ID      "espbutton1"
#define BUTTON_MSG     "hi+tod"
#define BUTTON_MINS    "30"
#define BUTTON_DEBUG   "&debug=1"
// we want "http://crashspacela.com/sign2/?output=jsonmin&id=espbutton&msg=hi+tod&diff_mins_max=15";

// TODO: can we do this with String() instead of char*?
const char* httpurl_stat  = BUTTON_BASEURL;  
const char* httpurl_press = BUTTON_BASEURL "&id=" BUTTON_ID "&msg=" BUTTON_MSG "&diff_mins_max=" BUTTON_MINS BUTTON_DEBUG;

//const char* httpsurl = "https://crashspacela.com/sign2/?output=jsonmin";
//const char* httpsfingerprint = "78 42 D1 58 CC A4 1D C5 CA 1F F2 FE C5 DA 68 BA A8 D9 85 CD";
// SSL Certificate finngerprint for the host

// What mode are we in?  These are the allowed ones
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
    MODE_SECTORBREATHE,
    MODE_RAINBOW
} LedMode;

ButtonMode buttonMode = MODE_STARTUP;
LedMode ledMode = MODE_OFF;

Ticker ledticker;
ESP8266WiFiMulti WiFiMulti;


const int fetchMillis = 3* 1000;  // how often API URL is checked
uint32_t lastFetchMillis = 0;

const uint32_t buttonMillis = 60 * 1000; // min time between valid presses
uint32_t lastButtonTime;

const uint8_t ledUpdateMillis = 50; // how often LEDs are updated

int badCount;
int maxBadCount = 5;    // how many bad events to become an error light
const uint32_t badMillis = 10 * 1000; // how long until badness becomes an error
uint32_t lastBadMillis = 0;

bool doPress = false; // was button pressed? (causes submit to API server)

uint8_t ledHue = 0; // rotating "base color" used by many of the patterns
int ledSpeed = 100;
int ledCnt = NUM_LEDS;
int ledRangeL = 0;
int ledRangeH = 255;

CRGB leds[NUM_LEDS];

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

    pinMode( BUTTONPIN, INPUT_PULLUP);
    
    FastLED.addLeds<WS2812, LEDPIN, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(BRIGHTNESS);
    fill_solid(leds, NUM_LEDS, CRGB(0, 255, 0));
    FastLED.show();
    
    for (uint8_t t = 4; t > 0; t--) {
        Serial.printf("[setupa] waiting %d...\n", t);
        Serial.flush();
        ledCnt = ledCnt - (ledCnt / 3);  // countdown light for fun
        delay(800);
    }
    
    WiFiMulti.addAP(wifiSSID, wifiPasswd);
    
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
        //buttonMode = MODE_ERROR;
        badCount++;
        Serial.print("WiFi not connected: "); Serial.println(rc);
        WiFi.printDiag(Serial);
        //blinkBuiltIn( 2, 50);
        delay(1000); // delay still services background tasks for WiFi connect
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
//        ledMode = MODE_SOLID; // fixme
//        ledHue = 128; // cyan
//        ledCnt = NUM_LEDS;
        ledMode = MODE_RAINBOW;
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
        ledRangeL = 0;   // brightness low
        ledRangeH = 100; // brightness high
    }
}

// called periodically from Ticker
void ledUpdate()
{
    buttonCheck();
    
    buttonModeToLedMode();

    errorDetect();
    
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
    else if( ledMode == MODE_RAINBOW ) { 
        fill_rainbow( leds, ledCnt, millis()/5, 255 / ledCnt );
    }
    else if( ledMode == MODE_PRESET ) {
        // do nothing
    }

    FastLED.show();
}

// check our button
void buttonCheck()
{
    int b = digitalRead( BUTTONPIN );
    if ( b == HIGH  ) { // not pressed
        return;
    }
    //Serial.println("PRESS");

    buttonMode = MODE_PRESSED;

    uint32_t now = millis();   
//    if ( (now < buttonMillis) && ((now - lastButtonTime) > buttonMillis) ) {
    if ( (now - lastButtonTime) > buttonMillis ) {
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
    if ( (now - lastFetchMillis) < fetchMillis ) {  // too soon
        return;
    }
    lastFetchMillis = now;

    uint32_t freesize = ESP.getFreeHeap();
    uint32_t chipId = ESP.getChipId();
    
    Serial.print(String("[http] @") + millis() +" id:"+ String( chipId, HEX ) +" freesize:"+ freesize );
    Serial.println(" IP: "+ WiFi.localIP().toString() +" SSID:"+ WiFi.SSID() +" RSSI:"+ WiFi.RSSI() +" dBm");

    HTTPClient http;

    //http.begin(httpsurl, httpsfingerprint);   // configure traged server and url
    const char* httpurlbase = (doPress) ? httpurl_press : httpurl_stat;
    
    String httpUrl = String(httpurlbase) +"&chipId="+ String(chipId,HEX) +"&secs="+ (millis()/1000) +"&bc="+ badCount;
    const char* httpurl = httpUrl.c_str();
    
    http.begin(httpurl);

    Serial.print("[http]: "); Serial.println(httpurl);
    // start connection and send HTTP header
    int httpCode = http.GET();

    // httpCode will be negative on error
    if (httpCode > 0) {
        // HTTP header has been sent and Server response header has been handled
        Serial.print("[http] response code: "); Serial.println(httpCode);

        // file found at server
        if ( httpCode == HTTP_CODE_OK ) {
            String payload = http.getString();
            bool goodresponse = handleJson( payload );
            if( !goodresponse ) {  badCount++;  }  // bad parse
        }
        else { 
            badCount++; // not a 200 response
        }
    }
    else {
        Serial.printf("[http] GET... failed, error: \n%s\n", http.errorToString(httpCode).c_str());
        // buttonMode = MODE_ERROR;
        badCount++;
    }

    http.end();

    doPress = false;  // say we handled the button press
  
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
        return false;
    }

    bool is_open = root["is_open"];
    double minutes_left = root["minutes_left"];

//    Serial.print("is_open:" + is_open +", minutes_left:" + minutes_left);

    if ( minutes_left > 0 ) {
        minutes_left = (minutes_left > 60) ? 60 : minutes_left;
        ledCnt =   NUM_LEDS * minutes_left / 60;  // ranges from 0 - NUM_LEDS
//        Serial.print("ledCnt:" + ledCnt);
        buttonMode = MODE_OPEN;
    }
    else {
        buttonMode = MODE_CLOSED;
    }
    
    return true;
}

// accumulate badness, if too much, trigger error
void errorDetect()
{
    uint32_t now = millis();   
    if ( (now - lastBadMillis) < badMillis ) {  // too soon
        return;
    }
    lastBadMillis = now;
    
    //Serial.printf("errorDetect: badCount:%d\n", badCount);
    
    if( badCount > maxBadCount ) { 
        buttonMode = MODE_ERROR;  // signal error
    }
    
    // decay badcount
    // FIXME: maybe have some mechanism to allow badCount to decay faster with multiple good events?
    if( badCount > 0 ) { badCount--; }
}


// Confusing NodeMCU vs Mini D1 board notation aside:
// On both NodeMCU and D1 Mini boards, pin "D4" is GPIO2.
// FastLED tries to help out NodeMCU users by mapping Dx numbers to GPIO numbers
//  (e.g. '4' means NodeMCU D4 pin not GPIO 4 when NodeMCU board is selected)
// However, when WeMos D1 Mini board is selected, FastLED pin number is GPIO number (e.g. '2' means GPIO2 not D2)

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
