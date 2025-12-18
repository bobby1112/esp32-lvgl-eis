#include "NXPPoint.h"
#include <Arduino.h>

float sinc2(float x) {
  if (x == 0) return 1;
  else return sin(PI * x) / (PI * x);
}

bool NXPPoint::isTransactionEnd(const char* data) {
  // The string to match
  const char* matchString = "--- End of transaction ---";

  if (strstr(data, matchString) != NULL) {
    return true; // Found the string
  } else {
    return false; // Did not find the string
  }
}

bool NXPPoint::isEnumerationEnd(const char* data) {
  // The string to match
  const char* matchString = "rc = 0,";

  if (strstr(data, matchString) != NULL) {
    return true; // Found the string
  } else {
    return false; // Did not find the string
  }
}

char* NXPPoint::lstrip(char* str) {
    if (str == NULL) {
        return NULL; // Handle NULL pointer
    }

    int start = 0;

    // Find the first non-whitespace character
    while (str[start] != '\0' && isspace(str[start])) {
        start++;
    }

    if (start > 0) {
        // Shift characters to the left
        int i = 0;
        while (str[i + start] != '\0') {
            str[i] = str[i + start];
            i++;
        }
        str[i] = '\0'; // Null-terminate the string
    }

    return str;
}

bool NXPPoint::isValidFormat(const char* str) {

    // Check if the pointer is null
    if (str == NULL) {    
        return false;
    }

    // Check the length
    int length = 0;
    while (str[length] != '\0') {
        length++;
    }

    if (length != 11) {
        return false;
    }

    // Check if string starts with "0x" or "0X"
    if (!(str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))) {
        return false;
    }

    // Check each character after "0x" for a valid hexadecimal digit
    for (int i = 2; i < 10; i++) {
        char c = str[i];
        bool isHexDigit = (c >= '0' && c <= '9') || 
                          (c >= 'A' && c <= 'F') || 
                          (c >= 'a' && c <= 'f');

        if (!isHexDigit) {
            return false;
        }
    }

    return true;
}

unsigned long NXPPoint::extractMantissa(const char* hexString) {
    unsigned long extract = (unsigned long)strtol(hexString, NULL, 16);
    return (extract & 0x000FFF0) >> 4;
}

unsigned long NXPPoint::extractExponent(const char* hexString) {
    unsigned long extract = (unsigned long)strtol(hexString, NULL, 16);
    return (extract & 0x00F0000) >> 16;
}

int NXPPoint::extractPOS(const char* hexString) {
    // Convert hex string to integer
    unsigned long extract = (unsigned long)strtol(hexString, NULL, 16);        
    unsigned long pos_neg = (extract & 0x00008000) >> 15;    
    if (pos_neg == 1) 
      return 1;
    else 
      return -1;

}


unsigned int NXPPoint::extractID(const char* hexString) {
    // Convert hex string to integer
    
    return strtol(hexString, NULL, 16);
}

unsigned long NXPPoint::extractVoltage(const char* hexString) {
    // Convert hex string to integer
    int extract = strtol(hexString, NULL, 16);
    return (extract & 0x003fff0) >> 4;
}

unsigned long NXPPoint::extractTemperature(const char* hexString) {
    // Convert hex string to integer
    int extract = strtol(hexString, NULL, 16);
    return (extract & 0x0000fff0) >> 6; // wklee, only use the 10-bit MSB
}


void NXPPoint::testNXP(HardwareSerial* NXPSerial, Stream* CPUSerial){
  NXPSerial->print("en\n");  
  static char _rx_buff[2048];
  while (NXPSerial->available())
  {
      // SerialP0.print(char(SerialPort.read()));
      int n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
      _rx_buff [n] = '\0'; // add null terminator
      CPUSerial->println(_rx_buff);
  }
}

