#ifndef EIS_LOGGER_H
#define EIS_LOGGER_H

#include <Arduino.h>
#include <SD.h>
#include <FS.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "MeasurementMode.h"

class EISLogger {
public:
    EISLogger();
    ~EISLogger();
    
    // Initialize the logger
    bool init();
    bool isReady() { return sd_ready; }
    
    // Main logging functions (thread-safe)
    bool logCompleteDataset(MeasurementMode mode, const int* frequencies, 
                           const double* real_impedances, const double* imag_impedances,
                           int num_points, double voltage = 0.0, double temperature = 0.0, 
                           int avg_count = 0, int cycle = 0);

    bool logComplete1kHzDataset(MeasurementMode mode, const double* all_real_data, 
                            const double* all_imag_data, int total_samples,
                            double voltage = 0.0, double temperature = 0.0, 
                            int avg_count = 0, int cycle = 0);

    bool logDataPoint(int sample_num, int frequency, double real_impedance, 
                     double imag_impedance, double voltage = 0.0, double temperature = 0.0);
    
    // File management (thread-safe)
    bool startNewLog(MeasurementMode mode, int cycle = 0);
    bool closeCurrentLog();
    
    // Utility functions (thread-safe)
    String generateFilename(MeasurementMode mode, int cycle = 0);
    String getModeString(MeasurementMode mode);
    void listLogFiles();
    bool deleteOldLogs(int days_old = 30);
    
    // Semaphore management
    static SemaphoreHandle_t getSDCardSemaphore() { return sd_card_semaphore; }
    
private:
    bool sd_ready;
    File current_log_file;
    String current_filename;
    bool file_open;
    
    // Semaphore for SD card access
    static SemaphoreHandle_t sd_card_semaphore;
    static bool semaphore_initialized;
    
    // Timeout for semaphore acquisition (ms)
    static const TickType_t SEMAPHORE_TIMEOUT = pdMS_TO_TICKS(1000);
    
    // Helper functions for organized storage
    bool createModeDirectories();
    String getModeDirectoryPath(MeasurementMode mode);
    String getModePrefix(MeasurementMode mode);
    void loadFileCounter();
    void saveFileCounter();
    String formatSimpleTimestamp();
    
    // Existing helper functions
    String sanitizeFilename(String filename);
    bool ensureDataDirectory();
    bool initializeSemaphore();
    bool acquireSDCardAccess();
    void releaseSDCardAccess();
};

#endif