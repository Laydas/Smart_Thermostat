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
#include <Preferences.h>
#include <Adafruit_FT6206.h>
#include "Free_Fonts.h"
#include "DHT.h"
#include "WiFi.h"
#include "time.h"

#include "Home_Icon.h"
#include "Cal_Icon.h"
#include "Gear_Icon.h"
#include "secrets.h"

#define DHTPIN 32
#define RELAY_1 33
#define RELAY_2 27
#define DHTTYPE DHT11
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


float old_t, old_h;

const uint16_t *menu[3] = {Home_Icon, Cal_Icon, Gear_Icon};
char* nav[4] = {"Main","Rooms","Schedule","Settings"};

/* 
  Schedule holds the information about each day of the week and all the
  configured temperatures. holds 10 individual temperature schedules,
  times are saved as a 24 hour clock with the target temp for that time.
*/ 
struct schedule {
  String day;
  uint8_t len = 0;
  struct temp_time {
    uint8_t hour;
    uint8_t minute;
    float_t temp;
  } times[10];
  
} schedule[7];

/*
  Keeps track of where within the schedule we are.
*/
struct CurrentBlock {
  int day;
  int slot;
} current_block;

/*
  Configure button placements
*/
const int next_dow[4] = {170, 20, 210, 80};
const int prev_dow[4] = {30, 20, 70, 80};

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

Preferences preferences;

