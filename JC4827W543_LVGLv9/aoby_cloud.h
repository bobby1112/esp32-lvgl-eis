//Version 1.2 
//Date 2024-05-08

#ifndef AOB_CLOUD_H
#define AOB_CLOUD_H

#include <WiFi.h>
#include <WebServer.h>
#include <HardwareSerial.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <esp_system.h> 

class Aoby_Cloud {
  private:
    String ssid;
    String password;
    HardwareSerial* CPUSerial;
    HardwareSerial* NXPSerial;
  public:
    Aoby_Cloud(HardwareSerial* CPUSerial);
    Aoby_Cloud(HardwareSerial* CPUSerial, HardwareSerial* NXPSerial);
    Aoby_Cloud(HardwareSerial* CPUSerial, String ssid, String password);
    void connect();
    void reconnect();
    bool isConnected();
    int send(const char* ccuid, const char* batteryid, double voltage, double temperature, double resistance_1k);
    int send_v2(const char* ccuid, const char* batteryid, int battery_index, double voltage, double temperature, double current, int noOfFreq, double** real, double** imaginary) ;
    int configure();
    int getInterval();
};

#endif