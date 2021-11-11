#ifndef DRAW_H
#define DRAW_H

#include <TFT_eSPI.h> 
#include <SPI.h>
#include "Free_Fonts.h"
#include "time.h"

#include "Home_Icon.h"
#include "Cal_Icon.h"
#include "Gear_Icon.h"
#define DEG2RAD 0.0174532925
#define PENRADIUS 2

/**
 * @brief This class has preconfigured drawing methods for a 480x320 pixel
 * tft screen, in this case the WT32-SC01 development board.
 * 
 */
class Draw {
  private:
    TFT_eSPI tft = TFT_eSPI();
    TFT_eSprite img = TFT_eSprite(&tft);
    const uint16_t *menu[3] = {Home_Icon, Cal_Icon, Gear_Icon};

  public:
    void begin();
    
    // Navigational
    void main(float temp, float humd, float goal_temp, float goal_humd, boolean holding);
    void rooms();
    void schedule(String slots[], String short_dow);
    void settings(boolean hold, float hold_temp, float goal_humd);

    // Helper functions
    void tempHeaders();
    void back(TFT_eSprite &img);
    void wifi(int x, int y, int strength);
    void fillArc(int x, int y, int start_angle, int seg_count, int rx, int ry, int w, unsigned int colour);
    void time();

    // Temperature Sensor
    void dhtHumd(float humd);
    void dhtTemp(float temp);
    void goalHumd(float humd);
    void goalTemp(boolean holding, float temp);
    
    // Fonts
    void mainFont(TFT_eSprite& img);
    void secondFont(TFT_eSprite& img);
    void headerFont(TFT_eSprite& img);
    void tableFont(TFT_eSprite& img);
};

/**
 * @brief Pins 18/19 are SDA/SCL for touch sensor on this device
 * 40 is a touch threshold
 * 
 */
void Draw::begin(){
  tft.init();
  tft.fillScreen(TFT_BLACK);
  tft.setRotation(3);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, 128);
}

/**
 * @brief Draw out the main landing screen with navigation to all
 * other screens with thermostat functions
 * 
 * @param temp 
 * @param humd 
 * @param goal_temp 
 * @param goal_humd 
 * @param holding
 */
void Draw::main(float temp, float humd, float goal_temp, float goal_humd, boolean holding){
  img.createSprite(480, 280);
  img.fillScreen(TFT_BLACK);
  img.pushSprite(0, 40);
  img.deleteSprite();
  for (int i = 0; i < 3; i++){
    tft.pushImage(380, (i+1) * 80, 100, 80, menu[i]);
  }
  tempHeaders();
  dhtTemp(temp);
  dhtHumd(humd);
  goalTemp(holding, goal_temp);
  goalHumd(goal_humd);
}

/**
 * @brief This will be a list of sensors in the house and their respective output
 * 
 */
void Draw::rooms(){
  img.createSprite(480, 280);
  img.fillRect(0, 0, 480, 280, TFT_BLACK);
  //img.pushSprite(0, 40);
  //img.deleteSprite();
  //img.createSprite(380, 260);
  //img.fillRect(0, 0, 380, 260, TFT_BLACK);
  mainFont(img);
  img.setTextDatum(MC_DATUM);
  img.drawString("Rooms!", 190, 130);
  back(img);
  img.pushSprite(0, 40);
  img.deleteSprite();
}

/**
 * @brief Allows you to view the schedule for the currently select day
 * 
 * @param day_slots 
 * @param short_dow 
 */
void Draw::schedule(String slots[], String short_dow){
  img.createSprite(480, 280);
  img.fillRect(0,0,480,280,TFT_BLACK);
  mainFont(img);
  img.fillTriangle(40,20,60,0,60,40, TFT_WHITE);
  img.fillTriangle(200,20,180,0,180,40, TFT_WHITE);
  img.setTextDatum(MC_DATUM);
  img.drawString(short_dow, 120, 20);
  img.setTextDatum(ML_DATUM);
  tableFont(img);
  for(int i = 0; i < 10; i++){
    if(slots[i] == "") continue;
    img.drawString(slots[i], 20, 80+(i*40), GFXFF);
    img.drawCircle(285,70+(i*40),3,TFT_WHITE);
  }
  back(img);
  img.pushSprite(0,40);
  img.deleteSprite();
}

/**
 * @brief Displays the target humidity
 * 
 * @param goal_humd current target humidity
 */
void Draw::settings(boolean hold, float hold_temp, float goal_humd){
  img.createSprite(480, 280);
  img.fillRect(0,0,480,280,TFT_BLACK);
  // Write out headers
  secondFont(img);
  img.setTextDatum(MC_DATUM);
  img.drawString("Hold", 40, 50);
  img.drawString("Hold Temp", 155, 50);
  img.drawString("Humidity", 305, 50);
  
  // Draw out triangles and buttons
  mainFont(img);
  if(hold){
    img.drawString("ON", 40, 120);
  } else {
    img.drawString("OFF", 40, 120);
  }
  img.drawRoundRect(10, 90, 60, 60, 5, TFT_WHITE);
  img.fillTriangle(155,90,185,120,125,120, TFT_WHITE);
  img.fillTriangle(155,240,185,210,125,210, TFT_WHITE);
  img.fillTriangle(305,90,335,120,275,120, TFT_WHITE);
  img.fillTriangle(305,240,335,210,275,210, TFT_WHITE);
  img.drawString(String(hold_temp), 155, 160);
  String temp_str = String((int)goal_humd);
  temp_str += "%";
  img.drawString(temp_str, 305, 160);
  back(img);
  img.pushSprite(0,40);
  img.deleteSprite();
  
}

