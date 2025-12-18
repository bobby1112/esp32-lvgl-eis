#ifndef UserMode_h
#define UserMode_h

#include <Arduino.h>
#include <lvgl.h>
#include "MeasurementMode.h"
#include "ButtonPanel.h"
#include "EISPlot.h"
#include "NXPPoint.h"
#include "LoggingQueue.h"

// Constants
#define MAX_DEV 2
#define MAX_FREQUENCIES 19

// Thread control
extern volatile bool stop_requested;

// State machine states
enum MeasurementState {
    STATE_IDLE,
    STATE_INITIALIZING,
    STATE_MEASURING_FREQUENCY,
    STATE_FREQUENCY_COMPLETE,
    STATE_SWEEP_COMPLETE,
    STATE_WAITING_60S,
    STATE_MEASUREMENT_COMPLETE,
    STATE_STREAMING_1KHZ
};

class UserMode {
public:
    UserMode(EISPlot* plot, NXPPoint* nxp);
    ~UserMode();
    
    // Set serial ports
    void setSerialPorts(HardwareSerial* nxpSerial, Stream* cpuSerial);
    
    // Set UI components
    void setButtonPanel(ButtonPanel* panel) { button_panel = panel; }
    
    // Set logging queue for Core 0 communication
    void setLoggingQueue(LoggingQueue* queue) { logging_queue = queue; }
    void enableLogging(bool enable) { logging_enabled = enable; }
    
    // Configuration
    void setMode(MeasurementMode mode) { current_mode = mode; }
    void setAverageCount(int count) { averaging_count = count; }
    
    // Main measurement control
    void startMeasurement();
    void stopMeasurement();
    bool isMeasurementActive() { return measurement_active; }

    void saveAllMode5DataToSD();
    void saveAllMode6DataToSD();
    
    // Update timers (called from measurement task on Core 1)
    void updateTimers();
    
private:
    // UI Components
    ButtonPanel* button_panel;
    EISPlot* eis_plot;
    NXPPoint* nxp_point;
    LoggingQueue* logging_queue;  // Queue for Core 0 communication
    HardwareSerial* nxp_serial;
    Stream* cpu_serial;
    
    // Measurement state
    MeasurementMode current_mode;
    int averaging_count;
    int num_devices;
    bool measurement_active;
    bool logging_enabled;
    
    // State machine
    MeasurementState measurement_state;
    unsigned long freq_measurement_start;
    unsigned long last_measurement_time;
    int sample_count_1khz;
    
    // Timing for modes 3 & 4
    unsigned long wait_start_time;
    int wait_seconds_remaining;
    bool waiting_60s;
    int current_cycle;
    
    // Progress tracking
    int current_freq_index;
    int current_sweep;
    int total_sweeps;
    int current_frequency;
    float progress_percentage;
    
    // Data storage
    double impedance_real[MAX_FREQUENCIES];
    double impedance_imag[MAX_FREQUENCIES];
    double accumulated_real[MAX_FREQUENCIES];
    double accumulated_imag[MAX_FREQUENCIES];
    double voltages[MAX_DEV];
    double temperatures[MAX_DEV];
    double voltage[MAX_DEV];
    unsigned int UID[MAX_DEV][4];
    
    // Frequency configuration
    static const int FREQ[MAX_FREQUENCIES];
    static const int FREQ_CMD_MAN[MAX_FREQUENCIES];
    static const int FREQ_CMD_EXP[MAX_FREQUENCIES];
    
    // State machine handlers
    void handleInitializing();
    void handleMeasuringFrequency();
    void handleFrequencyComplete();
    void handleSweepComplete();
    void handleWaiting60s();
    void handleMeasurementComplete();
    void handleStreaming1kHz();
    
    // Mode setup functions
    void setupMode1();  // Single Scan
    void setupMode2();  // Average Scan
    void setupMode3();  // Continuous
    void setupMode4();  // Average Loop
    void setupMode5();  // 1kHz Live
    void setupMode6();  // 1kHz Avg
    
    // Helper functions
    bool initializeDevices();
    bool measureSingleFrequencyNonBlocking(int freq_index, int target_device = 1); // mode 1 to 4
    bool measure1kHzSingleRead(int target_device = 1); // mode 5 & 6
    void calculateAverages();
    void startNextFrequency();
    void startWaiting60s();
    void updateProgressDisplay();
    bool checkStopRequested();
    void clearAccumulators();
    bool checkForDuplicates();
};

#endif