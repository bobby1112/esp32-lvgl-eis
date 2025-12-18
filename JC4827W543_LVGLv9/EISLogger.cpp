#include "EISLogger.h"

// Static semaphore initialization
SemaphoreHandle_t EISLogger::sd_card_semaphore = nullptr;
bool EISLogger::semaphore_initialized = false;

// Static counter for unique filenames
static uint16_t file_counter = 1;

EISLogger::EISLogger() : sd_ready(false), file_open(false) {
    current_filename = "";
}

EISLogger::~EISLogger() {
    if (file_open) {
        closeCurrentLog();
    }
}

bool EISLogger::init() {
    // Initialize semaphore first
    if (!initializeSemaphore()) {
        Serial.println("EISLogger: Failed to initialize semaphore");
        return false;
    }
    
    // Acquire SD card access
    if (!acquireSDCardAccess()) {
        Serial.println("EISLogger: Failed to acquire SD card access");
        return false;
    }
    
    // Check if SD card is available
    if (!SD.begin()) {
        Serial.println("EISLogger: SD card not available");
        sd_ready = false;
        releaseSDCardAccess();
        return false;
    }
    
    // Create organized directory structure
    if (!createModeDirectories()) {
        Serial.println("EISLogger: Failed to create mode directories");
        sd_ready = false;
        releaseSDCardAccess();
        return false;
    }
    
    // Read existing file counter from SD card
    loadFileCounter();
    
    sd_ready = true;
    releaseSDCardAccess();
    Serial.println("EISLogger: Initialized with organized folder structure");
    
    return true;
}

bool EISLogger::createModeDirectories() {
    // Create main data directory
    if (!SD.exists("/data")) {
        if (!SD.mkdir("/data")) {
            Serial.println("Failed to create /data directory");
            return false;
        }
    }
    
    // Create subdirectories for each mode
    const char* mode_dirs[] = {
        "/data/mode1",  // Single Scan
        "/data/mode2",  // Average Scan
        "/data/mode3",  // Continuous
        "/data/mode4",  // Average Loop
        "/data/mode5",  // 1kHz Live
        "/data/mode6"   // 1kHz Average
    };
    
    for (int i = 0; i < 6; i++) {
        if (!SD.exists(mode_dirs[i])) {
            if (!SD.mkdir(mode_dirs[i])) {
                Serial.printf("Failed to create directory: %s\n", mode_dirs[i]);
                return false;
            } else {
                Serial.printf("Created directory: %s\n", mode_dirs[i]);
            }
        }
    }
    
    Serial.println("All mode directories ready");
    return true;
}

void EISLogger::loadFileCounter() {
    // Try to read counter from a file
    File counterFile = SD.open("/data/counter.txt", FILE_READ);
    if (counterFile) {
        String counterStr = counterFile.readString();
        file_counter = counterStr.toInt();
        counterFile.close();
        Serial.printf("Loaded file counter: %d\n", file_counter);
    } else {
        file_counter = 1;
        Serial.println("Counter file not found, starting from 1");
    }
}

void EISLogger::saveFileCounter() {
    File counterFile = SD.open("/data/counter.txt", FILE_WRITE);
    if (counterFile) {
        counterFile.println(file_counter);
        counterFile.close();
    }
}

String EISLogger::formatSimpleTimestamp() {
    // Simple timestamp based on millis() converted to hours:minutes:seconds
    unsigned long ms = millis();
    unsigned long seconds = ms / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    
    seconds = seconds % 60;
    minutes = minutes % 60;
    hours = hours % 24;
    
    char timestamp[16];
    snprintf(timestamp, sizeof(timestamp), "%02lu%02lu%02lu", hours, minutes, seconds);
    
    return String(timestamp);
}

String EISLogger::getModeDirectoryPath(MeasurementMode mode) {
    switch (mode) {
        case MODE_SINGLE_SCAN:   return "/data/mode1";
        case MODE_AVERAGE_SCAN:  return "/data/mode2";
        case MODE_CONTINUOUS:    return "/data/mode3";
        case MODE_AVERAGE_LOOP:  return "/data/mode4";
        case MODE_1KHZ_LIVE:     return "/data/mode5";
        case MODE_1KHZ_AVG:      return "/data/mode6";
        default:                 return "/data";
    }
}

