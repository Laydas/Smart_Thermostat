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
#include "DHT.h"
#include "WiFi.h"
#include <Adafruit_FT6206.h>

#include "Draw.h"
#include "Thermostat.h"
#include "secrets.h"

#define DHTPIN 32
#define HEATPIN 33
#define HUMDPIN 27
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);
Adafruit_FT6206 ts = Adafruit_FT6206();

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
Draw draw = Draw();

/**
 * @brief Holds the current (or old) temperature. Used to compare against incoming
 * sensor readings and used to pass temps to other class methods
 * 
 */
struct old{
  float temp;
  float humd;
} old;

// Create a button object using the 4 corner coordinates
struct Button {
  int x, y, x2, y2;
  Button(int x, int x2, int y, int y2):x(x), x2(x2), y(y), y2(y2){}
};

// Define where each button is going to go on the screen
struct Layout {
  Button next_dow = Button(170, 210, 20, 80);
  Button prev_dow = Button(30, 70, 20, 80);
  Button menu_bar = Button(380, 480, 80, 320);
  Button menu_rooms = Button(380, 480, 80, 160);
  Button menu_sched = Button(380, 480, 160, 240);
  Button menu_setting = Button(380,480, 240, 320);
  Button hold = Button(0, 80, 130, 190);
  Button up_hold = Button(80, 230, 110, 215);
  Button down_hold = Button(80, 230, 215, 320);
  Button up_humd = Button(230, 380, 110, 215);
  Button down_humd = Button(230, 380, 215, 320);
} Layout;

char* nav[4] = {"Main","Rooms","Schedule","Settings"};

// Internet and NTP information
const char* ssid = SSID_NAME;
const char* password = SSID_PASS;
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -25200;
const int daylightOffset_sec = 3600;

int nav_current = 0;


void setup() {
  // Initialization of all the classes/objects needed
  Serial.begin(115200);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  dht.begin();
  initWiFi();
  thermostat.begin();

  if (!ts.begin(18, 19, 40)) {
    Serial.println("Couldn't start touchscreen controller");
    while (true);
  }

  draw.begin();
  // Draw the main landing screen
  draw.main(old.temp, old.humd, thermostat.getGoalTemp(), thermostat.getGoalHumd(), thermostat.getHold());
}

void loop() {
  // Update the onboard temp/humidity every 2 seconds. This might be a bit aggressive.
  unsigned long current = millis();
  if(current - interval.prev >= interval.intv){
    interval.prev = current;
    // Update the sensor readings
    old.temp = getDHTTemp(old.temp, nav[nav_current]);
    old.humd = getDHTHum(old.humd, nav[nav_current]);
    // Draw the date string at the top of the screen
    draw.time();

    // If the time has moved into a new scheduled slot then draw the goal temp again
    if(thermostat.checkSchedule()){
      draw.goalTemp(thermostat.getHold(), thermostat.getGoalTemp());
    }

    // Turn heating/humidity on/off every 5 minutes ( to avoid constantly turning on furnace )
    if(current - interval.prev_heat >= interval.intv_heat){
      thermostat.keepTemperature(old.temp);
      thermostat.keepHumidity(old.humd);
      interval.prev_heat = current;
    }
    
    // Attempt to reconnect to wifi if disconnected
    checkWifi();
    if((WiFi.status() != WL_CONNECTED) && (current - interval.prev_wifi >= interval.intv_wifi)){
      WiFi.disconnect();
      WiFi.reconnect();
      interval.prev_wifi = current;
    }
    interval.prev = current;
  }
    
  // Restart loop if the screen hasn't been touched
  if (! ts.touched()) {
    return;
  }

  handleTouch(ts.getPoint(), nav[nav_current]);

  delay(250); // Delay to reduce loop rate (reduces flicker caused by aliasing with TFT screen refresh rate)
}

/**
 * @brief Handle input from screen
 * 
 * @param p 
 * @param screen 
 */