void NXPPoint::UniqueID(HardwareSerial* NXPSerial,Stream* CPUSerial, unsigned int UID[][4], unsigned int num_devices){  
  NXPSerial->print("rtffe0009\n");	
  delay(100);
  char data[4];

  while (NXPSerial->available() ) { 
      char _rx_buff[2048];      
      int n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
      _rx_buff [n] = '\0'; // add null terminator           
      // CPUSerial->println(_rx_buff); 
      if (_rx_buff[0] == '=' && _rx_buff[1] == '=' && _rx_buff[2] == '>'){
            data[0] = _rx_buff[24];
            data[1]= _rx_buff[25];
            data[2]= _rx_buff[26];
            data[3]= _rx_buff[27];       
            UID[0][0] = extractID(data);
            for(int i=1; i<num_devices; i++)
            {
              n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
              _rx_buff [n] = '\0'; // add null terminator 
              // CPUSerial->println(_rx_buff); 
              // CPUSerial->println(n); 
              data[0] = _rx_buff[8];
              data[1]= _rx_buff[9];
              data[2]= _rx_buff[10];
              data[3]= _rx_buff[11];       
              UID[i][0] = extractID(data);
            }
            // CPUSerial->println (UID[0]);            
      }  
  }  

  NXPSerial->print("rtffe000a\n");	
  delay(100);
  while (NXPSerial->available() ) { 
      char _rx_buff[2048];      
      int n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
      _rx_buff [n] = '\0'; // add null terminator           
      if (_rx_buff[0] == '=' && _rx_buff[1] == '=' && _rx_buff[2] == '>'){
            data[0] = _rx_buff[24];
            data[1]= _rx_buff[25];
            data[2]= _rx_buff[26];
            data[3]= _rx_buff[27];
            UID[0][1] = extractID(data);
            for(int i=1; i<num_devices; i++)
            {
              n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
              _rx_buff [n] = '\0'; // add null terminator 
              // CPUSerial->println(_rx_buff); 
              // CPUSerial->println(n); 
              data[0]= _rx_buff[8];
              data[1]= _rx_buff[9];
              data[2]= _rx_buff[10];
              data[3]= _rx_buff[11];       
              UID[i][1] = extractID(data);
            }
            // CPUSerial->println (UID[1]);            
      }  
  }  

  NXPSerial->print("rtffe000b\n");	
  delay(100);
  while (NXPSerial->available() ) { 
      char _rx_buff[2048];      
      int n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
      _rx_buff [n] = '\0'; // add null terminator           
      if (_rx_buff[0] == '=' && _rx_buff[1] == '=' && _rx_buff[2] == '>'){
            data[0] = _rx_buff[24];
            data[1]= _rx_buff[25];
            data[2]= _rx_buff[26];
            data[3]= _rx_buff[27];
            UID[0][2] = extractID(data);
            for(int i=1; i<num_devices; i++)
            {
              n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
              _rx_buff [n] = '\0'; // add null terminator 
              data[0] = _rx_buff[8];
              data[1]= _rx_buff[9];
              data[2]= _rx_buff[10];
              data[3]= _rx_buff[11];       
              UID[i][2] = extractID(data);
            }
            // CPUSerial->println (UID[2]);            
      }  
  }  

  NXPSerial->print("rtffe000c\n");	
  delay(100);
  while (NXPSerial->available() ) { 
      char _rx_buff[2048];      
      int n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
      _rx_buff [n] = '\0'; // add null terminator           
      if (_rx_buff[0] == '=' && _rx_buff[1] == '=' && _rx_buff[2] == '>'){
            data[0] = _rx_buff[24];
            data[1]= _rx_buff[25];
            data[2]= _rx_buff[26];
            data[3]= _rx_buff[27];
            UID[0][3] = extractID(data);
            for(int i=1; i<num_devices; i++)
            {
              n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
              _rx_buff [n] = '\0'; // add null terminator               
              data[0]= _rx_buff[8];
              data[1]= _rx_buff[9];
              data[2]= _rx_buff[10];
              data[3]= _rx_buff[11];       
              UID[i][3] = extractID(data);
            }          
            // CPUSerial->println (UID[3]);            
      }  
  }  
}

