#include <Arduino.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library
#include <SPI.h>
#if not defined (_VARIANT_ARDUINO_DUE_X_) && not defined (_VARIANT_ARDUINO_ZERO_)
  #include <SoftwareSerial.h>
#endif
#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"
#include "Adafruit_BluefruitLE_UART.h"

 #define FACTORYRESET_ENABLE         0
    #define MINIMUM_FIRMWARE_VERSION    "0.6.6"
    #define MODE_LED_BEHAVIOUR          "MODE"
#define BLE_READPACKET_TIMEOUT         500   // Timeout in ms waiting to read a response
// For the breakout, you can use any 2 or 3 pins
// These pins will also work for the 1.8" TFT shield
#define TFT_CS     9
#define TFT_RST    6  // you can also connect this to the Arduino reset
                      // in which case, set this #define pin to 0!
#define TFT_DC     12

// Option 1 (recommended): must use the hardware SPI pins
// (for UNO thats sclk = 13 and sid = 11) and pin 10 must be
// an output. This is much faster - also required if you want
// to use the microSD card (see the image drawing example)
//Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS,  TFT_DC, TFT_RST);

// Option 2: use any pins but a little slower!
#define TFT_SCLK 3   // set these to be whatever pins you like!
#define TFT_MOSI 10   // set these to be whatever pins you like!
Adafruit_ST7735 tft = 
    Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

// COMMON SETTINGS
// ----------------------------------------------------------------------
// These settings are used in both SW UART, HW UART and SPI mode
// ----------------------------------------------------------------------
#define BUFSIZE                        128   // Size of the read buffer
#define VERBOSE_MODE                   true  // Enables debug output

#define BLUEFRUIT_HWSERIAL_NAME      Serial1
#define BLUEFRUIT_UART_MODE_PIN        -1    // Set to -1 if unused
/* ...or hardware serial, which does not need the RTS/CTS pins. Uncomment this line */
Adafruit_BluefruitLE_UART ble(BLUEFRUIT_HWSERIAL_NAME, BLUEFRUIT_UART_MODE_PIN);


// the packet buffer
extern uint8_t packetbuffer[];
float AccX, AccY, AccZ, VecX, VecY,LocX=tft.width() / 2, LocY=tft.height() / 2 - 10;

// A small helper
void error(const __FlashStringHelper*err) {
  Serial.println(err);
  while (1);
}


uint8_t readPacket(Adafruit_BLE *ble, uint16_t timeout);
float parsefloat(uint8_t *buffer);
void printHex(const uint8_t * data, const uint32_t numBytes);


void setupBluefruitController(void){
  
  while (!Serial);  // required for Flora & Micro
  delay(500);

  Serial.begin(115200);
  Serial.println(F("Adafruit Bluefruit App Controller Example"));
  Serial.println(F("-----------------------------------------"));

  /* Initialise the module */
  Serial.print(F("Initialising the Bluefruit LE module: "));

  if ( !ble.begin(VERBOSE_MODE) )
  {
    error(F("Couldn't find Bluefruit, make sure it's in CoMmanD mode & check wiring?"));
  }
  Serial.println( F("OK!") );

  if ( FACTORYRESET_ENABLE )
  {
    /* Perform a factory reset to make sure everything is in a known state */
    Serial.println(F("Performing a factory reset: "));
    if ( ! ble.factoryReset() ){
      error(F("Couldn't factory reset"));
    }
  }


  /* Disable command echo from Bluefruit */
  ble.echo(false);

  Serial.println("Requesting Bluefruit info:");
  /* Print Bluefruit information */
  ble.info();

  Serial.println(F("Please use Adafruit Bluefruit LE app to connect in Controller mode"));
  Serial.println(F("Then activate/use the sensors, color picker, game controller, etc!"));
  Serial.println();

  ble.verbose(false);  // debug info is a little annoying after this point!

  /* Wait for connection */
  while (! ble.isConnected()) {
      delay(500);
  }

  Serial.println(F("******************************"));

  // LED Activity command is only supported from 0.6.6
  if ( ble.isVersionAtLeast(MINIMUM_FIRMWARE_VERSION) )
  {
    // Change Mode LED Activity
    Serial.println(F("Change LED activity to " MODE_LED_BEHAVIOUR));
    ble.sendCommandCheckOK("AT+HWModeLED=" MODE_LED_BEHAVIOUR);
  }

  // Set Bluefruit to DATA mode
  Serial.println( F("Switching to DATA mode!") );
  ble.setMode(BLUEFRUIT_MODE_DATA);

  Serial.println(F("******************************"));
}