void handleTouch(TS_Point p, char* screen){
  int y = p.x;
  int x = map(p.y, 0, 480, 480, 0);

  // Handle all buttons that would appear on the main screen
  if (screen == "Main"){
    // Check to see if the touch was inside the menu bar (for now it's the only buttons on main anyways)
    if(isButton(x, y, Layout.menu_bar)){
      if(isButton(x, y, Layout.menu_rooms)){
        nav_current = 1;
        draw.rooms();
      } else if(isButton(x, y, Layout.menu_sched)){
        nav_current = 2;
        String slots[10];
        thermostat.daySlots(slots);
        draw.schedule(slots, thermostat.getShortDow());
      } else if(isButton(x, y, Layout.menu_setting)){
        nav_current = 3;
        draw.settings(thermostat.getHold(), thermostat.getHoldTemp(), thermostat.getGoalHumd());
      }
    }
  } else {
    // Check to see if the back button was pressed on the other screens
    if(isButton(x,y, Layout.menu_bar)){
      if(isButton(x, y, Layout.menu_rooms)){
        nav_current = 0;
        draw.main(old.temp, old.humd, thermostat.getGoalTemp(), thermostat.getGoalHumd(), thermostat.getHold());
      }
    }
  }

  // Settings has most of the buttons right now, this handles the control of holding a temp or setting
  // the current humidity goal
  if(screen == "Settings"){
    boolean touched_button = false;
    if(isButton(x, y, Layout.up_humd)){
      thermostat.setTargetHumidity(thermostat.getGoalHumd() + 1);
      touched_button = true;
    }
    if(isButton(x, y, Layout.down_humd)){
      thermostat.setTargetHumidity(thermostat.getGoalHumd() - 1);
      touched_button = true;
    }
    if(isButton(x, y, Layout.up_hold)){
      thermostat.setHoldTemp(thermostat.getHoldTemp() + 0.5f);
      touched_button = true;
    }
    if(isButton(x, y, Layout.down_hold)){
      thermostat.setHoldTemp(thermostat.getHoldTemp() - 0.5f);
      touched_button = true;
    }
    if(isButton(x, y, Layout.hold)){
      thermostat.toggleHold();
      touched_button = true;
    }
    // Only redraw the screen when a change has been made/button touched
    if(touched_button) 
      draw.settings(thermostat.getHold(), thermostat.getHoldTemp(), thermostat.getGoalHumd());
  }

  // Navigate through to view the weeks schedule
  if(screen == "Schedule"){
    if(isButton(x, y, Layout.prev_dow)){
      thermostat.prevDisplayDay();
      String slots[10];
      thermostat.daySlots(slots);
      draw.schedule(slots, thermostat.getShortDow());
    }
    if (isButton(x, y, Layout.next_dow)){
      thermostat.nextDisplayDay();
      String slots[10];
      thermostat.daySlots(slots);
      draw.schedule(slots, thermostat.getShortDow());
    }
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

// Update the screen with the temperature
float getDHTTemp(float old_temp, char* screen){
  float temp = dht.readTemperature();
  if (temp != old_temp && screen == "Main"){
    draw.dhtTemp(temp);
  }
  return temp;
}

// Update the screen with the humidity
float getDHTHum(float old_humd, char* screen){
  float humd = dht.readHumidity();
  if (humd != old_humd && screen == "Main"){
    draw.dhtHumd(humd);
  }
  return humd;
}

// Turn on the wifi and connect
void initWiFi(){
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

/**
 * @brief Check the strength of the wifi and call the draw.wifi with the current strength
 * 
 */
void checkWifi(){
  if(WiFi.status() != WL_CONNECTED){
    draw.wifi(455,35,0);
    return;
  }
  int strength = WiFi.RSSI();
  if(strength > -50){
    draw.wifi(455, 35, 3);
  } else if(strength > -70){
    draw.wifi(455, 35, 2);
  } else {
    draw.wifi(455, 35, 1);
  }
}