String EISLogger::getModePrefix(MeasurementMode mode) {
    switch (mode) {
        case MODE_SINGLE_SCAN:   return "EIS_SingleScan";
        case MODE_AVERAGE_SCAN:  return "EIS_AvgScan";
        case MODE_CONTINUOUS:    return "EIS_Continuous";
        case MODE_AVERAGE_LOOP:  return "EIS_AvgLoop";
        case MODE_1KHZ_LIVE:     return "EIS_1kHz_Live";
        case MODE_1KHZ_AVG:      return "EIS_1kHz_Avg";
        default:                 return "EIS_Unknown";
    }
}

String EISLogger::generateFilename(MeasurementMode mode, int cycle) {
    String timestamp = formatSimpleTimestamp();
    String modePrefix = getModePrefix(mode);
    String filename;
    
    if (cycle > 0) {
        filename = modePrefix + "_C" + String(cycle) + "_" + String(file_counter, DEC) + "_" + timestamp + ".csv";
    } else {
        filename = modePrefix + "_" + String(file_counter, DEC) + "_" + timestamp + ".csv";
    }
    
    // Increment counter for next file
    file_counter++;
    saveFileCounter();
    
    return sanitizeFilename(filename);
}

bool EISLogger::logCompleteDataset(MeasurementMode mode, const int* frequencies, 
                                  const double* real_impedances, const double* imag_impedances,
                                  int num_points, double voltage, double temperature, 
                                  int avg_count, int cycle) {
    if (!sd_ready) return false;
    
    // Acquire exclusive access to SD card
    if (!acquireSDCardAccess()) {
        Serial.println("EISLogger: Failed to acquire SD card access for dataset logging");
        return false;
    }
    
    bool success = false;
    
    // Generate filepath with mode directory
    String modeDir = getModeDirectoryPath(mode);
    String filename = generateFilename(mode, cycle);
    String filepath = modeDir + "/" + filename;
    
    File logFile = SD.open(filepath, FILE_WRITE);
    if (!logFile) {
        Serial.printf("EISLogger: Failed to open %s\n", filepath.c_str());
        releaseSDCardAccess();
        return false;
    }
    
    // Write header
    String header = "RealImpedance_mOhm,ImagImpedance_mOhm\n";
    logFile.print(header);
    
    // Write data points (only impedance values)
    for (int i = 0; i < num_points; i++) {
        String line = String(real_impedances[i], 4) + "," + String(imag_impedances[i], 4) + "\n";
        logFile.print(line);
    }
    
    logFile.flush();
    logFile.close();
    success = true;
    
    Serial.printf("EISLogger: Logged %d points to %s\n", num_points, filepath.c_str());
    
    releaseSDCardAccess();
    return success;
}

// bool EISLogger::logComplete1kHzDataset(MeasurementMode mode, const double* all_real_data, 
//                                      const double* all_imag_data, int total_samples,
//                                      double voltage, double temperature, int avg_count, int cycle) {
//     if (!sd_ready) return false;
    
//     // Acquire exclusive access to SD card
//     if (!acquireSDCardAccess()) {
//         Serial.println("EISLogger: Failed to acquire SD card access for 1kHz dataset logging");
//         return false;
//     }
    
//     bool success = false;
    
//     // Generate filepath with mode directory
//     String modeDir = getModeDirectoryPath(mode);
//     String filename = generateFilename(mode, cycle);
//     String filepath = modeDir + "/" + filename;
    
//     File logFile = SD.open(filepath, FILE_WRITE);
//     if (!logFile) {
//         Serial.printf("EISLogger: Failed to open %s\n", filepath.c_str());
//         releaseSDCardAccess();
//         return false;
//     }
    
