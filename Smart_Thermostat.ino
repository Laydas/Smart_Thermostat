/*

Iain Letourneau

ESP32 on the WT32-SC01 development board
Smart Thermostat

Uses an internally stored schedule to determine what temperature/humidity the house
should be. Has a DHT22 connected on the outside of the enclosure that feeds temp info
back to the board.

Controls a furnace by sending a command to turn on or off using PID control to avoid
overshooting the heating. Also controls the humidifier on the furnace.
 
*/
#include <TFT_eSPI.h> 
#include <SPI.h>
#include <Adafruit_FT6206.h>
#include "Free_Fonts.h"
#include "DHT.h"
#include "WiFi.h"
#include "time.h"

#include "Thermostat.h"
#include "Home_Icon.h"
#include "Cal_Icon.h"
#include "Gear_Icon.h"
#include "secrets.h"

#define DHTPIN 32
#define HEATPIN 33
#define HUMDPIN 27
#define DHTTYPE DHT22
#define PENRADIUS 2
#define DEG2RAD 0.0174532925

DHT dht(DHTPIN, DHTTYPE);

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite img = TFT_eSprite(&tft);

Adafruit_FT6206 ts = Adafruit_FT6206();

int key_h, button_col;

// Keep all the intervals in one object
struct intervals {
  unsigned long prev = 0;
  unsigned long prev_wifi = 0;
  unsigned long prev_heat = 0;
  unsigned long intv = 2000;
  unsigned long intv_wifi = 30000;
  unsigned long intv_heat = 120000;
} interval;

Thermostat thermostat = Thermostat(HEATPIN, HUMDPIN);

float old_t, old_h;

const uint16_t *menu[3] = {Home_Icon, Cal_Icon, Gear_Icon};
char* nav[4] = {"Main","Rooms","Schedule","Settings"};

/**
 * @brief Structure for holding button coordinates, makes it easy to check
 * if a button was pressed on touch input
 * 
 */
struct Button {
  int x, y, x2, y2;
  Button(int x, int x2, int y, int y2):x(x), x2(x2), y(y), y2(y2){
  }
};

struct Layout {
  Button next_dow = Button(170, 210, 20, 80);
  Button prev_dow = Button(30, 70, 20, 80);
  Button menu_bar = Button(380, 480, 80, 320);
  Button menu_rooms = Button(380, 480, 80, 160);
  Button menu_sched = Button(380, 480, 160, 240);
  Button menu_setting = Button(380,480, 240, 320);
  Button up_humd = Button(160, 320, 110, 215);
  Button down_humd = Button(160, 320, 215, 320);
} Layout;

/*
  Internet and NTP information
 */
const char* ssid = SSID_NAME;
const char* password = SSID_PASS;
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -25200;
const int daylightOffset_sec = 3600;

int current_dow;
int nav_current = 0;

