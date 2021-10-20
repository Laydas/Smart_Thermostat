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
#define PENRADIUS 2
#define DEG2RAD 0.0174532925

DHT dht(DHTPIN, DHTTYPE);

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite img = TFT_eSprite(&tft);

Adafruit_FT6206 ts = Adafruit_FT6206();

int key_h, button_col;
uint16_t pen_color = TFT_CYAN;

unsigned long prev_time = 0;
const long interval = 2000;

float old_t, old_h;

const uint16_t *menu[3] = {Home_Icon, Cal_Icon, Gear_Icon};

char* nav[4] = {"Main","Rooms","Schedule","Settings"};

struct temp_time {
  int hour;
  int minute;
  float temp;
};

struct schedule {
  char* day;
  temp_time times[10] = {};
  int len = 0;
} schedule[7];

const int next_dow[4] = {170, 20, 210, 80};
const int prev_dow[4] = {30, 20, 70, 80};
int current_dow = 0;

int nav_current = 0;


/***********************************************************************************************************************************/
void setup() {
  Serial.begin(115200);
  dht.begin();

  // --------------------------------------------------------------------
  // Create a default schedule here. Replace this will a call
  // from ROM first, there should be a way to save your schedule
  // --------------------------------------------------------------------
  char* dow[7] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
  // Default schedule
  for(int i = 1; i < 6; i++){
    schedule[i].day = dow[i];
    schedule[i].times[0] = {6,0,22.5};
    schedule[i].times[1] = {8,30,19.5};
    schedule[i].times[2] = {3,30,22.5};
    schedule[i].times[3] = {11,30,19.5};
    schedule[i].len = 4;
  }
  schedule[0].day = dow[0];
  schedule[0].times[0] = {8,0,22};
  schedule[0].times[1] = {11,0,19.5};
  schedule[0].len = 2;
  schedule[6].day = dow[6];
  schedule[6].times[0] = {8,0,22};
  schedule[6].times[1] = {11,0,19.5};
  schedule[6].len = 2;
  // --------------------------------------------------------------------

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
  button_col = tft.width() - 100;
  
  drawNav(nav[nav_current]);
  
}

void loop() {
  // Update the onboard temp/humidity every 2 seconds.
  // This might be a bit aggressive.
  unsigned long current = millis();
  if(current - prev_time >= interval){
    prev_time = current;
    old_t = getDHTTemp(old_t, nav[nav_current]);
    old_h = getDHTHum(old_h, nav[nav_current]);
    drawWifi(455, 35, 1);
  }
    
  // Restart loop if the screen hasn't been touched
  if (! ts.touched()) {
    return;
  }

  handleTouch(ts.getPoint(), nav[nav_current]);

  delay(250); // Delay to reduce loop rate (reduces flicker caused by aliasing with TFT screen refresh rate)
}

void handleTouch(TS_Point p, char* screen){
  int new_nav = nav_current;
  int y = p.x;
  int x = map(p.y, 0, 480, 480, 0);

  int button = getButtonPress(x, y);
  
  if ((x > prev_dow[0]) && (x < prev_dow[2]) && (y > prev_dow[1]) && y < prev_dow[3]){
    current_dow = (current_dow + 6) % 7;
    drawSchedule();
  }
  if (x > next_dow[0] && x < next_dow[2] && y > next_dow[1] && y < next_dow[3]){
    current_dow = (current_dow + 1) % 7;
    drawSchedule();
  }
  if (screen == "Main"){
    if (button == 1){
      new_nav = 1;
    } else if (button == 2){
      new_nav = 2;
    } else if (button == 3){
      new_nav = 3;
    }
  } else {
    if (button == 1) {
      new_nav = 0;
    }  
  }
  
  if(new_nav != nav_current){
    drawNav(nav[new_nav]);
    nav_current = new_nav;
  }
}

int getButtonPress(int x, int y){
  if (x > button_col){
    if (y > (key_h * 3)){
      // Button 3 was pressed
      return 3;
    } else if ( y > (key_h * 2)) {
      // Button 2 was pressed
      return 2;
    } else if (y > key_h){
      // Button 1 was pressed
      return 1;
    } else {
      return 0;
    }
  } else {
    return 0;
  }
}

void drawNav(char* screen){
  tft.fillScreen(TFT_BLACK);
  if (screen == "Main"){
    drawMain();
  } else if (screen == "Rooms"){
    drawRooms();
  } else if (screen == "Schedule"){
    drawSchedule();
  } else if (screen == "Settings"){
    drawSettings();
  }
  drawWifi(455, 35, 1);
}