//     // Write header with metadata
//     String header;
//     if (mode == MODE_1KHZ_LIVE) {
//         header = "# Mode 5: 1kHz Live Stream Data\n";
//         header += "# Total Samples: " + String(total_samples) + "\n";
//         header += "# Voltage: " + String(voltage, 3) + " V\n";
//         header += "# Temperature: " + String(temperature, 1) + " C\n";
//         header += "SampleNumber,RealImpedance_mOhm,ImagImpedance_mOhm\n";
//     } else if (mode == MODE_1KHZ_AVG) {
//         header = "# Mode 6: 1kHz Averaged Data\n";
//         header += "# Averaging Count: " + String(avg_count) + "\n";
//         header += "# Total Individual Samples: " + String(total_samples) + "\n";
//         header += "# Voltage: " + String(voltage, 3) + " V\n";
//         header += "# Temperature: " + String(temperature, 1) + " C\n";
//         header += "SampleNumber,RealImpedance_mOhm,ImagImpedance_mOhm\n";
//     }
    
//     logFile.print(header);
    
//     // Write all data points
//     for (int i = 0; i < total_samples; i++) {
//         String line = String(i + 1) + "," + 
//                      String(all_real_data[i], 4) + "," + 
//                      String(all_imag_data[i], 4) + "\n";
//         logFile.print(line);
//     }
        
//     // Write summary at the end
//     logFile.println();
//     logFile.println("# Dataset Summary");
//     logFile.printf("# Mode: %d\n", mode);
//     logFile.printf("# Total Samples: %d\n", total_samples);
//     logFile.printf("# Final Voltage: %.3f V\n", voltage);
//     logFile.printf("# Final Temperature: %.1f C\n", temperature);
    
//     if (total_samples > 0) {
//         // Calculate basic statistics
//         double real_sum = 0, imag_sum = 0;
//         double real_min = all_real_data[0], real_max = all_real_data[0];
//         double imag_min = all_imag_data[0], imag_max = all_imag_data[0];
        
//         for (int i = 0; i < total_samples; i++) {
//             real_sum += all_real_data[i];
//             imag_sum += all_imag_data[i];
            
//             if (all_real_data[i] < real_min) real_min = all_real_data[i];
//             if (all_real_data[i] > real_max) real_max = all_real_data[i];
//             if (all_imag_data[i] < imag_min) imag_min = all_imag_data[i];
//             if (all_imag_data[i] > imag_max) imag_max = all_imag_data[i];
//         }
        
//         double real_avg = real_sum / total_samples;
//         double imag_avg = imag_sum / total_samples;
        
//         logFile.printf("# Real Impedance - Avg: %.4f, Min: %.4f, Max: %.4f mOhm\n", 
//                       real_avg, real_min, real_max);
//         logFile.printf("# Imaginary Impedance - Avg: %.4f, Min: %.4f, Max: %.4f mOhm\n", 
//                       imag_avg, imag_min, imag_max);
//     }
    
//     logFile.flush();
//     logFile.close();
//     success = true;
    
//     Serial.printf("EISLogger: Complete 1kHz dataset logged (%d samples) to %s\n", 
//                  total_samples, filepath.c_str());
    
//     // Release SD card access
//     releaseSDCardAccess();
//     return success;
// }

bool EISLogger::logComplete1kHzDataset(MeasurementMode mode, const double* all_real_data, 
                                     const double* all_imag_data, int total_samples,
                                     double voltage, double temperature, int avg_count, int cycle) {
    if (!sd_ready) return false;
    
    if (!acquireSDCardAccess()) {
        Serial.println("EISLogger: Failed to acquire SD card access for 1kHz dataset logging");
        return false;
    }
    
    bool success = false;
    
    // Generate filepath with mode directory
    String modeDir = getModeDirectoryPath(mode);
    String filename = generateFilename(mode, cycle);
    String filepath = modeDir + "/" + filename;
    
    File logFile = SD.open(filepath, FILE_WRITE);
    if (!logFile) {
        Serial.printf("EISLogger: Failed to open %s\n", filepath.c_str());
        releaseSDCardAccess();
        return false;
    }
    
    // Write SIMPLE header - just the essentials
    String header;
    if (mode == MODE_1KHZ_LIVE) {
        header = "# Mode 5: 1kHz Live Stream - " + String(total_samples) + " samples\n";
        header += "RealImpedance_mOhm,ImagImpedance_mOhm\n";
    } else if (mode == MODE_1KHZ_AVG) {
        header = "# Mode 6: 1kHz Individual Samples (X=" + String(avg_count) + ")\n";
        header += "RealImpedance_mOhm,ImagImpedance_mOhm\n";
    }
    
    logFile.print(header);
    
    // Write ONLY the data points - simple format
    for (int i = 0; i < total_samples; i++) {
        String line = String(all_real_data[i], 4) + "," + String(all_imag_data[i], 4) + "\n";
        logFile.print(line);
    }
    
    logFile.flush();
    logFile.close();
    success = true;
    
    Serial.printf("EISLogger: %d data points saved to %s\n", total_samples, filepath.c_str());
    
    releaseSDCardAccess();
    return success;
}

