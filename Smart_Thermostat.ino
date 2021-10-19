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

#include "Home_Icon.h"
#include "Cal_Icon.h"
#include "Gear_Icon.h"

#define DHTPIN 32
#define DHTTYPE DHT22
#define PENRADIUS 3
#define DEG2RAD 0.0174532925

DHT dht(DHTPIN, DHTTYPE);

TFT_eSPI tft = TFT_eSPI();
Adafruit_FT6206 ts = Adafruit_FT6206();

int key_h;
uint16_t pen_color = TFT_CYAN;

unsigned long prev_time = 0;
const long interval = 2000;

float old_t, old_h;

const uint16_t *menu[3] = {Home_Icon, Cal_Icon, Gear_Icon};


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

  key_h = tft.height() / 4;
  tft.fillScreen(TFT_BLACK);

  for (int i = 0; i < 3; i++){
    tft.pushImage(380, (i+1) * 80, 100, 80, menu[i]);
  }
  drawWifi(455, 35, 1);
  
}

void loop() {
  // Update the onboard temp/humidity every 2 seconds.
  // This might be a bit aggressive.
  unsigned long current = millis();
  if(current - prev_time >= interval){
    prev_time = current;
      old_t = getDHTTemp(old_t);
      old_h = getDHTHum(old_h);
    }

  // Restart loop if the screen hasn't been touched
  if (! ts.touched()) {
    return;
  }

  // Get where the screen was touched and map it out for
  // the rotated (landscape) display
  TS_Point p = ts.getPoint();
  int y = p.x;
  int x = map(p.y, 0, 480, 480, 0);

  // Print out the touch point for debugging (remove upon deploy)
  Serial.print("("); Serial.print(x);
  Serial.print(", "); Serial.print(y);
  Serial.println(")");
  
  // Figure out where the screen was touched.
  if (x > (tft.width() - 100)){
    if (y < (key_h * 2) && (y > key_h)) {
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


// Update the screen with the temperature
float getDHTTemp(float old_t){
  float t = dht.readTemperature();
  if (t != old_t){
    tft.setCursor(0,200,2);
    tft.setTextColor(TFT_WHITE,TFT_BLUE); tft.setTextSize(3);
    tft.setFreeFont(FMO12);
    tft.fillRect(0, 150, 265, 210, TFT_BLACK);
    tft.print(String(t));
    tft.print("c");
  }
  return t;
}

// Update the screen with the humidity
float getDHTHum(float old_h){
  float h = dht.readHumidity();
  if (h != old_h){
    tft.setCursor(0,280,2);
    tft.setFreeFont(FF26);
    tft.fillRect(0,210,265, 295, TFT_BLACK);
    tft.print(String(h));
    tft.print("%");
  }
  return h;
}


//------------------------------------------------
// x is center X of wifi logo
// y is center Y of wifi logo
// strength 0-3 is the number of bars highlighted
//------------------------------------------------
void drawWifi(int x, int y, int strength){
  uint16_t str_sig[3] = {0x39E7,0x39E7,0x39E7};
  for (int i = 0; i < strength; i++){
    str_sig[i] = TFT_WHITE; 
  }
  fillArc(x, y, 310, 17, 25, 30, 4, str_sig[2]);
  fillArc(x, y+5, 315, 15, 18, 25, 4, str_sig[1]);
  tft.fillCircle(x-1, y-7, 4, str_sig[0]);
}
//------------------------------------------------


// I took this function from the TFT_espi library examples
// I really just use this to draw a wifi shape without using images
void fillArc(int x, int y, int start_angle, int seg_count, int rx, int ry, int w, unsigned int colour)
{

  byte seg = 6; // Segments are 3 degrees wide = 120 segments for 360 degrees
  byte inc = 6; // Draw segments every 3 degrees, increase to 6 for segmented ring

  // Calculate first pair of coordinates for segment start
  float sx = cos((start_angle - 90) * DEG2RAD);
  float sy = sin((start_angle - 90) * DEG2RAD);
  uint16_t x0 = sx * (rx - w) + x;
  uint16_t y0 = sy * (ry - w) + y;
  uint16_t x1 = sx * rx + x;
  uint16_t y1 = sy * ry + y;

  // Draw colour blocks every inc degrees
  for (int i = start_angle; i < start_angle + seg * seg_count; i += inc) {

    // Calculate pair of coordinates for segment end
    float sx2 = cos((i + seg - 90) * DEG2RAD);
    float sy2 = sin((i + seg - 90) * DEG2RAD);
    int x2 = sx2 * (rx - w) + x;
    int y2 = sy2 * (ry - w) + y;
    int x3 = sx2 * rx + x;
    int y3 = sy2 * ry + y;

    tft.fillTriangle(x0, y0, x1, y1, x2, y2, colour);
    tft.fillTriangle(x1, y1, x2, y2, x3, y3, colour);

    // Copy segment end to sgement start for next segment
    x0 = x2;
    y0 = y2;
    x1 = x3;
    y1 = y3;
  }
}