void NXPPoint::configureNXPread(HardwareSerial* NXPSerial, const char* cmd1,char* cmd2, const char* cmd3){
  //NXPSerial->print("en\n");  
  // Delay of 1 second (1000 milliseconds)
  //delay(1000);
  // Send the other commands with 200 milliseconds delay between them
  NXPSerial->print(cmd1);
  delay(50);
  NXPSerial->print(cmd2);
  delay(50);
  NXPSerial->print(cmd3);
  delay(50);

  while (NXPSerial->available() )
  {
    char _rx_buff[2048];      
    int n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
    _rx_buff [n] = '\0'; // add null terminator  
      if (isTransactionEnd(_rx_buff)){
      // CPUSerial->println("End of Transaction detected");
        while (NXPSerial->available()) {
          NXPSerial->read();
        }
    }
  }
}

unsigned int NXPPoint::Enumerate(HardwareSerial* NXPSerial,Stream* CPUSerial){
 
  unsigned int num_devices = 0; 
  // Enumeration
  NXPSerial->print("en\n");	
  delay(200);
  String data = "";
  char cmd_buf[32];
  char _rx_buff[2048];  
  while (NXPSerial->available() ) {       
      int n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
      _rx_buff [n] = '\0'; // add null terminator     

      if (isEnumerationEnd(_rx_buff)){
        // CPUSerial->println("End of Enumeration");
        NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);        
        if (_rx_buff[1] == 'c' && _rx_buff[3] == '=' && _rx_buff[5] == '0')
        {
          for(int i=0; i<3; i++) data+= _rx_buff[8+i];
          num_devices = data.toInt();           
        }        
      }  
  }
  // Initialization
  snprintf(cmd_buf, sizeof(cmd_buf), "rtff110%02x", num_devices);
  CPUSerial->println(cmd_buf);
  NXPSerial->print(cmd_buf);
  delay(200);

  // while (NXPSerial->available() ) {    
  //     int n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
  //     _rx_buff [n] = '\0'; // add null terminator     
  //     if (_rx_buff[0] == '=' && _rx_buff[1] == '=' && _rx_buff[2] == '>'){
  //       for(int i=1; i<num_devices; i++)  // Skip first reading, not connected to battery.
  //       {
  //         n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
  //         _rx_buff [n] = '\0'; // add null terminator
  //         lstrip(_rx_buff);
  //         if(isValidFormat(_rx_buff)){
  //             CPUSerial->println(_rx_buff);                        
  //         }        
  //       }
  //     }
  //     if (isTransactionEnd(_rx_buff)){
  //       while (NXPSerial->available()) {
  //         NXPSerial->read();
  //       }
  //     }
  // }
  CPUSerial->println("Initialization completes.");
  return num_devices;
}

void NXPPoint::fixCRC(HardwareSerial* NXPSerial, Stream* CPUSerial, int num_devices){
  char _rx_buff[128]; 
  char format[64];
  for(int i=1; i<num_devices; i++)
  {
    snprintf(format, sizeof(format), "rtffaffff %d\n", i);   
    NXPSerial->print(format); 
    CPUSerial->println(format); 
    delay(200);
    // Clear all Service Registers.
    CPUSerial->println("Clear Service Registers."); 
    while (NXPSerial->available())
    {
      int n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
      _rx_buff [n] = '\0'; // add null terminator
      if (_rx_buff[0] == '=' && _rx_buff[1] == '=' && _rx_buff[2] == '>'){
        for(int i=1; i<num_devices; i++)  // Skip first reading, not connected to battery.
        {
          n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
          _rx_buff [n] = '\0'; // add null terminator
          lstrip(_rx_buff);
          if(isValidFormat(_rx_buff)){
              CPUSerial->println(_rx_buff);                        
          }        
        }
      }
      if (isTransactionEnd(_rx_buff)){
        while (NXPSerial->available()) {
          NXPSerial->read();
        }
      }
    }
  }  
  for(int i=1; i<num_devices; i++)
  {
    snprintf(format, sizeof(format), "rtffb0004 %d\n", i);   
    NXPSerial->print(format); 
    CPUSerial->println(format); 
    delay(200);
    // Clear all Service Registers.
    CPUSerial->println("Clear Service Registers."); 
    while (NXPSerial->available())
    {
      int n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
      _rx_buff [n] = '\0'; // add null terminator
      if (_rx_buff[0] == '=' && _rx_buff[1] == '=' && _rx_buff[2] == '>'){
        for(int i=1; i<num_devices; i++)  // Skip first reading, not connected to battery.
        {
          n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
          _rx_buff [n] = '\0'; // add null terminator
          lstrip(_rx_buff);
          if(isValidFormat(_rx_buff)){
              CPUSerial->println(_rx_buff);                        
          }        
        }
      }
      if (isTransactionEnd(_rx_buff)){
        while (NXPSerial->available()) {
          NXPSerial->read();
        }
      }
    } 
  }   
  // Set to Normal mode.
  CPUSerial->println("Set to Normal mode.");  
}