void drawMain(){
  for (int i = 0; i < 3; i++){
    tft.pushImage(380, (i+1) * 80, 100, 80, menu[i]);
  }
  drawDHTTemp(old_t);
  drawDHTHum(old_h);
}

void drawRooms(){
  tft.setCursor(0,200,2);
  tft.setTextColor(TFT_WHITE,TFT_BLUE); tft.setTextSize(3);
  tft.setFreeFont(FMO12);
  tft.fillRect(0, 150, 265, 210, TFT_BLACK);
  tft.print("Rooms!");
  drawBack();
}

void drawSchedule(){
  img.setTextSize(2);
  img.setFreeFont(FF26);
  img.createSprite(160, 80);
  img.fillTriangle(0,40,20,20,20,60, TFT_WHITE);
  img.fillTriangle(160,40,140,20,140,60, TFT_WHITE);
  img.setTextDatum(MC_DATUM);
  img.drawString(schedule[current_dow].day, 80, 40);
  img.pushSprite(40,10);
  img.deleteSprite();
  
  img.setTextSize(2);
  img.setFreeFont(FM9);
  img.setTextDatum(ML_DATUM);
  img.createSprite(320, 220);
  img.fillSprite(TFT_BLACK);
  img.pushSprite(0,100);
  img.deleteSprite();
  for(int i = 0; i < schedule[current_dow].len; i++){
    img.createSprite(300,40);
    String temp_str;
    if (schedule[current_dow].times[i].hour < 10){
      temp_str += "0";
    }
    temp_str += String(schedule[current_dow].times[i].hour) + ":";
    if (schedule[current_dow].times[i].minute < 10){
      temp_str += "0";
    }
    temp_str += String(schedule[current_dow].times[i].minute);
    temp_str += "  " + String(schedule[current_dow].times[i].temp);
    temp_str += "c";
    img.drawString(temp_str, 0, 20, GFXFF);
    img.drawCircle(265,10,3,TFT_WHITE);
    img.pushSprite(20, 100+(i*40));
    img.deleteSprite();
  }
  drawBack();
}

void drawSettings(){
  tft.setCursor(0,200,2);
  tft.setTextColor(TFT_WHITE,TFT_BLUE); tft.setTextSize(3);
  tft.setFreeFont(FMO12);
  tft.fillRect(0, 150, 265, 210, TFT_BLACK);
  tft.print("Settings!");
  drawBack();
}

// Update the screen with the temperature
float getDHTTemp(float old_t, char* screen){
  float t = dht.readTemperature();
  if (t != old_t && screen == "Main"){
    drawDHTTemp(t);
  }
  return t;
}

void drawDHTTemp(float t){
  img.createSprite(265, 60);
  img.fillSprite(TFT_BLACK);
  img.setTextSize(2);
  img.setTextDatum(ML_DATUM);
  img.setFreeFont(FF26);
  String temp = String(t) + " c";
  img.drawString(temp, 5, 30, GFXFF);
  img.pushSprite(0, 150);
  img.deleteSprite();
}

// Update the screen with the humidity
float getDHTHum(float old_h, char* screen){
  float h = dht.readHumidity();
  if (h != old_h && screen == "Main"){
    drawDHTHum(h);
  }
  return h;
}

void drawDHTHum(float h){
  img.createSprite(265, 60);
  img.fillSprite(TFT_BLACK);
  img.setTextSize(2);
  img.setTextDatum(ML_DATUM);
  img.setFreeFont(FF26);
  String hum = String(h) + "%";
  img.drawString(hum, 5, 30, GFXFF);
  img.pushSprite(0, 210);
  img.deleteSprite();
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

void drawBack(){
  img.createSprite(80, 80);
  int start_x = 10;
  int start_y = 40;
  for(int i = 0; i < 25; i++){
    img.fillCircle(start_x + i, start_y - i, PENRADIUS, TFT_WHITE);
  }
  
  for(int i = 0; i < 25; i++){
    img.fillCircle(start_x + i, start_y + i, PENRADIUS, TFT_WHITE);
  }

  for(int i = 0; i < 50; i++){
    img.fillCircle( start_x + i, start_y, PENRADIUS, TFT_WHITE);
  }
  img.pushSprite(400, 80);
  img.deleteSprite();
}
