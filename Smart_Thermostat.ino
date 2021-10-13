/*

Iain Letourneau

ESP32 on the WT32-SC01 development board
Smart Thermostat
 
 */
#include <Adafruit_GFX.h>
#include <SPI.h>
#include <TFT_eSPI.h> 
#include <Adafruit_FT6206.h>
#include "DHT.h"

#define DHTPIN 32

#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);

TFT_eSPI tft = TFT_eSPI();
Adafruit_FT6206 ts = Adafruit_FT6206();

#define PENRADIUS 3

int key_w;
uint16_t pen_color = TFT_CYAN;

unsigned long prev_time = 0;
const long interval = 2000;

float t, h;

/***********************************************************************************************************************************/
void setup() {
  Serial.begin(115200);
  dht.begin();
  // Pins 18/19 are SDA/SCL for touch sensor on this device
  // 40 is a touch threshold
  if (!ts.begin(18, 19, 40)) {
    Serial.println("Couldn't start touchscreen controller");
    while (true);
  }

  tft.init();

  // Thanks to https://github.com/seaniefs/WT32-SC01-Exp
  // for figuring this out
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, 128);

  //tft.setRotation(3);

  key_w = tft.width() / 4;
  tft.fillScreen(TFT_BLACK);
  uint16_t cols[4] = {TFT_CYAN, TFT_GREEN, TFT_YELLOW, TFT_RED};
  for (int i = 0; i < 4; i++){
    tft.fillRect(i * key_w, 0, key_w, 100, cols[i]);
  }
}

/***********************************************************************************************************************************/
uint16_t fill_color = TFT_BLACK;

void loop() {
  unsigned long current = millis();
  if(current - prev_time >= interval){
    prev_time = current;
    t = dht.readTemperature();
    h = dht.readHumidity();

    tft.setCursor(0,200,2);
    tft.setTextColor(TFT_WHITE,TFT_BLUE); tft.setTextSize(3);
    tft.print(String(t));
    tft.println("c");
    tft.print(String(h));
    tft.print("%");
  } 
  
  if (! ts.touched()) {
    return;
  }
  
  TS_Point p = ts.getPoint();

  Serial.print("("); Serial.print(p.x);
  Serial.print(", "); Serial.print(p.y);
  Serial.println(")");

  
  
  if (p.y < 100){
    if (p.x < key_w) {
      pen_color = TFT_CYAN;
    } else if (p.x < (key_w * 2)) {
      pen_color = TFT_GREEN;
    } else if (p.x < (key_w * 3)) {
      pen_color = TFT_YELLOW;
    } else {
      pen_color = TFT_RED;
    }
  } else {
    tft.fillCircle(p.x,p.y, PENRADIUS, pen_color);
  }

  delay(5); // Delay to reduce loop rate (reduces flicker caused by aliasing with TFT screen refresh rate)
}