void NXPPoint::getStatus(HardwareSerial* NXPSerial, Stream* CPUSerial, int num_devices){
  char _rx_buff[128]; 
  CPUSerial->println("rtffd0002\n");  
  NXPSerial->print("rtffd0002\n");
  delay(200);
  while (NXPSerial->available())
  {
    int n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
    _rx_buff [n] = '\0'; // add null terminator
    if (_rx_buff[0] == '=' && _rx_buff[1] == '=' && _rx_buff[2] == '>'){
      for(int i=1; i<num_devices; i++)  // Skip first reading, not connected to battery.
      {
        n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
        _rx_buff [n] = '\0'; // add null terminator
        lstrip(_rx_buff);
        if(isValidFormat(_rx_buff)){
            CPUSerial->println(_rx_buff);                        
        }        
      }
    }
    if (isTransactionEnd(_rx_buff)){
      while (NXPSerial->available()) {
        NXPSerial->read();
      }
    }
  }  
  CPUSerial->println("rtffd0009\n");  
  NXPSerial->print("rtffd0009\n");
  delay(200);
  while (NXPSerial->available())
  {
    int n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
    _rx_buff [n] = '\0'; // add null terminator
    if (_rx_buff[0] == '=' && _rx_buff[1] == '=' && _rx_buff[2] == '>'){
      for(int i=1; i<num_devices; i++)  // Skip first reading, not connected to battery.
      {
        n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
        _rx_buff [n] = '\0'; // add null terminator
        lstrip(_rx_buff);
        if(isValidFormat(_rx_buff)){
            CPUSerial->println(_rx_buff);                        
        }        
      }
    }
    if (isTransactionEnd(_rx_buff)){
      while (NXPSerial->available()) {
        NXPSerial->read();
      }
    }
  }  
  CPUSerial->println("rtffd0012\n");  
  NXPSerial->print("rtffd0012\n");
  delay(200);
  while (NXPSerial->available())
  {
    int n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
    _rx_buff [n] = '\0'; // add null terminator
    if (_rx_buff[0] == '=' && _rx_buff[1] == '=' && _rx_buff[2] == '>'){
      for(int i=1; i<num_devices; i++)  // Skip first reading, not connected to battery.
      {
        n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
        _rx_buff [n] = '\0'; // add null terminator
        lstrip(_rx_buff);
        if(isValidFormat(_rx_buff)){
            CPUSerial->println(_rx_buff);                        
        }        
      }
    }
    if (isTransactionEnd(_rx_buff)){
      while (NXPSerial->available()) {
        NXPSerial->read();
      }
    }
  }  
  CPUSerial->println("rtffd0013\n");  
  NXPSerial->print("rtffd0013\n");
  delay(200);
  while (NXPSerial->available())
  {
    int n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
    _rx_buff [n] = '\0'; // add null terminator
    if (_rx_buff[0] == '=' && _rx_buff[1] == '=' && _rx_buff[2] == '>'){
      for(int i=1; i<num_devices; i++)  // Skip first reading, not connected to battery.
      {
        n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
        _rx_buff [n] = '\0'; // add null terminator
        lstrip(_rx_buff);
        if(isValidFormat(_rx_buff)){
            CPUSerial->println(_rx_buff);                        
        }        
      }
    }
    if (isTransactionEnd(_rx_buff)){
      while (NXPSerial->available()) {
        NXPSerial->read();
      }
    }
  }  
  CPUSerial->println("rtffd0014\n");  
  NXPSerial->print("rtffd0014\n");
  delay(200);
  while (NXPSerial->available())
  {
    int n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
    _rx_buff [n] = '\0'; // add null terminator
    if (_rx_buff[0] == '=' && _rx_buff[1] == '=' && _rx_buff[2] == '>'){
      for(int i=1; i<num_devices; i++)  // Skip first reading, not connected to battery.
      {
        n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
        _rx_buff [n] = '\0'; // add null terminator
        lstrip(_rx_buff);
        if(isValidFormat(_rx_buff)){
            CPUSerial->println(_rx_buff);                        
        }        
      }
    }
    if (isTransactionEnd(_rx_buff)){
      while (NXPSerial->available()) {
        NXPSerial->read();
      }
    }
  }    
  CPUSerial->println("rtffd0015\n");  
  NXPSerial->print("rtffd0015\n");
  delay(200);
  while (NXPSerial->available())
  {
    int n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
    _rx_buff [n] = '\0'; // add null terminator
    if (_rx_buff[0] == '=' && _rx_buff[1] == '=' && _rx_buff[2] == '>'){
      for(int i=1; i<num_devices; i++)  // Skip first reading, not connected to battery.
      {
        n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
        _rx_buff [n] = '\0'; // add null terminator
        lstrip(_rx_buff);
        if(isValidFormat(_rx_buff)){
            CPUSerial->println(_rx_buff);                        
        }        
      }
    }
    if (isTransactionEnd(_rx_buff)){
      while (NXPSerial->available()) {
        NXPSerial->read();
      }
    }
  }  
  CPUSerial->println("rtffd0016\n");  
  NXPSerial->print("rtffd0016\n");
  delay(200);
  while (NXPSerial->available())
  {
    int n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
    _rx_buff [n] = '\0'; // add null terminator
    if (_rx_buff[0] == '=' && _rx_buff[1] == '=' && _rx_buff[2] == '>'){
      for(int i=1; i<num_devices; i++)  // Skip first reading, not connected to battery.
      {
        n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
        _rx_buff [n] = '\0'; // add null terminator
        lstrip(_rx_buff);
        if(isValidFormat(_rx_buff)){
            CPUSerial->println(_rx_buff);                        
        }        
      }
    }
    if (isTransactionEnd(_rx_buff)){
      while (NXPSerial->available()) {
        NXPSerial->read();
      }
    }
  }  
  CPUSerial->println("rtffd0018\n");  
  NXPSerial->print("rtffd0018\n");
  delay(200);
  while (NXPSerial->available())
  {
    int n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
    _rx_buff [n] = '\0'; // add null terminator
    if (_rx_buff[0] == '=' && _rx_buff[1] == '=' && _rx_buff[2] == '>'){
      for(int i=1; i<num_devices; i++)  // Skip first reading, not connected to battery.
      {
        n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
        _rx_buff [n] = '\0'; // add null terminator
        lstrip(_rx_buff);
        if(isValidFormat(_rx_buff)){
            CPUSerial->println(_rx_buff);                        
        }        
      }
    }
    if (isTransactionEnd(_rx_buff)){
      while (NXPSerial->available()) {
        NXPSerial->read();
      }
    }
  }    
}

