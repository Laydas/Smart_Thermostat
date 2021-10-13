/*

Iain Letourneau

ESP32 on the WT32-SC01 development board
Smart Thermostat
 
 */
#include <TFT_eSPI.h> 
#include <SPI.h>
#include "Free_Fonts.h"
#include <Adafruit_FT6206.h>
#include "DHT.h"

#define DHTPIN 32
#define DHTTYPE DHT22
#define PENRADIUS 3

DHT dht(DHTPIN, DHTTYPE);

TFT_eSPI tft = TFT_eSPI();
Adafruit_FT6206 ts = Adafruit_FT6206();

int key_h;
uint16_t pen_color = TFT_CYAN;

unsigned long prev_time = 0;
const long interval = 2000;

float t, h, old_t, old_h;

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
  tft.setRotation(3);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, 128);

  //tft.setRotation(3);

  key_h = tft.height() / 4;
  tft.fillScreen(TFT_BLACK);
  uint16_t cols[4] = {TFT_CYAN, TFT_GREEN, TFT_YELLOW, TFT_RED};
  for (int i = 0; i < 4; i++){
    tft.fillRect(tft.width() - 100, i * key_h, 100, key_h, cols[i]);
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
    if (t != old_t){
      tft.setCursor(0,200,2);
      tft.setTextColor(TFT_WHITE,TFT_BLUE); tft.setTextSize(3);
      tft.setFreeFont(FMO12);
      tft.fillRect(0, 150, 265, 210, TFT_BLACK);
      tft.print(String(t));
      tft.print("c");
      old_t = t;
    }
    if (h != old_h){
      tft.setCursor(0,280,2);
      tft.setFreeFont(FF26);
      tft.fillRect(0,210,265, 295, TFT_BLACK);
      tft.print(String(h));
      tft.print("%");
      old_h = h;
    }
  } 
  
  if (! ts.touched()) {
    return;
  }
  
  TS_Point p = ts.getPoint();
  //int y = map(p.x, 0, 320, 320, 0);
  int y = p.x;
  int x = map(p.y, 0, 480, 480, 0);

  Serial.print("("); Serial.print(x);
  Serial.print(", "); Serial.print(y);
  Serial.println(")");
  
  
  if (x > (tft.width() - 100)){
    if (y < key_h) {
      pen_color = TFT_CYAN;
    } else if (y < (key_h * 2)) {
      pen_color = TFT_GREEN;
    } else if (y < (key_h * 3)) {
      pen_color = TFT_YELLOW;
    } else {
      pen_color = TFT_RED;
    }
  } else {
    tft.fillCircle(x,y, PENRADIUS, pen_color);
  }

  delay(5); // Delay to reduce loop rate (reduces flicker caused by aliasing with TFT screen refresh rate)
}
