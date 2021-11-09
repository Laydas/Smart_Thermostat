#ifndef THERMOSTAT_H
#define THERMOSTAT_H

#include <Preferences.h>

/**
 * @brief Holds all the logic for thermostat functions such as tracking a schedule and keeping the house warm
 * 
 */
class Thermostat {
  private:
    boolean heat_on = false;
    boolean humd_on = false;
    float target_humidity;
    char* dow[7] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    char* full_days[7] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
    int day;
    int heat;
    int humd;
    int screen_dow;
    int slot;
	  Preferences preferences;

    /**
     * Schedule holds the day, number of schedule slots, and schedulable slots
     * Slots contains the hour, minute, and temperature setting for each slot
     */
    struct Schedule {
      String day;
      uint8_t len = 0;
      struct Slot {
        uint8_t hour;
        uint8_t minute;
        float_t temp;
      } Slot[10]; // Maximum 10 slots
    } Schedule[7]; // 7 days of the week

    void initSchedule();

  public:
    Thermostat(int heatPin, int humdPin);
    char* getShortDow();
    float getGoalHumd();
    float getGoalTemp();
    int getSlot();
    int getSlotCount();
    int getTimeNow(int * ar);
    String getSlotInfo(int slot);
    
    void begin();

    void checkSchedule();
    void createSchedule(Preferences &prefs);
    void loadSchedule(Preferences& prefs);
    
    void prevDisplayDay();
    void nextDisplayDay();

    void keepTemperature(float temp);
    void keepHumidity(float humd);
    void setHeating(boolean val);
    void setHumidity(boolean val);
    void setTargetHumidity(float target);
};

/**
 * Private class functions
 */


/**
 * @brief Using the time from an ntp server, the thermostat determines the current day, hour, minute
 * 
 */
void Thermostat::initSchedule(){
  struct tm timeinfo;
  while(!getLocalTime(&timeinfo)){
    delay(100);
  }
  int tz[3];
  getTimeNow(tz);
  screen_dow = tz[0];
  for(int i = 0; i < Schedule[tz[0]].len; i++){
    if((tz[1]*60)+tz[2] < (Schedule[tz[0]].Slot[i].hour*60) + Schedule[tz[0]].Slot[i].minute){
      if(i == 0){
        day = (tz[0] + 6) % 7;
        slot = Schedule[tz[0] - 1].len - 1;
        return;
      } else {
        day = tz[0];
        slot = i - 1;
        return;
      }
    } 
  }
  day = tz[0];
  slot = Schedule[tz[0]].len - 1;
}


/**
 * Public class functions
 */


/**
 * @brief Construct a new Thermostat:: Thermostat object.
 * assign the heating and humidity relays to the correct pins
 * 
 * @param heatPin 
 * @param humdPin 
 */
Thermostat::Thermostat(int heatPin, int humdPin){
  heat = heatPin;
  humd = humdPin;
}


/**
 * @brief Returns the 3 letter day of the week for the currently displayed day
 * 
 * @return char* 
 */
char* Thermostat::getShortDow(){
  return dow[screen_dow];
}

/**
 * @brief Returns the current target humidity
 * 
 * @return float 
 */
float Thermostat::getGoalHumd(){
  return target_humidity;
}

/**
 * @brief Returns the current target temperature
 * 
 * @return float 
 */
float Thermostat::getGoalTemp(){
  return Schedule[day].Slot[slot].temp;
}

/**
 * @brief Returns the schedule slot that the thermostat is currently in
 * 
 * @return int 
 */
int Thermostat::getSlot(){
  return slot;
}


/**
 * @brief Returns the number of programmed schedules for the currently displayed day
 * 
 * @return int 
 */
int Thermostat::getSlotCount(){
  return Schedule[screen_dow].len;
}


/**
 * @brief Get the current timestamp as an integer array
 * 
 * @param ar 
 * @return int[3]
 */
int Thermostat::getTimeNow(int * ar){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    delay(100);
  }
  char dow[2]; // 0 - 6
  char hour[3]; // 0 - 23
  char minute[3]; // 0 - 59
  
  strftime(dow, 2, "%w", &timeinfo);
  strftime(hour, 3, "%H", &timeinfo);
  strftime(minute, 3, "%M", &timeinfo);

  ar[0] = String(dow).toInt();
  ar[1] = String(hour).toInt();
  ar[2] = String(minute).toInt();
}


/**
 * @brief Return the details about all the slots for the requested day
 * the format returns as HH:MM Temp (06:30 22.5)
 * 
 * @param slot 
 * @return String 
 */
String Thermostat::getSlotInfo(int slot){
  String temp_str;
  if (Schedule[screen_dow].Slot[slot].hour < 10){
    temp_str += "0";
  }
  temp_str += String(Schedule[screen_dow].Slot[slot].hour) + ":";
  if (Schedule[screen_dow].Slot[slot].minute < 10){
    temp_str += "0";
  }
  temp_str += String(Schedule[screen_dow].Slot[slot].minute);
  temp_str += "  " + String(Schedule[screen_dow].Slot[slot].temp);
  temp_str += "c";
  return temp_str;
}


/**
 * @brief Configures the heating and humidity relay pins and turns them to off so that the
 * furnace is stuck with heat on incase there is an issue on device startup. Loads
 * the schedule from preferences as well.
 * 
 */
