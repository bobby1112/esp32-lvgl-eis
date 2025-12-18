// MyDataStruct.h
#ifndef NXPPoint_h
#define NXPPoint_h

#include <cstring>
#include <HardwareSerial.h>
#include <math.h>

class NXPPoint {

public:
    void testNXP(HardwareSerial *NXPSerial,Stream* CPUSerial);
    void configureNXPread(HardwareSerial *NXPSerial, const char* command1,char* command2, const char* command3);
    long readNXPvalue_real(HardwareSerial *NXPSerial,Stream* CPUSerial, int num_devices, double voltage[], double impedances_real[], int ave_iter);
    long readNXPvalue_imaginary(HardwareSerial *NXPSerial,Stream* CPUSerial, int num_devices, double voltage[], double impedances_real[]);
    void readNXPvalue_voltage(HardwareSerial *NXPSerial,Stream* CPUSerial, unsigned int num_devices, double voltages[]);
    void readNXPvalue_voltageZM(HardwareSerial *NXPSerial,Stream* CPUSerial, unsigned int num_devices, double voltage[]);
    long readNXPvalue_temperature(HardwareSerial *NXPSerial,Stream* CPUSerial, int num_devices, double temperatures[]);
    unsigned int Enumerate(HardwareSerial* NXPSerial,Stream* CPUSerial);
    void UniqueID(HardwareSerial* NXPSerial,Stream* CPUSerial, unsigned int UID[][4], unsigned int num_devices);    
    void startBalancing(HardwareSerial* NXPSerial, Stream* CPUSerial, int id);
    void stopBalancing(HardwareSerial* NXPSerial,Stream* CPUSerial, int id);
    void readNXPvalue_current(HardwareSerial *NXPSerial,Stream* CPUSerial, double currents[]);
    void getStatus(HardwareSerial* NXPSerial, Stream* CPUSerial, int num_devices);
    void fixCRC(HardwareSerial* NXPSerial, Stream* CPUSerial, int num_devices);
    
// wklee, store calibration data
    double CAL_IMP_1KHZ = 24.8149;
    double CAL_VOLT     = 3;  // 2V==> 0.5, 4V ==> 1, 12V ==> 3
    double REXT = 56;         // 2V==> 10 ohm, 4V ==> 20 ohm, 12V ==> 56 ohm
private:
    bool isTransactionEnd(const char* data);
    bool isEnumerationEnd(const char* data);
    char* lstrip(char* str);
    bool isValidFormat(const char* str);
    unsigned long extractMantissa(const char* hexString);
    unsigned long extractExponent(const char* hexString);
    int extractPOS(const char* hexString);
    unsigned long extractVoltage(const char* hexString);
    unsigned long extractTemperature(const char* hexString);
    unsigned int extractID(const char* hexString);
};

#endif