/***********************************************************************************************************************************/
void setup() {
  Serial.begin(115200);
  pinMode(RELAY_1, OUTPUT);
  pinMode(RELAY_2, OUTPUT);
  heating(false);
  humidity(false);
  
  dht.begin();

  preferences.begin("schedule",false);

  readSchedule(preferences);

  initWiFi();
  
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
  
  drawNav(nav[nav_current], current_block);
  
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
    checkSchedule(current_block);
    // Turn heating/humidity on/off
    if(current - interval.prev_heat >= interval.intv_heat){
      if(old_t < schedule[current_block.day].times[current_block.slot].temp -1){
        heating(true);
      } else if(old_t > schedule[current_block.day].times[current_block.slot].temp + 1){
        heating(false);
      }

      if(old_h < 35){
        humidity(true);
      } else if (old_h > 40){
        humidity(false);
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
    drawNav(nav[new_nav], current_block);
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

void drawNav(char* screen, CurrentBlock &block){
  tft.fillRect(0,40,480,280,TFT_BLACK);
  if (screen == "Main"){
    drawMain(block);
  } else if (screen == "Rooms"){
    drawRooms();
  } else if (screen == "Schedule"){
    drawSchedule();
  } else if (screen == "Settings"){
    drawSettings();
  }
  checkWifi();
}

void drawMain(CurrentBlock &block){
  for (int i = 0; i < 3; i++){
    tft.pushImage(380, (i+1) * 80, 100, 80, menu[i]);
  }
  drawTempHeaders();
  drawDHTTemp(old_t);
  drawDHTHum(old_h);
  drawGoal(block);
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
  img.drawString(schedule[current_dow].day, 80, 40);
  img.pushSprite(40,35);
  img.deleteSprite();

  
  img.setTextDatum(ML_DATUM);
  
  img.createSprite(300,160);
  tableFont(img);
  for(int i = 0; i < schedule[current_dow].len; i++){
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
    img.drawString(temp_str, 0, 20+(i*40), GFXFF);
    img.drawCircle(265,10+(i*40),3,TFT_WHITE);
  }
  img.pushSprite(20, 100);
  img.deleteSprite();
  drawBack(0);
}

void drawSettings(){
  tft.setCursor(0,200,2);
  tft.setTextColor(TFT_WHITE,TFT_BLUE); tft.setTextSize(3);
  tft.setFreeFont(FMO12);
  tft.fillRect(0, 150, 265, 210, TFT_BLACK);
  tft.print("Settings!");
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
  img.createSprite(265, 60);
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
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  initSchedule(current_block);
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

void drawGoal(CurrentBlock &block){
  String goal_str = String(schedule[block.day].times[block.slot].temp);
  img.createSprite(180,60);
  img.setFreeFont(FF26);
  img.setTextColor(TFT_WHITE);
  img.setTextSize(2);
  img.setTextDatum(ML_DATUM);
  img.drawString(goal_str,0,30);
  img.pushSprite(180,180);
  img.deleteSprite();
}

void initSchedule(CurrentBlock &block){
  struct tm timeinfo;
  while(!getLocalTime(&timeinfo)){
    delay(100);
  }
  int tz[3];
  getTimeNow(tz);
  current_dow = tz[0];
  for(int i = 0; i < schedule[tz[0]].len; i++){
    if((tz[1]*60)+tz[2] < (schedule[tz[0]].times[i].hour*60) + schedule[tz[0]].times[i].minute){
      if(i == 0){
        block.day = (tz[0] + 6) % 7;
        block.slot = schedule[tz[0] - 1].len - 1;
        return;
      } else {
        block.day = tz[0];
        block.slot = i - 1;
        return;
      }
    } 
  }
  block.day = tz[0];
  block.slot = schedule[tz[0]].len - 1;
}

void checkSchedule(CurrentBlock &block){
  int tz[3];
  getTimeNow(tz);
  int next_hour,next_minute;
  if(block.slot + 1 == schedule[block.day].len){
    if((block.day + 1) % 7 != tz[0]) return;
    next_hour = schedule[(block.day+1)%7].times[0].hour;
    next_minute = schedule[(block.day+1)%7].times[0].minute;
  } else {
    next_hour = schedule[block.day].times[block.slot+1].hour;
    next_minute = schedule[block.day].times[block.slot+1].minute;
  }
  if((next_hour*60) + next_minute <= (tz[1]*60)+tz[2]){
    if(block.slot + 1 == schedule[block.day].len){
      block.day = (block.day + 1) % 7;
      block.slot = 0;
      drawGoal(block);
    } else {
      block.slot += 1;
      drawGoal(block);
    }
  }
}

int getTimeNow(int * ar){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    delay(100);
  }
  char dow[2];
  char hour[3];
  char minute[3];
  
  strftime(dow, 2, "%w", &timeinfo);
  strftime(hour, 3, "%H", &timeinfo);
  strftime(minute, 3, "%M", &timeinfo);

  ar[0] = String(dow).toInt();
  ar[1] = String(hour).toInt();
  ar[2] = String(minute).toInt();
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

void heating(boolean val){
  digitalWrite(RELAY_1, !val);
}

void humidity(boolean val){
  digitalWrite(RELAY_2, !val);
}

void writeSchedule(Preferences &prefs, char* days[]){
  String temp_sched = "8,0,22;23,0,19.5";
  prefs.putString(days[0],temp_sched);
  prefs.putString(days[6],temp_sched);
  temp_sched = "6,0,22.5;8,30,19.5;15,30,22.5;23,0,19.5";
  for(int i = 1; i < 6; i++){
    prefs.putString(days[i],temp_sched);
  }
}

void readSchedule(Preferences& prefs){
  char* full_days[7] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
  String sched_sun = prefs.getString(full_days[0],"");
  
  // If the schedule has never been saved then save it on setup
  if(sched_sun == ""){
    writeSchedule(prefs, full_days);
  } else {
    /* 
       Read the schedule from memory and put it into the schedule object
       schedules are saved as a non-nested key:value json like object
       Format of preferences schedule as below
       "<Day_of_week>": [ { <hour(int)>, <minute(int)>, <temperature(float)> }, {}]
    */
    char* dow[7] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    
    for(int i = 0; i < 7; i++){
      String get_sched = prefs.getString(full_days[i],"");
      int slot_count = 0;
      String t_s = "";
      String slots[10];
      for(int s = 0; s < get_sched.length(); s++){
        if(get_sched[s] == ';'){
          slots[slot_count] = t_s;
          slot_count ++;
          t_s = "";
          continue;
        }
        t_s += get_sched[s];
      }
      slots[slot_count] = t_s;
      slot_count++;
      schedule[i].day = dow[i];
      int p = 0;
      for(int s = 0; s < slot_count; s++){
        int temp_c = 0;
        String temp_v;
        for(int s_l = 0; s_l < slots[s].length(); s_l++){
          if(slots[s][s_l] == ','){
            if(temp_c == 0){
              schedule[i].times[s].hour = temp_v.toInt();
              temp_v = "";
              temp_c++;
              continue;
            } else if(temp_c == 1){
              schedule[i].times[s].minute = temp_v.toInt();
              temp_v = "";
              temp_c++;
              continue;
            }
          }
          temp_v += slots[s][s_l];
        }
        schedule[i].times[s].temp = temp_v.toFloat();
      }
      schedule[i].len = slot_count;
    }
  }
}