/**
 * @brief Draws out the current/target column headers
 * 
 */
void Draw::tempHeaders(){
  img.createSprite(360,30);
  secondFont(img);
  img.setTextDatum(ML_DATUM);
  img.drawString("current",20,15, GFXFF);
  img.drawString("target", 200, 15, GFXFF);
  img.pushSprite(0,150);
  img.deleteSprite();
}

/**
 * @brief Draws a back arrow, used in nested navigational screens
 * 
 */
void Draw::back(TFT_eSprite &img){
  //img.createSprite(80, 80);
  int start_x = 410;
  int start_y = 80;
  for(int i = 0; i < 25; i++){
    img.fillCircle(start_x + i, start_y - i, PENRADIUS, TFT_WHITE);
  }
  
  for(int i = 0; i < 25; i++){
    img.fillCircle(start_x + i, start_y + i, PENRADIUS, TFT_WHITE);
  }

  for(int i = 0; i < 50; i++){
    img.fillCircle( start_x + i, start_y, PENRADIUS, TFT_WHITE);
  }
}

/**
 * @brief draws the wifi logo according to strength
 * 
 * @param x center of wifi logo
 * @param y center of wifi logo
 * @param strength 0-3
 */
void Draw::wifi(int x, int y, int strength){
  uint16_t str_sig[3] = {0x39E7,0x39E7,0x39E7};
  for (int i = 0; i < strength; i++){
    str_sig[i] = TFT_WHITE; 
  }
  fillArc(x, y, 310, 17, 25, 30, 4, str_sig[2]);
  fillArc(x, y+5, 315, 15, 18, 25, 4, str_sig[1]);
  tft.fillCircle(x-1, y-7, 4, str_sig[0]);
}

/**
 * @brief I took this function from the TFT_espi library examples, used to draw arcs
 * 
 * @param x
 * @param y 
 * @param start_angle 
 * @param seg_count 
 * @param rx 
 * @param ry 
 * @param w 
 * @param colour 
 */
void Draw::fillArc(int x, int y, int start_angle, int seg_count, int rx, int ry, int w, unsigned int colour)
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

/**
 * @brief Retrieves the time from an NTP server and draws to screen
 * 
 */
void Draw::time(){
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

/**
 * @brief Draws out the given humidity
 * 
 * @param humd current humidity from sensor
 */
void Draw::dhtHumd(float humd){
  img.createSprite(180, 60);
  img.setTextDatum(ML_DATUM);
  mainFont(img);
  String humd_str = String(humd) + "%";
  img.drawString(humd_str, 5, 30, GFXFF);
  img.pushSprite(0, 240);
  img.deleteSprite();
}

/**
 * @brief Draws out the given temperature
 * 
 * @param temp current temperature from sensor
 */
void Draw::dhtTemp(float temp){
  img.createSprite(180, 60);
  img.setTextDatum(ML_DATUM);
  mainFont(img);
  String temp_str = String(temp) + " c";
  img.drawString(temp_str, 5, 30, GFXFF);
  img.pushSprite(0, 180);
  img.deleteSprite();
}

/**
 * @brief Draws out the target humidity
 * 
 * @param humd 
 */
void Draw::goalHumd(float humd){
  String goal_str = String(humd);
  img.createSprite(180, 60);
  mainFont(img);
  img.setTextDatum(ML_DATUM);
  img.drawString(goal_str, 0, 30, GFXFF);
  img.pushSprite(180,240);
  img.deleteSprite();
}

/**
 * @brief Draws out the target temperature
 * 
 * @param temp 
 */
void Draw::goalTemp(boolean holding, float temp){
  String goal_str = String(temp);
  img.createSprite(180,60);
  mainFont(img);
  img.setTextDatum(ML_DATUM);
  img.drawString(goal_str,0,30, GFXFF);
  if(holding){
    img.drawRoundRect(0, 0, 180, 60, 5, TFT_WHITE);
  }
  img.pushSprite(180,180);
  img.deleteSprite();
}

/**
 * Pre-determined fonts for ease of re-use
 * 
 */
void Draw::mainFont(TFT_eSprite& img){
  img.setFreeFont(FF26);
  img.setTextColor(TFT_WHITE);
  img.setTextSize(2);
}
void Draw::secondFont(TFT_eSprite& img){
  img.setFreeFont(FMO12);
  img.setTextColor(TFT_DARKGREY);
  img.setTextSize(1);
}
void Draw::headerFont(TFT_eSprite& img){
  img.setFreeFont(FF26);
  img.setTextColor(TFT_WHITE);
  img.setTextSize(1);
}
void Draw::tableFont(TFT_eSprite& img){
  img.setTextSize(2);
  img.setFreeFont(FM9);
  img.setTextColor(TFT_WHITE);
}

#endif