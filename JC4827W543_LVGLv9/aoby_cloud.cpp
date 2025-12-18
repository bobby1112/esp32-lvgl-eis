//Version 1.2
//Date 2024-05-08
#include "aoby_cloud.h"

Aoby_Cloud::Aoby_Cloud(HardwareSerial* CPUSerial, String ssid, String password) {
  this->ssid = ssid;
  this->password = password;
  this->CPUSerial = CPUSerial;
  this->NXPSerial = NXPSerial;
}

Aoby_Cloud::Aoby_Cloud(HardwareSerial* CPUSerial, HardwareSerial* NXPSerial) {
  this->CPUSerial = CPUSerial;  
  this->NXPSerial = NXPSerial;
}

void Aoby_Cloud::connect() {

  Preferences preferences;
  preferences.begin("aoby_cloud", false); 
  this->ssid = preferences.getString("ssid", "");
  this->password = preferences.getString("password", "");
  CPUSerial->println("SSID: " + this->ssid);  
  CPUSerial->println("Password: " + this->password);
  preferences.end();

  WiFi.begin(ssid.c_str(), password.c_str());
  CPUSerial->print("Connecting to WiFi");
  int count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    CPUSerial->print(".");
    this->configure();
    count++;
    if (count > 10) {
      esp_restart();
      break;
    }
  }
  CPUSerial->println("");
  CPUSerial->println("Connected to WiFi");
  CPUSerial->print("IP Address: ");
  CPUSerial->println(WiFi.localIP());
}


void Aoby_Cloud::reconnect() {
    // Check if the ESP32 is connected to WiFi
    if (WiFi.status() != WL_CONNECTED) {
        CPUSerial->println("Reconnecting to WiFi...");
        WiFi.disconnect();
        WiFi.reconnect();
        int count = 0;
        // Attempt to reconnect every 500 milliseconds
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            CPUSerial->print(".");
            this->configure();
            count++;
            if (count > 10) {
              esp_restart();
              break;
            }
            
        }
        CPUSerial->println();
        CPUSerial->println("WiFi reconnected!");
    }
}

int Aoby_Cloud::send(const char* ccuid, const char* batteryid, double voltage, double temperature, double resistance_1k) {


  return 0;
}


int Aoby_Cloud::send_v2(const char* ccuid, const char* batteryid, int battery_index, double voltage, double temperature, double current, int noOfFreq, double** real, double** imaginary) 
{
 
  return 0;

}


int Aoby_Cloud::configure(){

  
  
  return 0; 
}

int Aoby_Cloud::getInterval() {
  Preferences preferences;
  preferences.begin("aoby_cloud", false);
  int interval = preferences.getInt("interval", 0);
  preferences.end();
  return interval;
}