long NXPPoint::readNXPvalue_real(HardwareSerial *NXPSerial,Stream* CPUSerial, int num_devices, double voltage[], double impedances_real[], int ave_iter){
  double real = 0; 
  char cmd_buf[64];
  int pos_neg = 0;
  unsigned int mantissa=0, mantissa2=0;
  NXPSerial->print("rtffe0007\n");	
  delay(200);

  while (NXPSerial->available() ) { 
      char _rx_buff[2048];      
      int n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
      _rx_buff [n] = '\0'; // add null terminator
      // CPUSerial->println(_rx_buff);
      if (_rx_buff[0] == '=' && _rx_buff[1] == '=' && _rx_buff[2] == '>'){
        // CPUSerial->println(_rx_buff);
        for(int i=1; i<num_devices; i++)
        {
          n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
          _rx_buff [n] = '\0'; // add null terminator
          lstrip(_rx_buff);
          if(isValidFormat(_rx_buff)){
            // CPUSerial->println(_rx_buff);
            // CPUSerial->print("Real POS :");
            // CPUSerial->println(extractPOS(_rx_buff));
            // CPUSerial->print("mantissa :");
            // CPUSerial->println(extractMantissa(_rx_buff));
            // CPUSerial->print("exponent :");
            // CPUSerial->println(extractExponent(_rx_buff));                        
            
            double Vzm = voltage[i];
            double tmp = sinc2(1007 / 62500.0) * 141000000;
            double Zm_real = 0;
            pos_neg = extractPOS(_rx_buff);
            if (pos_neg==1)
            {              
              mantissa = (long)extractMantissa(_rx_buff);
              // Convert two's complement --> decimal
              mantissa2 = (mantissa ^ 0x00000fff) +1;
              real = mantissa2 * pow(2, (long)extractExponent(_rx_buff));
              Zm_real = -1 * (real * REXT) / (Vzm * tmp) * 1000 + CAL_IMP_1KHZ; // wklee, mili-ohm * 1000
            }
            else
            {
              mantissa = (long)extractMantissa(_rx_buff);              
              real = mantissa * pow(2, (long)extractExponent(_rx_buff));              
              Zm_real = (real * REXT) / (Vzm * tmp) * 1000 + CAL_IMP_1KHZ; // wklee, mili-ohm * 1000
            }
            
            if(ave_iter>0)  // wklee, skip the first iteration of averaging.
            {
              impedances_real[i] += Zm_real;
              impedances_real[i] /=2;
            }
            else
            {
              impedances_real[i] = Zm_real;
            }
            snprintf(cmd_buf, sizeof(cmd_buf), "Battery - %d: impedance (real): %.4f", i, Zm_real);
            CPUSerial->println (cmd_buf); 
          }
        }
      }

      if (isTransactionEnd(_rx_buff)){
        // CPUSerial->println("End of Transaction detected");
          while (NXPSerial->available()) {
            NXPSerial->read();
          }
      }
  }
  
  return real;
}