bool EISLogger::startNewLog(MeasurementMode mode, int cycle) {
    if (!sd_ready) return false;
    
    // Acquire exclusive access to SD card
    if (!acquireSDCardAccess()) {
        Serial.println("EISLogger: Failed to acquire SD card access for new log");
        return false;
    }
    
    // Close any existing file
    if (file_open) {
        current_log_file.flush();
        current_log_file.close();
        file_open = false;
    }
    
    // Generate filepath with mode directory
    String modeDir = getModeDirectoryPath(mode);
    current_filename = generateFilename(mode, cycle);
    String filepath = modeDir + "/" + current_filename;
    
    // Open new file
    current_log_file = SD.open(filepath, FILE_WRITE);
    if (!current_log_file) {
        Serial.printf("EISLogger: Failed to create %s\n", filepath.c_str());
        releaseSDCardAccess();
        return false;
    }
    
    file_open = true;
    
    // Write detailed header for streaming modes
    // String header;
    // if (mode == MODE_1KHZ_LIVE) {
    //     header = "SampleNumber,Timestamp,RealImpedance_mOhm,ImagImpedance_mOhm\n";
    //     current_log_file.print(header);
    // } else if (mode == MODE_1KHZ_AVG) {
    //     header = "SampleNumber,RealImpedance_mOhm,ImagImpedance_mOhm\n";
    //     current_log_file.print(header);
    //     // Add comment line
    //     current_log_file.print("# Individual samples for averaging\n");
    // } else {
    //     // Standard header
    //     header = "RealImpedance_mOhm,ImagImpedance_mOhm\n";
    //     current_log_file.print(header);
    // }
    
    String header;
    if (mode == MODE_1KHZ_LIVE) {
        header = "# Mode 5: 1kHz Live Stream\n";
        header += "RealImpedance_mOhm,ImagImpedance_mOhm\n";
    } else if (mode == MODE_1KHZ_AVG) {
        header = "# Mode 6: 1kHz Individual Samples\n";
        header += "RealImpedance_mOhm,ImagImpedance_mOhm\n";
    } else {
        header = "RealImpedance_mOhm,ImagImpedance_mOhm\n";
    }

    current_log_file.print(header);
    current_log_file.flush();
    Serial.printf("EISLogger: Started new log %s\n", filepath.c_str());
    
    releaseSDCardAccess();
    return true;
}

bool EISLogger::logDataPoint(int sample_num, int frequency, double real_impedance, 
                           double imag_impedance, double voltage, double temperature) {
    if (!sd_ready || !file_open) return false;
    
    // Acquire exclusive access to SD card
    if (!acquireSDCardAccess()) {
        Serial.println("EISLogger: Failed to acquire SD card access for data point logging");
        return false;
    }
    
    String line;
    
    // Different formats for different modes
    if (strstr(current_filename.c_str(), "1kHz_Live")) {
        // Mode 5: Include simple timestamp for each sample
        String timestamp = formatSimpleTimestamp();
        line = String(sample_num) + "," + timestamp + "," + 
               String(real_impedance, 4) + "," + String(imag_impedance, 4) + "\n";
    } else if (strstr(current_filename.c_str(), "1kHz")) {
        // Mode 6: Just sample number and values
        line = String(sample_num) + "," + String(real_impedance, 4) + "," + 
               String(imag_impedance, 4) + "\n";
    } else {
        // Standard format for other modes
        line = String(real_impedance, 4) + "," + String(imag_impedance, 4) + "\n";
    }
    
    current_log_file.print(line);
    current_log_file.flush();
    
    releaseSDCardAccess();
    return true;
}