/***********************************************************************************************************************************/
void setup() {
  Serial.begin(115200);

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  dht.begin();
  initWiFi();
  thermostat.begin();
  thermostat.setTargetHumidity(30.0);


  // Pins 18/19 are SDA/SCL for touch sensor on this device
  // 40 is a touch threshold
  if (!ts.begin(18, 19, 40)) {
    Serial.println("Couldn't start touchscreen controller");
    while (true);
  }

  tft.init();
  tft.fillScreen(TFT_BLACK);
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
  if(current - interval.prev >= interval.intv){
    interval.prev = current;
    old_t = getDHTTemp(old_t, nav[nav_current]);
    old_h = getDHTHum(old_h, nav[nav_current]);
    drawTime();
    thermostat.checkSchedule();
    // Turn heating/humidity on/off
    // Move this into thermostat class
    if(current - interval.prev_heat >= interval.intv_heat){
      if(old_t < thermostat.getGoalTemp() -1){
        thermostat.setHeating(true);
      } else if(old_t > thermostat.getGoalTemp() + 1){
        thermostat.setHeating(false);
      }

      if(old_h < 35){
        thermostat.setHumidity(true);
      } else if (old_h > 40){
        thermostat.setHumidity(false);
      }
    }
    
    checkWifi();
    if((WiFi.status() != WL_CONNECTED) && (current - interval.prev_wifi >= interval.intv_wifi)){
      WiFi.disconnect();
      WiFi.reconnect();
      interval.prev_wifi = current;
    }
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

  // Handle all buttons that would appear on the main screen
  if (screen == "Main"){
    // Check to see if the touch was inside the menu bar (for now it's the only buttons on main anyways)
    if(isButton(x, y, Layout.menu_bar)){
      if(isButton(x, y, Layout.menu_rooms)){
        new_nav = 1;
      } else if(isButton(x, y, Layout.menu_sched)){
        new_nav = 2;
      } else if(isButton(x, y, Layout.menu_setting)){
        new_nav = 3;
      }
    }
  } else {
    // Check to see if the back button was pressed on the other screens
    if(isButton(x,y, Layout.menu_bar)){
      if(isButton(x, y, Layout.menu_rooms)){
        new_nav = 0;
      }
    }
  }

  if(screen == "Settings"){
    if(isButton(x, y, Layout.up_humd)){
      thermostat.setTargetHumidity(thermostat.getGoalHumd() + 1);
      drawSettings();
    }
    if(isButton(x, y, Layout.down_humd)){
      thermostat.setTargetHumidity(thermostat.getGoalHumd() - 1);
      drawSettings();
    }
  }

  if(screen == "Schedule"){
    if(isButton(x, y, Layout.prev_dow)){
      thermostat.prevDisplayDay();
      drawSchedule();
    }
    if (isButton(x, y, Layout.next_dow)){
      thermostat.nextDisplayDay();
      drawSchedule();
    }
  }
  
  if(new_nav != nav_current){
    drawNav(nav[new_nav]);
    nav_current = new_nav;
  }
}

/**
 * @brief Checks to see if the touched coordinates are inside the button
 * 
 * @param x 
 * @param y 
 * @param button 
 * @return boolean 
 */
boolean isButton(int &x, int &y, Button &button){
  if((x > button.x && x < button.x2) && (y > button.y && y < button.y2))
    return true;
  else
    return false;
}

void drawNav(char* screen){
  tft.fillRect(0,40,480,280,TFT_BLACK);
  if (screen == "Main"){
    drawMain();
  } else if (screen == "Rooms"){
    drawRooms();
  } else if (screen == "Schedule"){
    drawSchedule();
  } else if (screen == "Settings"){
    drawSettings();
  }
  checkWifi();
}

void drawMain(){
  for (int i = 0; i < 3; i++){
    tft.pushImage(380, (i+1) * 80, 100, 80, menu[i]);
  }
  drawTempHeaders();
  drawDHTTemp(old_t);
  drawDHTHum(old_h);
  drawGoalTemp();
  drawGoalHumd();
}

void drawRooms(){
  tft.setCursor(0,200,2);
  tft.setTextColor(TFT_WHITE,TFT_BLUE); tft.setTextSize(3);
  tft.setFreeFont(FMO12);
  tft.fillRect(0, 150, 265, 210, TFT_BLACK);
  tft.print("Rooms!");
  drawBack(0);
}

void drawSchedule(){
  img.createSprite(160, 80);
  mainFont(img);
  img.fillTriangle(0,40,20,20,20,60, TFT_WHITE);
  img.fillTriangle(160,40,140,20,140,60, TFT_WHITE);
  img.setTextDatum(MC_DATUM);
  img.drawString(thermostat.getShortDow(), 80, 40);
  img.pushSprite(40,35);
  img.deleteSprite();

  img.setTextDatum(ML_DATUM);
  img.createSprite(300,160);
  tableFont(img);
  for(int i = 0; i < thermostat.getSlotCount(); i++){
    String temp_str = thermostat.getSlotInfo(i);
    img.drawString(temp_str, 0, 20+(i*40), GFXFF);
    img.drawCircle(265,10+(i*40),3,TFT_WHITE);
  }
  img.pushSprite(20, 100);
  img.deleteSprite();
  drawBack(0);
}

void drawSettings(){
  img.createSprite(200, 60);
  secondFont(img);
  img.setTextDatum(MC_DATUM);
  img.drawString("Set Humidity", 80, 30);
  img.pushSprite(160, 60);
  img.deleteSprite();
  img.createSprite(160, 150);
  mainFont(img);
  img.fillTriangle(80,0,110,30,50,30, TFT_WHITE);
  img.fillTriangle(80,150,110,120,50,120, TFT_WHITE);
  img.setTextDatum(MC_DATUM);
  String temp_str = String(thermostat.getGoalHumd());
  temp_str += "%";
  img.drawString(temp_str, 80, 70);
  img.pushSprite(160,140);
  img.deleteSprite();
  drawBack(0);
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
  img.createSprite(180, 60);
  img.setTextDatum(ML_DATUM);
  
  mainFont(img);
  String temp = String(t) + " c";
  img.drawString(temp, 5, 30, GFXFF);
  img.pushSprite(0, 180);
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
  img.createSprite(180, 60);
  img.setTextDatum(ML_DATUM);
  
  mainFont(img);
  String hum = String(h) + "%";
  img.drawString(hum, 5, 30, GFXFF);
  img.pushSprite(0, 240);
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

void drawBack(int angle){
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

void initWiFi(){
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }
}

/*
  Check the wifi strength and update the display
 */
void checkWifi(){
  if(WiFi.status() != WL_CONNECTED){
    drawWifi(455,35,0);
    return;
  }
  int strength = WiFi.RSSI();
  if(strength > -50){
    drawWifi(455, 35, 3);
  } else if(strength > -70){
    drawWifi(455, 35, 2);
  } else {
    drawWifi(455, 35, 1);
  }
}

void drawTime(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    return;
  }
  char local_out[33];
  char now_ampm[3];
  strftime(local_out,33,"%A, %B %d %I:%M", &timeinfo);
  strftime(now_ampm,3,"%p", &timeinfo);
  String ampm = String(now_ampm);
  ampm.toLowerCase();
  String full_out = String(local_out) + " ";
  full_out += ampm;
  ampm.toLowerCase();
  img.createSprite(400, 40);
  headerFont(img);
  img.setTextDatum(TR_DATUM);
  img.drawString(full_out, 400, 10, GFXFF);
  img.pushSprite(10,0);
  img.deleteSprite();
}

void drawTempHeaders(){
  img.createSprite(360,30);
  secondFont(img);
  img.setTextDatum(ML_DATUM);
  img.drawString("current",20,15, GFXFF);
  img.drawString("target", 200, 15, GFXFF);
  img.pushSprite(0,150);
  img.deleteSprite();
}

void mainFont(TFT_eSprite& img){
  img.setFreeFont(FF26);
  img.setTextColor(TFT_WHITE);
  img.setTextSize(2);
}
void secondFont(TFT_eSprite& img){
  img.setFreeFont(FMO12);
  img.setTextColor(TFT_DARKGREY);
  img.setTextSize(1);
}
void headerFont(TFT_eSprite& img){
  img.setFreeFont(FF26);
  img.setTextColor(TFT_WHITE);
  img.setTextSize(1);
}
void tableFont(TFT_eSprite& img){
  img.setTextSize(2);
  img.setFreeFont(FM9);
  img.setTextColor(TFT_WHITE);
}

void drawGoalTemp(){
  String goal_str = String(thermostat.getGoalTemp());
  img.createSprite(180,60);
  img.setFreeFont(FF26);
  img.setTextColor(TFT_WHITE);
  img.setTextSize(2);
  img.setTextDatum(ML_DATUM);
  img.drawString(goal_str,0,30);
  img.pushSprite(180,180);
  img.deleteSprite();
}

void drawGoalHumd(){
  String goal_str = String(thermostat.getGoalHumd());
  img.createSprite(180, 60);
  img.setFreeFont(FF26);
  img.setTextColor(TFT_WHITE);
  img.setTextSize(2);
  img.setTextDatum(ML_DATUM);
  img.drawString(goal_str, 0, 30);
  img.pushSprite(180,240);
  img.deleteSprite();
}