void setupBluefruit(void)
{
  while (!Serial);  // required for Flora & Micro
  delay(500);

  Serial.println(F("Adafruit Bluefruit Command <-> Data Mode Example"));
  Serial.println(F("------------------------------------------------"));

  /* Initialise the module */
  Serial.print(F("Initialising the Bluefruit LE module: "));

  if ( !ble.begin(VERBOSE_MODE) )
  {
    error(F("Couldn't find Bluefruit, make sure it's in CoMmanD mode & check wiring?"));
  }
  Serial.println( F("OK!") );

  /* Disable command echo from Bluefruit */
  ble.echo(false);

  Serial.println("Requesting Bluefruit info:");
  /* Print Bluefruit information */
  ble.info();

  ble.verbose(false);  // debug info is a little annoying after this point!

  /* Wait for connection */
  while (! ble.isConnected()) {
      delay(500);
  }

  Serial.println(F("******************************"));

  // Set module to DATA mode
  Serial.println( F("Switching to DATA mode!") );
  ble.setMode(BLUEFRUIT_MODE_DATA);

  Serial.println(F("******************************"));
}

void testdrawtext(char *text, uint16_t color) {
  tft.setCursor(0, 0);
  tft.setTextColor(color);
  tft.setTextWrap(true);
  tft.print(text);
}

float width = tft.width();
float height = tft.height();

void setupScreen(void) {
  Serial.begin(115200);
  Serial.print("Hello! ST7735 TFT Test");

  // Use this initializer (uncomment) if you're using a 1.44" TFT
  tft.initR(INITR_144GREENTAB);   // initialize a ST7735S chip, black tab

  Serial.println("Initialized");

  uint16_t time = millis();
  tft.fillScreen(ST7735_GREEN);
    tft.drawRect(tft.width()/4, tft.height()/4 ,tft.width()/2 , tft.height()/2, ST7735_WHITE);
  time = millis() - time;

  // large block of text
  tft.fillScreen(ST7735_BLACK);
  testdrawtext("WE ARE AWESOME ", ST7735_WHITE);

}

void drawcircle(uint8_t radius, uint16_t color) {
  float centerWidth = (tft.width() / 2) ;
  float centerHeight = (tft.height() / 2) - radius;
  tft.drawCircle(centerWidth, centerHeight, radius, color);
}


void drawcir(uint8_t radius, uint16_t color, uint8_t x, uint8_t y) {
  tft.drawCircle(x, y, radius, color);
}


void setup() {
 // initialize the digital pin as an output.
  Serial.begin(9600);
  Serial.print("Hello! ST7735 TFT Test\n");  
  setupScreen();
  setupBluefruitController();
  tft.fillScreen(ST7735_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(ST7735_WHITE);
  tft.setTextWrap(true);
}


void loop() {
   /* Wait for new data to arrive */
  uint8_t len = readPacket(&ble, BLE_READPACKET_TIMEOUT);
  if (len == 0) return;

  /* Got a packet! */
  // printHex(packetbuffer, len);

  // Accelerometer
  if (packetbuffer[1] == 'A') {
    AccX = parsefloat(packetbuffer+2);
    AccY = parsefloat(packetbuffer+6);
    AccZ = parsefloat(packetbuffer+10);
    VecX = VecX + AccX;
    VecY = VecY + AccY;
    LocX = LocX + VecX;
    LocY = LocY + VecY;

    if(LocX>width){
      LocX = width;
    }

    if(LocY>height){
      LocY = height;
    }

    if(LocX<0){
      LocX = 0;
    }

    if(LocY<0){
      LocY = 0;
    }

    tft.fillScreen(ST7735_BLACK);
    drawcir(10,ST7735_WHITE, LocX, LocY);
    
    Serial.print("AccelX\t");
    Serial.print(AccX); Serial.print('\t');Serial.print(LocX);Serial.print('\n');
    Serial.print("AccelY\t");
    Serial.print(AccY); Serial.print('\t');Serial.print(LocY);Serial.print('\n');
    delay(100);
  }



  

  
  
}