long NXPPoint::readNXPvalue_imaginary(HardwareSerial *NXPSerial,Stream* CPUSerial, int num_devices, double voltage[], double impedances_real[]){

  long imaginary = 0; 
  char cmd_buf[64];
  NXPSerial->print("rtffe0008\n");  
  delay(200);

  char _rx_buff[2048];
  while (NXPSerial->available())
  {
      // CPUSerial->println("i am inside readNXP_imaginary");
      int n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
      _rx_buff [n] = '\0'; // add null terminator

      if (_rx_buff[0] == '=' && _rx_buff[1] == '=' && _rx_buff[2] == '>'){
        // CPUSerial->println(_rx_buff);
        n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
        _rx_buff [n] = '\0'; // add null terminator
        for(int i=1; i<num_devices; i++)
        {
          lstrip(_rx_buff);
          if(isValidFormat(_rx_buff)){
            // CPUSerial->println(_rx_buff);
            // CPUSerial->print("Imag POS :");
            // CPUSerial->println(extractPOS(_rx_buff));
            // CPUSerial->print("mantissa :");
            // CPUSerial->println(extractMantissa(_rx_buff));
            // CPUSerial->print("exponent :");
            // CPUSerial->println(extractExponent(_rx_buff));
            imaginary = extractPOS(_rx_buff) * (long)extractMantissa(_rx_buff) * pow(2, (long)extractExponent(_rx_buff));
            double Vzm = voltage[i]*CAL_VOLT;
            float tmp = sinc2(1007 / 62500.0) * 141000000;
            float Zm_img = (imaginary * REXT) / (Vzm * tmp) * 1000; // wklee, mili-ohm * 1000
            impedances_real[i] = Zm_img + CAL_IMP_1KHZ;
            snprintf(cmd_buf, sizeof(cmd_buf), "Battery - %d: impedance (imaginary): %.4f", i, impedances_real[i]);
            CPUSerial->println (cmd_buf); 
          }
        }
      }

      if (isTransactionEnd(_rx_buff)){
        // CPUSerial->println("End of Transaction detected");
        while (NXPSerial->available()) {
          NXPSerial->read();
        }
      }

  }
  return imaginary;

}


