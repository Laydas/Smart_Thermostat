#ifndef THERMOSTAT_H
#define THERMOSTAT_H

#include <Preferences.h>

class Thermostat {
  private:
    char* full_days[7] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
    char* dow[7] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    int heat;
    int humd;
	  Preferences preferences;
    struct schedule {
      String day;
      uint8_t len = 0;
      struct temp_time {
        uint8_t hour;
        uint8_t minute;
        float_t temp;
      } times[10];
    } schedule[7];
  
    struct CurrentBlock {
      int day;
      int slot;
    } current_block;
    void initSchedule();

  public:
    int screen_dow;
    int day;
    int slot;
    void nextDaySched();
    void prevDaySched();
    Thermostat(int heatPin, int humdPin);
    String getSlotInfo(int slot);
    int getSlots();
    char* getShortDow();
    float goalTemp();
    
    int getSlot(){
      return current_block.slot;
    }
    int getDay(){
      return current_block.day;
    }
    void init();
    void readSchedule(Preferences& prefs);
    void checkSchedule();
    void writeSchedule(Preferences &prefs, char* days[]);
    int getTimeNow(int * ar);
};

void Thermostat::initSchedule(){
  struct tm timeinfo;
  while(!getLocalTime(&timeinfo)){
    delay(100);
  }
  int tz[3];
  getTimeNow(tz);
  screen_dow = tz[0];
  for(int i = 0; i < schedule[tz[0]].len; i++){
    if((tz[1]*60)+tz[2] < (schedule[tz[0]].times[i].hour*60) + schedule[tz[0]].times[i].minute){
      if(i == 0){
        day = (tz[0] + 6) % 7;
        slot = schedule[tz[0] - 1].len - 1;
        return;
      } else {
        day = tz[0];
        slot = i - 1;
        return;
      }
    } 
  }
  day = tz[0];
  slot = schedule[tz[0]].len - 1;
}

String Thermostat::getSlotInfo(int slot){
  String temp_str;
  if (schedule[screen_dow].times[slot].hour < 10){
    temp_str += "0";
  }
  temp_str += String(schedule[screen_dow].times[slot].hour) + ":";
  if (schedule[screen_dow].times[slot].minute < 10){
    temp_str += "0";
  }
  temp_str += String(schedule[screen_dow].times[slot].minute);
  temp_str += "  " + String(schedule[screen_dow].times[slot].temp);
  temp_str += "c";
  return temp_str;
}

int Thermostat::getSlots(){
  return schedule[screen_dow].len;
}

char* Thermostat::getShortDow(){
  return dow[screen_dow];
}

float Thermostat::goalTemp(){
  return schedule[day].times[slot].temp;
}

Thermostat::Thermostat(int heatPin, int humdPin){
  heat = heatPin;
  humd = humdPin;
}

void Thermostat::init(){
  pinMode(heat, OUTPUT);
  pinMode(humd, OUTPUT);
  digitalWrite(heat, HIGH);
  digitalWrite(humd, HIGH);
  preferences.begin("schedule",false);
  readSchedule(preferences);
  initSchedule();
}

void Thermostat::readSchedule(Preferences& prefs){
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

void Thermostat::checkSchedule(){
  int tz[3];
  getTimeNow(tz);
  int next_hour,next_minute;
  
  if(slot + 1 == schedule[day].len){
    if( (day + 1) % 7 != tz[0]) 
      return;
    next_hour = schedule[(day + 1) % 7].times[0].hour;
    next_minute = schedule[(day + 1) % 7].times[0].minute;
  } else {
    next_hour = schedule[day].times[slot + 1].hour;
    next_minute = schedule[day].times[slot + 1].minute;
  }
  if( (next_hour * 60) + next_minute <= (tz[1] * 60) + tz[2]){
    if(slot + 1 == schedule[day].len){
      day = (day + 1) % 7;
      slot = 0; 
    } else {
      slot += 1;
    }
  }
}

void Thermostat::writeSchedule(Preferences &prefs, char* days[]){
  String temp_sched = "8,0,22;23,0,19.5";
  prefs.putString(days[0],temp_sched);
  prefs.putString(days[6],temp_sched);
  temp_sched = "6,0,22.5;8,30,19.5;15,30,22.5;23,0,19.5";
  for(int i = 1; i < 6; i++){
    prefs.putString(days[i],temp_sched);
  }
}

int Thermostat::getTimeNow(int * ar){
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
void Thermostat::prevDaySched(){
  screen_dow = (screen_dow + 6) % 7;
}

void Thermostat::nextDaySched(){
  screen_dow = (screen_dow + 1) % 7;
}

#endif