void Thermostat::begin(){
  pinMode(heat, OUTPUT);
  pinMode(humd, OUTPUT);
  digitalWrite(heat, HIGH);
  digitalWrite(humd, HIGH);
  preferences.begin("schedule",false);
  loadSchedule(preferences);
  initSchedule();
}


/**
 * @brief Gets the current timestamp from an NTP server and then updates the
 * day and slot so that the correct temperature is set as the target.
 * 
 */
void Thermostat::checkSchedule(){
  int tz[3];
  getTimeNow(tz);
  int next_hour,next_minute;
  
  if(slot + 1 == Schedule[day].len){
    if( (day + 1) % 7 != tz[0]) 
      return;
    next_hour = Schedule[(day + 1) % 7].Slot[0].hour;
    next_minute = Schedule[(day + 1) % 7].Slot[0].minute;
  } else {
    next_hour = Schedule[day].Slot[slot + 1].hour;
    next_minute = Schedule[day].Slot[slot + 1].minute;
  }
  if( (next_hour * 60) + next_minute <= (tz[1] * 60) + tz[2]){
    if(slot + 1 == Schedule[day].len){
      day = (day + 1) % 7;
      slot = 0; 
    } else {
      slot += 1;
    }
  }
}


/**
 * @brief Write the default schedule to long term storage.
 * Currently this is hard-coded, this should instead take the Schedule struct
 * and then parse and write the schedule back into preferences.
 * 
 * As there isn't any function to input schedules into the screen at this time
 * this needs to stay hard-coded.
 * 
 * @param prefs 
 */
void Thermostat::createSchedule(Preferences &prefs){
  String temp_sched = "8,0,22;23,0,19.5";
  prefs.putString(full_days[0],temp_sched);
  prefs.putString(full_days[6],temp_sched);
  temp_sched = "6,0,22.5;8,30,19.5;15,30,22.5;23,0,19.5";
  for(int i = 1; i < 6; i++){
    prefs.putString(full_days[i],temp_sched);
  }
}


/**
 * @brief Reads the schedule for each day of the week from preferences and then parses it and
 * saves it into the Schedule struct.
 * 
 * @param prefs 
 */
void Thermostat::loadSchedule(Preferences& prefs){
  String sched_sun = prefs.getString(full_days[0],"");

  // If the schedule has never been saved then save it on setup
  if(sched_sun == ""){
    createSchedule(prefs);
  } else {
    /**
     * Read the schedule from memory and put it into the schedule object
     * schedules are saved as a non-nested key:value json like object
     * Format of preferences schedule as below
     * "<Day_of_week>": [ { <hour(int)>, <minute(int)>, <temperature(float)> }, {}]
     * 
     */
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
      Schedule[i].day = dow[i];
      int p = 0;
      for(int s = 0; s < slot_count; s++){
        int temp_c = 0;
        String temp_v;
        for(int s_l = 0; s_l < slots[s].length(); s_l++){
          if(slots[s][s_l] == ','){
            if(temp_c == 0){
              Schedule[i].Slot[s].hour = temp_v.toInt();
              temp_v = "";
              temp_c++;
              continue;
            } else if(temp_c == 1){
              Schedule[i].Slot[s].minute = temp_v.toInt();
              temp_v = "";
              temp_c++;
              continue;
            }
          }
          temp_v += slots[s][s_l];
        }
        Schedule[i].Slot[s].temp = temp_v.toFloat();
      }
      Schedule[i].len = slot_count;
    }
  }
}


/**
 * @brief Sets the display day to the previous day of the week
 * 
 */
void Thermostat::prevDisplayDay(){
  screen_dow = (screen_dow + 6) % 7;
}

/**
 * @brief Sets the display day to the next day of the week
 * 
 */
void Thermostat::nextDisplayDay(){
  screen_dow = (screen_dow + 1) % 7;
}


/**
 * --------HEATING FUNCTIONS--------
 */
 
/**
 * @brief Takes an input temperature and determines whether the furnace should
 * turn on or off.
 * 
 * @param temp 
 */
void Thermostat::keepTemperature(float temp){
  if(heat_on == true){
    if(Schedule[day].Slot[slot].temp + 0.5 < temp){
      digitalWrite(heat, HIGH); // Turn heat off
      heat_on = false;
    }
  } else {
    if(Schedule[day].Slot[slot].temp - 0.5 > temp){
      digitalWrite(heat, LOW); // Turn heat on
      heat_on = true;
    }
  }
}

/**
 * @brief Takes an input humidity and determines whether the humidifier should
 * turn on or off.
 * 
 * @param humd 
 */
void Thermostat::keepHumidity(float humd){
  if(humd_on == true){
    if(target_humidity + 2 < humd){
      digitalWrite(humd, HIGH); // Turn humidity off
      humd_on = false;
    }
  } else {
    if(target_humidity -2 > humd){
      digitalWrite(humd, LOW); // Turn humidity on
      humd_on = true;
    }
  }
}

/**
 * @brief Allows manually turning furnace on or off
 * 
 * @param val 
 */
void Thermostat::setHeating(boolean val){
  digitalWrite(heat, !val);
}

/**
 * @brief Allows manually turning humidifier on or off
 * 
 * @param val 
 */
void Thermostat::setHumidity(boolean val){
  digitalWrite(humd, !val);
}

/**
 * @brief Sets the target humidity
 * 
 * @param target 
 */
void Thermostat::setTargetHumidity(float target){
  target_humidity = target;
}

#endif