void NXPPoint::readNXPvalue_voltage(HardwareSerial *NXPSerial,Stream* CPUSerial, unsigned int num_devices, double voltages[]){
  char cmd_buf[64];
  long volt = 0; 
  double Vzm = 0;  
  // CPUSerial->println("Submit command rtffe0006");
  NXPSerial->print("rtffe0000\n");    // Main ADC voltage
  delay(200);

  while (NXPSerial->available())
  {
      // CPUSerial->println("i am inside readNXP_voltage");
      char _rx_buff[2048]; 
      int n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
      _rx_buff [n] = '\0'; // add null terminator

      if (_rx_buff[0] == '=' && _rx_buff[1] == '=' && _rx_buff[2] == '>'){
        // CPUSerial->println(_rx_buff);
        for(int i=1; i<num_devices; i++)  // Skip first reading, not connected to battery.
        {
          n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
          _rx_buff [n] = '\0'; // add null terminator
          // CPUSerial->println(_rx_buff);
          lstrip(_rx_buff);
          if(isValidFormat(_rx_buff)){
              // CPUSerial->println(_rx_buff);
              // CPUSerial->print("Voltage :");
              volt = extractVoltage(_rx_buff);
              Vzm = 4.8 * (volt/16383.0) + 1.2;              
              voltages[i] = CAL_VOLT*Vzm;
              snprintf(cmd_buf, sizeof(cmd_buf), "Battery - %d: Batt voltage: %.4f", i, voltages[i] );
              CPUSerial->println (cmd_buf);            
          }        
        }
      }

      if (isTransactionEnd(_rx_buff)){
      // CPUSerial->println("End of Transaction detected");
        while (NXPSerial->available()) {
          NXPSerial->read();
        }
      }
  }  
}

void NXPPoint::readNXPvalue_voltageZM(HardwareSerial *NXPSerial,Stream* CPUSerial, unsigned int num_devices, double voltage[]){
  char cmd_buf[64];
  long volt = 0; 
  double Vzm = 0;
  // CPUSerial->println("Submit command rtffe0006");
  NXPSerial->print("rtffe0006\n");    // Main ADC voltage
  delay(200);

  while (NXPSerial->available())
  {
      // CPUSerial->println("i am inside readNXP_voltage");
      char _rx_buff[2048]; 
      int n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
      _rx_buff [n] = '\0'; // add null terminator

      if (_rx_buff[0] == '=' && _rx_buff[1] == '=' && _rx_buff[2] == '>'){
        // CPUSerial->println(_rx_buff);
        for(int i=1; i<num_devices; i++)  // Skip first reading, not connected to battery.
        {
          n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
          _rx_buff [n] = '\0'; // add null terminator
          // CPUSerial->println(_rx_buff);
          lstrip(_rx_buff);
          if(isValidFormat(_rx_buff)){
              // CPUSerial->println(_rx_buff);
              // CPUSerial->print("Voltage :");
              volt = extractVoltage(_rx_buff);
              Vzm = 4.8 * (volt/16383.0) + 1.2;
              voltage[i] = Vzm;
              snprintf(cmd_buf, sizeof(cmd_buf), "Battery - %d: impedance (voltage): %.4f", i, voltage[i] );
              CPUSerial->println (cmd_buf);            
          }        
        }
      }

      if (isTransactionEnd(_rx_buff)){
      // CPUSerial->println("End of Transaction detected");
        while (NXPSerial->available()) {
          NXPSerial->read();
        }
      }
  }  
}