void EISLogger::listLogFiles() {
    if (!sd_ready) return;
    
    if (!acquireSDCardAccess()) {
        Serial.println("EISLogger: Failed to acquire SD card access for listing files");
        return;
    }
    
    Serial.println("=== EIS Log Files by Mode ===");
    
    const char* mode_dirs[] = {
        "/data/mode1", "/data/mode2", "/data/mode3", 
        "/data/mode4", "/data/mode5", "/data/mode6"
    };
    
    const char* mode_names[] = {
        "Mode 1 (Single Scan)", "Mode 2 (Average Scan)", "Mode 3 (Continuous)",
        "Mode 4 (Average Loop)", "Mode 5 (1kHz Live)", "Mode 6 (1kHz Avg)"
    };
    
    for (int i = 0; i < 6; i++) {
        Serial.printf("\n--- %s ---\n", mode_names[i]);
        
        File modeDir = SD.open(mode_dirs[i]);
        if (!modeDir) {
            Serial.printf("Cannot open directory: %s\n", mode_dirs[i]);
            continue;
        }
        
        int file_count = 0;
        while (true) {
            File entry = modeDir.openNextFile();
            if (!entry) break;
            
            if (!entry.isDirectory() && String(entry.name()).endsWith(".csv")) {
                Serial.printf("  %s - %d bytes\n", entry.name(), entry.size());
                file_count++;
            }
            entry.close();
        }
        modeDir.close();
        
        if (file_count == 0) {
            Serial.println("  (No files)");
        }
    }
    
    releaseSDCardAccess();
}

bool EISLogger::initializeSemaphore() {
    if (!semaphore_initialized) {
        sd_card_semaphore = xSemaphoreCreateMutex();
        if (sd_card_semaphore == nullptr) {
            Serial.println("EISLogger: Failed to create SD card semaphore");
            return false;
        }
        semaphore_initialized = true;
        Serial.println("EISLogger: SD card semaphore created successfully");
    }
    return true;
}

bool EISLogger::acquireSDCardAccess() {
    if (sd_card_semaphore == nullptr) {
        Serial.println("EISLogger: Semaphore not initialized");
        return false;
    }
    
    if (xSemaphoreTake(sd_card_semaphore, pdMS_TO_TICKS(1000)) == pdTRUE) {
        return true;
    } else {
        Serial.println("EISLogger: Failed to acquire SD card semaphore (timeout)");
        return false;
    }
}

void EISLogger::releaseSDCardAccess() {
    if (sd_card_semaphore != nullptr) {
        xSemaphoreGive(sd_card_semaphore);
    }
}

String EISLogger::sanitizeFilename(String filename) {
    // Remove or replace invalid characters for FAT32 filesystem
    filename.replace("/", "_");
    filename.replace("\\", "_");
    filename.replace(":", "-");
    filename.replace("*", "_");
    filename.replace("?", "_");
    filename.replace("\"", "_");
    filename.replace("<", "_");
    filename.replace(">", "_");
    filename.replace("|", "_");
    
    return filename;
}

String EISLogger::getModeString(MeasurementMode mode) {
    switch (mode) {
        case MODE_SINGLE_SCAN:   return "SingleScan";
        case MODE_AVERAGE_SCAN:  return "AvgScan";
        case MODE_CONTINUOUS:    return "Continuous";
        case MODE_AVERAGE_LOOP:  return "AvgLoop";
        case MODE_1KHZ_LIVE:     return "1kHz_Live";
        case MODE_1KHZ_AVG:      return "1kHz_Avg";
        default:                 return "Unknown";
    }
}

bool EISLogger::ensureDataDirectory() {
    return createModeDirectories();
}

bool EISLogger::closeCurrentLog() {
    if (!file_open) return false;
    
    if (!acquireSDCardAccess()) {
        Serial.println("EISLogger: Failed to acquire SD card access for closing log");
        return false;
    }
    
    if (current_log_file) {
        current_log_file.flush();
        current_log_file.close();
        file_open = false;
        Serial.printf("EISLogger: Closed log %s\n", current_filename.c_str());
    }
    
    releaseSDCardAccess();
    return true;
}