long NXPPoint::readNXPvalue_temperature(HardwareSerial *NXPSerial,Stream* CPUSerial, int num_devices, double temperatures[])
{
  double temperature = 0; 
  char cmd_buf[64];
  NXPSerial->print("rtffe0004\n");  
  delay(200);

  while (NXPSerial->available())
  {
      char _rx_buff[2048]; 
      int n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
      _rx_buff [n] = '\0'; // add null terminator

      if (_rx_buff[0] == '=' && _rx_buff[1] == '=' && _rx_buff[2] == '>')
      {        
        for(int i=1; i<num_devices; i++)  // Skip first reading, not connected to battery.
        {
          n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
          _rx_buff [n] = '\0'; // add null terminator
          // CPUSerial->println(_rx_buff);
          lstrip(_rx_buff);
          if(isValidFormat(_rx_buff)){
              // CPUSerial->println(_rx_buff);
              // CPUSerial->print("Voltage :");
              temperature = extractTemperature(_rx_buff)*0.25;
              temperatures[i] = temperature;              
              snprintf(cmd_buf, sizeof(cmd_buf), "Battery - %d: temperature: %.2f", i,  temperatures[i]);
              CPUSerial->println (cmd_buf);            
          }        
        }
      }

      if (isTransactionEnd(_rx_buff)){      
        while (NXPSerial->available()) {
          NXPSerial->read();
        }
      }
  }
  return temperature;
}

void NXPPoint::readNXPvalue_current(HardwareSerial *NXPSerial,Stream* CPUSerial, double currents[])
{
  double sensor = 0; 
  char cmd_buf[64];
  NXPSerial->print("CR\n");  
  delay(200);

  while (NXPSerial->available())
  {
      char _rx_buff[256]; 
      int n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
      _rx_buff [n] = '\0'; // add null terminator
      // CPUSerial->println(_rx_buff);
      if (_rx_buff[0] == 'A' && _rx_buff[1] == 'D' && _rx_buff[2] == 'C')
      {        
        // Read only 3 sensors. 1st reading => current sensor; 2nd and 3rd are dummy sensors, not used.
        for(int i=0; i<3; i++)  
        {
          n = NXPSerial->readBytesUntil ('\n', _rx_buff, sizeof (_rx_buff)-1);
          _rx_buff [n] = '\0'; // add null terminator
          // CPUSerial->println(_rx_buff);
          sensor= atof(_rx_buff);
          currents[i] = sensor;
          snprintf(cmd_buf, sizeof(cmd_buf), "Sensor - %d: value: %.2f", i,  sensor);
          CPUSerial->println (cmd_buf);                      
        }
      }

      if (isTransactionEnd(_rx_buff)){      
         while (NXPSerial->available()) {
           NXPSerial->read();
         }
      }
  }
  
}


void NXPPoint::startBalancing(HardwareSerial* NXPSerial,Stream* CPUSerial, int id){
  char format[64];
  snprintf(format, sizeof(format), "rtff70c42 %d\n", id);   // set 1kHz frequency
  NXPSerial->print(format);    
  delay(200);
  snprintf(format, sizeof(format), "rtff81000 %d\n", id);   //enable bal. (SetBalCurr, bit-12), 12.5%, 132s timeout
  NXPSerial->print(format);  
  delay(200);
  snprintf(format, sizeof(format), "rtff90000 %d\n", id);   // time based balancing
  NXPSerial->print(format);  
  delay(200);
  snprintf(format, sizeof(format), "rtff60c01 %d\n", id);   // start balancing
  NXPSerial->print(format);  
  delay(100);
  CPUSerial->println("Pinis");
}


void NXPPoint::stopBalancing(HardwareSerial* NXPSerial,Stream* CPUSerial, int id){
  char format[32];
  snprintf(format, sizeof(format), "rtff60801 %d", id);   // stop balancing
  NXPSerial->print(format);  
  delay(100);
}