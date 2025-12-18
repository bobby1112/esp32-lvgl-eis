#include "UserMode.h"

// Global thread control variable
volatile bool stop_requested = false;
bool first_1khz_read = true;

// Static frequency arrays
const int UserMode::FREQ[MAX_FREQUENCIES] = {
    1007, 4943, 3906, 3082, 2471, 1953, 1541, 1220, 977,
    770, 610, 488, 385, 305, 242, 192, 152, 122, 97
};

const int UserMode::FREQ_CMD_MAN[MAX_FREQUENCIES] = {
    66, 162, 16, 202, 162, 8, 202, 10, 15, 202, 10, 2, 202, 10, 254, 202, 10, 2, 102
};

const int UserMode::FREQ_CMD_EXP[MAX_FREQUENCIES] = {
    11, 12, 15, 11, 11, 15, 10, 14, 15, 9, 13, 15, 8, 12, 7, 7, 11, 13, 7
};

UserMode::UserMode(EISPlot* plot, NXPPoint* nxp) 
    : button_panel(nullptr), eis_plot(plot), nxp_point(nxp), logging_queue(nullptr),
      nxp_serial(nullptr), cpu_serial(nullptr),
      current_mode(MODE_NONE), averaging_count(3),
      num_devices(0), measurement_active(false), logging_enabled(false),
      wait_start_time(0), wait_seconds_remaining(0),
      waiting_60s(false), current_cycle(0),
      current_freq_index(0), current_sweep(0),
      total_sweeps(0), current_frequency(0), progress_percentage(0.0),
      measurement_state(STATE_IDLE), last_measurement_time(0),
      freq_measurement_start(0), sample_count_1khz(0)
      /*ui_response_task_handle(NULL), ui_task_running(false)*/ {
    
    // Initialize arrays
    clearAccumulators();
    memset(impedance_real, 0, sizeof(impedance_real));
    memset(impedance_imag, 0, sizeof(impedance_imag));
    memset(voltages, 0, sizeof(voltages));
    memset(temperatures, 0, sizeof(temperatures));
    memset(voltage, 0, sizeof(voltage));
    memset(UID, 0, sizeof(UID));
}

UserMode::~UserMode() {
    if (measurement_active) {
        stopMeasurement();
    }
}

void UserMode::setSerialPorts(HardwareSerial* nxpSerial, Stream* cpuSerial) {
    nxp_serial = nxpSerial;
    cpu_serial = cpuSerial;
}

void UserMode::startMeasurement() {
    if (measurement_active) {
        cpu_serial->println("Measurement already in progress!");
        return;
    }
    
    measurement_active = true;
    stop_requested = false;
    current_cycle = 0;
    waiting_60s = false;
    measurement_state = STATE_INITIALIZING;
    
    cpu_serial->printf("UserMode: Starting measurement - Mode %d, Avg Count: %d\n", 
                      current_mode, averaging_count);
    
    if (eis_plot) {
        eis_plot->setCurrentMode(current_mode);

        eis_plot->clearData();
        cpu_serial->println("UserMode: Cleared old measurement data");
    }

    // Show dialog immediately for ALL modes
    if (button_panel) {
        button_panel->showMeasurementDialog(current_mode, averaging_count);
        button_panel->setMeasurementStatus("Initializing...");
        lv_timer_handler();
        delay(100);
    }

    // Reset voltage & temperature display CORRECTLY
    if (eis_plot && eis_plot->voltage_label && eis_plot->temp_label) {
        lv_label_set_text(eis_plot->voltage_label, "V: -.--- V");
        lv_label_set_text(eis_plot->temp_label, "T: --.- °C");
        lv_timer_handler();  // Process the label updates
    }

    // Initialize hardware
    if (!initializeDevices()) {
        cpu_serial->println("Failed to initialize devices!");
        stopMeasurement();
        if (button_panel) {
            button_panel->setMeasurementStatus("Device Error");
            button_panel->closeMeasurementDialog();
        }
        return;
    }
    
    // Set up initial state based on mode
    switch (current_mode) {
        case MODE_SINGLE_SCAN:
            setupMode1();
            break;
        case MODE_AVERAGE_SCAN:
            setupMode2();
            break;
        case MODE_CONTINUOUS:
            setupMode3();
            break;
        case MODE_AVERAGE_LOOP:
            setupMode4();
            break;
        case MODE_1KHZ_LIVE:
            setupMode5();
            break;
        case MODE_1KHZ_AVG:
            setupMode6();
            break;
        default:
            cpu_serial->println("Invalid mode!");
            stopMeasurement();
            break;
    }
}

void UserMode::stopMeasurement() {
    cpu_serial->println("UserMode: Stop measurement requested");
    stop_requested = true;
    measurement_active = false;
    waiting_60s = false;
    measurement_state = STATE_IDLE;
    
    lv_timer_handler();

    // Queue close file command to logging task on Core 0
    if (logging_enabled && logging_queue) {
        logging_queue->enqueueCloseFile();
        cpu_serial->println("Core 1: Queued file close for Core 0");
    }
    
    if (button_panel) {
        button_panel->setMeasurementStatus("Stopped");
        button_panel->setMeasurementRunning(false);

        delay(300); // Brief delay for user to see "Stopped" status
        button_panel->closeMeasurementDialog();
        cpu_serial->println("Dialog auto-closed after STOP");
    }
    if (eis_plot) {
        eis_plot->setStatusText("Stopped");
    }

    lv_timer_handler();
    cpu_serial->println("UserMode: Measurement stopped and dialog closed");
}

void UserMode::updateTimers() {
    if (!measurement_active) return;
    
    // Check for stop request first
    if (checkStopRequested()) {
        stopMeasurement();
        return;
    }
    
    // Update state machine
    switch (measurement_state) {
        case STATE_INITIALIZING:
            handleInitializing();
            break;
        case STATE_MEASURING_FREQUENCY:
            handleMeasuringFrequency();
            break;
        case STATE_FREQUENCY_COMPLETE:
            handleFrequencyComplete();
            break;
        case STATE_SWEEP_COMPLETE:
            handleSweepComplete();
            break;
        case STATE_WAITING_60S:
            handleWaiting60s();
            break;
        case STATE_MEASUREMENT_COMPLETE:
            handleMeasurementComplete();
            break;
        case STATE_STREAMING_1KHZ:
            handleStreaming1kHz();
            break;
        default:
            break;
    }
    
    // Allow LVGL to process events
    lv_timer_handler();
}

// STATE HANDLERS
void UserMode::handleInitializing() {
    if (eis_plot) eis_plot->clearData();
    
    switch (current_mode) {
        case MODE_SINGLE_SCAN:
        case MODE_CONTINUOUS:
            startNextFrequency();
            break;
        case MODE_AVERAGE_SCAN:
        case MODE_AVERAGE_LOOP:
            clearAccumulators();
            current_sweep = 1;
            startNextFrequency();
            break;
        case MODE_1KHZ_LIVE:
            measurement_state = STATE_STREAMING_1KHZ;
            if (eis_plot) eis_plot->switchToTimePlot();
            break;
        case MODE_1KHZ_AVG:
            clearAccumulators();
            current_sweep = 0;
            sample_count_1khz = 0;
            if (eis_plot) eis_plot->switchToTimePlot();
            measurement_state = STATE_STREAMING_1KHZ;
            break;
    }
}

void UserMode::handleMeasuringFrequency() {
    unsigned long now = millis();
    
    // Non-blocking frequency measurement with timeout
    if (now - freq_measurement_start < 1000) {
        updateProgressDisplay();
        return;
    }
    
    // Complete the frequency measurement
    if (measureSingleFrequencyNonBlocking(current_freq_index, 1)) {
        measurement_state = STATE_FREQUENCY_COMPLETE;
        
        // Update real-time display for single point modes
        if (current_mode == MODE_SINGLE_SCAN || current_mode == MODE_CONTINUOUS) {
            if (eis_plot) {
                eis_plot->updateSinglePoint(current_freq_index, 
                    impedance_real[current_freq_index], impedance_imag[current_freq_index]);
            }
        }
    } else {
        cpu_serial->printf("Failed to measure frequency %d\n", current_freq_index);
        stopMeasurement();
    }
}

void UserMode::handleFrequencyComplete() {
    // Move to next frequency or complete sweep
    current_freq_index++;
    current_frequency = current_freq_index + 1;
    
    if (current_freq_index < MAX_FREQUENCIES) {
        startNextFrequency();
    } else {
        // Sweep complete
        measurement_state = STATE_SWEEP_COMPLETE;
    }
}

void UserMode::handleSweepComplete() {
    // Process completed sweep based on mode
    switch (current_mode) {
        case MODE_SINGLE_SCAN:
            // Show final result on Core 1
            if (eis_plot) {
                eis_plot->updateData(impedance_real, impedance_imag, MAX_FREQUENCIES);
            }
            // Queue dataset for logging on Core 0 (SEQUENTIAL - after measurement complete)
            if (logging_enabled && logging_queue) {
                bool queued = logging_queue->enqueueDataset(current_mode, FREQ, impedance_real, 
                                              impedance_imag, MAX_FREQUENCIES);
                cpu_serial->printf("Core 1: Dataset queued for Core 0 logging: %s\n", 
                                 queued ? "SUCCESS" : "QUEUE FULL");
            }
            measurement_state = STATE_MEASUREMENT_COMPLETE;
            break;
            
        case MODE_AVERAGE_SCAN:
            // Accumulate data
            for (int i = 0; i < MAX_FREQUENCIES; i++) {
                accumulated_real[i] += impedance_real[i];
                accumulated_imag[i] += impedance_imag[i];
            }
            
            if (current_sweep >= averaging_count) {
                // Calculate averages and finish
                calculateAverages();
                if (eis_plot) {
                    eis_plot->updateData(impedance_real, impedance_imag, MAX_FREQUENCIES);
                }
                // Queue averaged dataset for Core 0 logging (SEQUENTIAL)
                if (logging_enabled && logging_queue) {
                    bool queued = logging_queue->enqueueDataset(current_mode, FREQ, impedance_real, 
                                                  impedance_imag, MAX_FREQUENCIES, 0.0, 0.0, averaging_count);
                    cpu_serial->printf("Core 1: Averaged dataset queued: %s\n", 
                                     queued ? "SUCCESS" : "QUEUE FULL");
                }
                measurement_state = STATE_MEASUREMENT_COMPLETE;
            } else {
                // Continue to next sweep
                current_sweep++;
                current_freq_index = 0;
                current_frequency = 1;
                startNextFrequency();
            }
            break;
            
        case MODE_CONTINUOUS:
            // Show result on Core 1
            if (eis_plot) {
                eis_plot->updateData(impedance_real, impedance_imag, MAX_FREQUENCIES);
            }
            // Queue cycle data for Core 0 logging (SEQUENTIAL)
            if (logging_enabled && logging_queue) {
                bool queued = logging_queue->enqueueDataset(current_mode, FREQ, impedance_real, 
                                              impedance_imag, MAX_FREQUENCIES, 
                                              0.0, 0.0, 0, current_cycle);
                cpu_serial->printf("Core 1: Cycle %d queued for logging: %s\n", 
                                 current_cycle, queued ? "SUCCESS" : "QUEUE FULL");
            }
            startWaiting60s();
            break;
            
        case MODE_AVERAGE_LOOP:
            // Accumulate data
            for (int i = 0; i < MAX_FREQUENCIES; i++) {
                accumulated_real[i] += impedance_real[i];
                accumulated_imag[i] += impedance_imag[i];
            }
            
            if (current_sweep >= averaging_count) {
                // Calculate averages and show result
                calculateAverages();
                if (eis_plot) {
                    eis_plot->updateData(impedance_real, impedance_imag, MAX_FREQUENCIES);
                }
                // Queue averaged cycle data for Core 0 logging (SEQUENTIAL)
                if (logging_enabled && logging_queue) {
                    bool queued = logging_queue->enqueueDataset(current_mode, FREQ, impedance_real, 
                                                  impedance_imag, MAX_FREQUENCIES, 
                                                  0.0, 0.0, averaging_count, current_cycle);
                    cpu_serial->printf("Core 1: Avg cycle %d queued: %s\n", 
                                     current_cycle, queued ? "SUCCESS" : "QUEUE FULL");
                }
                startWaiting60s();
            } else {
                // Continue next sweep in cycle
                current_sweep++;
                current_freq_index = 0;
                current_frequency = 1;
                startNextFrequency();
            }
            break;
    }
}

void UserMode::handleWaiting60s() {
    unsigned long elapsed = (millis() - wait_start_time) / 1000;
    int seconds_left = 60 - elapsed;
    
    if (seconds_left != wait_seconds_remaining) {
        wait_seconds_remaining = seconds_left;
        
        if (button_panel) {
            button_panel->updateMeasurementProgress(current_cycle, 
                (current_mode == MODE_AVERAGE_LOOP) ? averaging_count : 1,
                (current_mode == MODE_AVERAGE_LOOP) ? averaging_count : 1,
                MAX_FREQUENCIES, seconds_left);
        }
        
        if (seconds_left % 10 == 0) {
            cpu_serial->printf("Waiting: %d seconds remaining\n", seconds_left);
        }
    }
    
    if (seconds_left <= 0) {
        // Start next cycle
        current_cycle++;
        if (current_mode == MODE_AVERAGE_LOOP) {
            clearAccumulators();
            current_sweep = 1;
        }
        current_freq_index = 0;
        current_frequency = 1;
        startNextFrequency();
    }
}

void UserMode::handleMeasurementComplete() {
    if (button_panel) {
        button_panel->setMeasurementStatus("Complete!");
    }
    if (eis_plot) {
        eis_plot->setStatusText("Done");
    }

    // AUTO-CLOSE dialog after 3 seconds (longer for user to see completion)
    static unsigned long completion_time = 0;
    if (completion_time == 0) {
        completion_time = millis();
        
        // LOGGING: For modes 5 and 6, queue the complete dataset for logging
        if (logging_enabled && logging_queue) {
            if (current_mode == MODE_1KHZ_LIVE && sample_count_1khz > 0) {
                // Get all 1kHz data from EISPlot for final complete logging
                if (eis_plot && eis_plot->total_1khz_samples > 0) {
                    double voltage = (num_devices > 1) ? voltages[1] : 0.0;
                    double temp = (num_devices > 1) ? temperatures[1] : 0.0;
                    
                    bool queued = logging_queue->enqueue1kHzDatasetChunked(
                        current_mode, 
                        eis_plot->all_1khz_real, 
                        eis_plot->all_1khz_imag, 
                        eis_plot->total_1khz_samples,
                        voltage, temp, 0, 0);
                        
                    cpu_serial->printf("Core 1: FINAL Mode 5 dataset queued (%d samples): %s\n", 
                                     eis_plot->total_1khz_samples, queued ? "SUCCESS" : "QUEUE FULL");
                }
            } else if (current_mode == MODE_1KHZ_AVG && current_sweep >= averaging_count) {
                // For Mode 6, queue the final averaged result plus all samples used
                if (eis_plot && eis_plot->total_1khz_samples > 0) {
                    double voltage = (num_devices > 1) ? voltages[1] : 0.0;
                    double temp = (num_devices > 1) ? temperatures[1] : 0.0;
                    
                    bool queued = logging_queue->enqueue1kHzDatasetChunked(
                        current_mode, 
                        eis_plot->all_1khz_real, 
                        eis_plot->all_1khz_imag, 
                        eis_plot->total_1khz_samples,
                        voltage, temp, averaging_count, 0);
                        
                    cpu_serial->printf("Core 1: FINAL Mode 6 dataset queued (%d samples, avg=%d): %s\n", 
                                     eis_plot->total_1khz_samples, averaging_count, queued ? "SUCCESS" : "QUEUE FULL");
                }
            }
            
            // Queue file close
            logging_queue->enqueueCloseFile();
            cpu_serial->println("Core 1: Queued final file close");
        }
    }
    
    if (millis() - completion_time > 3000) {  // 3 second display
        measurement_active = false;
        measurement_state = STATE_IDLE;
        if (button_panel) {
            button_panel->closeMeasurementDialog();
        }
        completion_time = 0;
        
        cpu_serial->printf("Measurement complete - Mode %d finished\n", current_mode);
    }
}

// void UserMode::handleMeasurementComplete() {
//     if (button_panel) {
//         button_panel->setMeasurementStatus("Complete!");
//     }
//     if (eis_plot) {
//         eis_plot->setStatusText("Done");
//     }
//     // if (current_mode == MODE_1KHZ_LIVE) {
//     //     eis_plot->showTimeDataTable(sample_count_1khz);
//     // }
//     // if (current_mode == MODE_1KHZ_AVG) {
//     //     eis_plot->showImpedanceDataTable(impedance_real, impedance_imag, 1);
//     // }

//     // Auto-close dialog after 2 seconds
//     static unsigned long completion_time = 0;
//     if (completion_time == 0) {
//         completion_time = millis();
//     }
    
//     if (millis() - completion_time > 2000) {
//         measurement_active = false;
//         measurement_state = STATE_IDLE;
//         if (button_panel) {
//             button_panel->closeMeasurementDialog();
//         }
//         completion_time = 0;
//     }
    
//     // For modes 5 and 6, queue the complete dataset for logging
//     if (logging_enabled && logging_queue) {
//         if (current_mode == MODE_1KHZ_LIVE && sample_count_1khz > 0) {
//             // Get all 1kHz data from EISPlot for logging
//             if (eis_plot && eis_plot->total_1khz_samples > 0) {
//                 double voltage = (num_devices > 1) ? voltages[1] : 0.0;
//                 double temp = (num_devices > 1) ? temperatures[1] : 0.0;
                
//                 bool queued = logging_queue->enqueue1kHzDatasetChunked(
//                     current_mode, 
//                     eis_plot->all_1khz_real, 
//                     eis_plot->all_1khz_imag, 
//                     eis_plot->total_1khz_samples,
//                     voltage, temp, 0, 0);
                    
//                 cpu_serial->printf("Core 1: Complete Mode 5 dataset queued (%d samples): %s\n", 
//                                  eis_plot->total_1khz_samples, queued ? "SUCCESS" : "QUEUE FULL");
//             }
//         } else if (current_mode == MODE_1KHZ_AVG && current_sweep >= averaging_count) {
//             // For Mode 6, queue the final averaged result plus all samples used for averaging
//             if (eis_plot && eis_plot->total_1khz_samples > 0) {
//                 double voltage = (num_devices > 1) ? voltages[1] : 0.0;
//                 double temp = (num_devices > 1) ? temperatures[1] : 0.0;
                
//                 bool queued = logging_queue->enqueue1kHzDatasetChunked(
//                     current_mode, 
//                     eis_plot->all_1khz_real, 
//                     eis_plot->all_1khz_imag, 
//                     eis_plot->total_1khz_samples,
//                     voltage, temp, averaging_count, 0);
                    
//                 cpu_serial->printf("Core 1: Complete Mode 6 dataset queued (%d samples, avg=%d): %s\n", 
//                                  eis_plot->total_1khz_samples, averaging_count, queued ? "SUCCESS" : "QUEUE FULL");
//             }
//         }
//     }
    
//     // Show appropriate data tables
//     if (current_mode == MODE_1KHZ_LIVE) {
//         if (eis_plot) eis_plot->showTimeDataTable(sample_count_1khz);
//     } else if (current_mode == MODE_1KHZ_AVG) {
//         if (eis_plot) eis_plot->showImpedanceDataTable(impedance_real, impedance_imag, 1);
//     }

// }

// Updated UserMode.cpp - Simplified Mode 6 logging

void UserMode::handleStreaming1kHz() {
    if (checkStopRequested()) {
        cpu_serial->println("Mode 5/6: STOP detected, exiting streaming immediately");
        return; // This will trigger stopMeasurement() in main loop
    }

    unsigned long now = millis();
    
    if (current_mode == MODE_1KHZ_LIVE) {
        // Mode 5: Continuous streaming - SAVE ALL DATA POINTS
        if (now - last_measurement_time >= 50) {
            // CHECK STOP REQUEST FIRST
            if (checkStopRequested()) {
                cpu_serial->println("Mode 5: Stop detected during sampling");
                saveAllMode5DataToSD();
                measurement_state = STATE_MEASUREMENT_COMPLETE;
                return;
            }
            
            // Perform measurement
            if (measure1kHzSingleRead(1)) {
                sample_count_1khz++;
                current_frequency = sample_count_1khz;
                
                // Update display on Core 1
                if (eis_plot) {
                    eis_plot->addTimePoint(impedance_real[0], impedance_imag[0]);
                }
                
                // SAVE EVERY SINGLE DATA POINT
                if (logging_enabled && logging_queue) {
                    // Start new log file on first sample
                    if (sample_count_1khz == 1) {
                        bool queued = logging_queue->enqueueStartStreaming(current_mode, 0);
                        cpu_serial->printf("Mode 5: Started logging stream for ALL samples\n");
                    }
                    
                    // Queue EVERY data point
                    double voltage = (num_devices > 1) ? voltages[1] : 0.0;
                    double temp = (num_devices > 1) ? temperatures[1] : 0.0;
                    
                    bool queued = logging_queue->enqueueSinglePoint(sample_count_1khz, 1007, 
                                           impedance_real[0], impedance_imag[0], voltage, temp);
                    
                    if (!queued) {
                        cpu_serial->printf("Mode 5: Queue full at sample %d - force saving\n", sample_count_1khz);
                        saveAllMode5DataToSD();
                    }
                    
                    if (sample_count_1khz % 50 == 0) {
                        cpu_serial->printf("Mode 5: Logged %d samples\n", sample_count_1khz);
                    }
                }

                updateProgressDisplay();
                last_measurement_time = now;
                if (checkStopRequested()) {
                    cpu_serial->printf("Mode 5: Stop detected after sample %d - saving data\n", sample_count_1khz);
                    saveAllMode5DataToSD();
                    measurement_state = STATE_MEASUREMENT_COMPLETE;
                    return;
                }
                
                if (sample_count_1khz % 50 == 0) {
                    cpu_serial->printf("Mode 5: %d samples (STOP responsive)\n", sample_count_1khz);
                }
            }
        }
        
    } else if (current_mode == MODE_1KHZ_AVG) {
        // Mode 6: SAVE ONLY THE INDIVIDUAL SAMPLES (no final average entry)
        if (current_sweep < averaging_count && now - last_measurement_time >= 10) {
            // CHECK STOP REQUEST FIRST
            if (checkStopRequested()) {
                cpu_serial->println("Mode 6: Stop detected during averaging");
                saveAllMode6DataToSD();
                measurement_state = STATE_MEASUREMENT_COMPLETE;
                return;
            }
            
            // Perform measurement
            if (measure1kHzSingleRead(1)) {
                current_sweep++;
                accumulated_real[0] += impedance_real[0];
                accumulated_imag[0] += impedance_imag[0];
                
                // SAVE ONLY THE INDIVIDUAL SAMPLE (not the running average)
                if (logging_enabled && logging_queue) {
                    // Start new log file on first sample
                    if (current_sweep == 1) {
                        bool queued = logging_queue->enqueueStartStreaming(current_mode, 0);
                        cpu_serial->printf("Mode 6: Started logging for %d individual samples\n", averaging_count);
                    }
                    
                    // Log each individual sample (the raw measurement, not running average)
                    double voltage = (num_devices > 1) ? voltages[1] : 0.0;
                    double temp = (num_devices > 1) ? temperatures[1] : 0.0;
                    
                    bool queued = logging_queue->enqueueSinglePoint(current_sweep, 1007, 
                                           impedance_real[0], impedance_imag[0], voltage, temp);
                    
                    cpu_serial->printf("Mode 6: Logged sample %d/%d: Real=%.4f, Imag=%.4f\n", 
                                     current_sweep, averaging_count, impedance_real[0], impedance_imag[0]);
                }
                
                // Show running average on display only (not logged)
                double avg_real = accumulated_real[0] / current_sweep;
                double avg_imag = accumulated_imag[0] / current_sweep;
                
                if (eis_plot) {
                    eis_plot->addTimePoint(avg_real, avg_imag);
                }

                updateProgressDisplay();
                last_measurement_time = now;
                
                if (current_sweep >= averaging_count) {
                    // Calculate final averages for internal use only
                    impedance_real[0] = accumulated_real[0] / averaging_count;
                    impedance_imag[0] = accumulated_imag[0] / averaging_count;
                    
                    // NO LOGGING OF FINAL AVERAGE - just individual samples are saved
                    cpu_serial->printf("Mode 6: Complete - %d individual samples saved to CSV\n", averaging_count);
                    measurement_state = STATE_MEASUREMENT_COMPLETE;
                }
            }
        }
    }
}
//old handlestreaming
// void UserMode::handleStreaming1kHz() {
//     unsigned long now = millis();
    
//     if (current_mode == MODE_1KHZ_LIVE) {
//         // Continuous streaming - sample every 100ms
//         if (now - last_measurement_time >= 100) {
//             if (checkStopRequested()) {
//                 cpu_serial->println("Mode 5: Stop detected during sampling");

//             if (measureSingleFrequencyNonBlocking(0, 1)) {
//                 sample_count_1khz++;
//                 current_frequency = sample_count_1khz;
                
//                 // Update display on Core 1
//                 if (eis_plot) {
//                     eis_plot->addTimePoint(impedance_real[0], impedance_imag[0]);
//                 }
                
//                 // Queue data points for Core 0 logging (SEQUENTIAL)
//                 // if (logging_enabled && logging_queue) {
//                 //     // Start new log file on first sample
//                 //     if (sample_count_1khz == 1) {
//                 //         bool queued = logging_queue->enqueueStartStreaming(current_mode, 0);
//                 //         cpu_serial->printf("Core 1: Stream start queued: %s\n", 
//                 //                          queued ? "SUCCESS" : "QUEUE FULL");
//                 //     }
                    
//                 //     // Queue individual data points
//                 //     bool queued = logging_queue->enqueueSinglePoint(sample_count_1khz, 1007, 
//                 //                            impedance_real[0], impedance_imag[0]);
                    
//                 //     // Report queue status periodically
//                 //     if (sample_count_1khz % 50 == 0) {
//                 //         int queue_count = logging_queue->getQueueCount();
//                 //         cpu_serial->printf("Core 1: 1kHz sample %d, Queue: %d items%s\n", 
//                 //                          sample_count_1khz, queue_count, 
//                 //                          queue_count > 7 ? " (HIGH)" : "");
//                 //     }
//                 // }

//                 updateProgressDisplay();
//                 last_measurement_time = now;
//             }
//         }
//     } else if (current_mode == MODE_1KHZ_AVG) {
//         // Fixed number of samples
//         if (current_sweep < averaging_count && now - last_measurement_time >= 10) {
//             if (measureSingleFrequencyNonBlocking(0, 1)) {
//                 current_sweep++;
//                 accumulated_real[0] += impedance_real[0];
//                 accumulated_imag[0] += impedance_imag[0];
                
//                 // Show running average on Core 1
//                 double avg_real = accumulated_real[0] / current_sweep;
//                 double avg_imag = accumulated_imag[0] / current_sweep;
                
//                 if (eis_plot) {
//                     eis_plot->addTimePoint(avg_real, avg_imag);
//                 }

//                 updateProgressDisplay();
//                 last_measurement_time = now;
                
//                 if (current_sweep >= averaging_count) {
//                     // Queue final averaged result for Core 0 logging (SEQUENTIAL)
//                     // if (logging_enabled && logging_queue) {
//                     //     int freq_array[1] = {1007};
//                     //     double real_array[1] = {accumulated_real[0] / averaging_count};
//                     //     double imag_array[1] = {accumulated_imag[0] / averaging_count};
                        
//                     //     bool queued = logging_queue->enqueueDataset(current_mode, freq_array, real_array, 
//                     //                                   imag_array, 1, 0.0, 0.0, averaging_count);
//                     //     cpu_serial->printf("Core 1: 1kHz avg result queued: %s\n", 
//                     //                      queued ? "SUCCESS" : "QUEUE FULL");
//                     // }
//                     measurement_state = STATE_MEASUREMENT_COMPLETE;
//                 }
//             }
//         }
//     }
// }

void UserMode::saveAllMode5DataToSD() {
    if (!logging_enabled || !logging_queue || !eis_plot) {
        cpu_serial->println("Mode 5: Cannot save - logging disabled or components missing");
        return;
    }
    
    if (eis_plot->total_1khz_samples == 0) {
        cpu_serial->println("Mode 5: No data to save");
        return;
    }
    
    cpu_serial->printf("Mode 5: Saving ALL %d samples to SD card...\n", eis_plot->total_1khz_samples);
    
    double voltage = (num_devices > 1) ? voltages[1] : 0.0;
    double temp = (num_devices > 1) ? temperatures[1] : 0.0;
    
    // Use chunked logging to save ALL data points
    bool queued = logging_queue->enqueue1kHzDatasetChunked(
        current_mode, 
        eis_plot->all_1khz_real, 
        eis_plot->all_1khz_imag, 
        eis_plot->total_1khz_samples,
        voltage, temp, 0, 0);
        
    if (queued) {
        cpu_serial->printf("Mode 5: Complete dataset (%d points) queued successfully\n", eis_plot->total_1khz_samples);
    } else {
        cpu_serial->printf("Mode 5: FAILED to queue dataset (%d points) - queue full\n", eis_plot->total_1khz_samples);
    }
    
    // Queue file close to ensure data is written
    logging_queue->enqueueCloseFile();
}

void UserMode::saveAllMode6DataToSD() {
    if (!logging_enabled || !logging_queue || !eis_plot) {
        cpu_serial->println("Mode 6: Cannot save - logging disabled or components missing");
        return;
    }
    
    if (eis_plot->total_1khz_samples == 0) {
        cpu_serial->println("Mode 6: No data to save");
        return;
    }
    
    cpu_serial->printf("Mode 6: Saving ALL %d individual samples to SD...\n", eis_plot->total_1khz_samples);
    
    double voltage = (num_devices > 1) ? voltages[1] : 0.0;
    double temp = (num_devices > 1) ? temperatures[1] : 0.0;
    
    // Save all individual samples used for averaging (no final average entry)
    bool queued = logging_queue->enqueue1kHzDatasetChunked(
        current_mode, 
        eis_plot->all_1khz_real, 
        eis_plot->all_1khz_imag, 
        eis_plot->total_1khz_samples,
        voltage, temp, averaging_count, 0);
        
    if (queued) {
        cpu_serial->printf("Mode 6: Complete dataset (%d individual samples, X=%d) queued successfully\n", 
                         eis_plot->total_1khz_samples, averaging_count);
    } else {
        cpu_serial->printf("Mode 6: FAILED to queue dataset (%d samples) - queue full\n", eis_plot->total_1khz_samples);
    }
    
    // Queue file close to ensure data is written
    logging_queue->enqueueCloseFile();
}

// SETUP FUNCTIONS
void UserMode::setupMode1() {
    cpu_serial->println("Mode 1: Single Scan - Core 1 measurement, Core 0 logging");
    current_freq_index = 0;
    current_frequency = 1;
    current_cycle = 1;
    measurement_state = STATE_INITIALIZING;
}

void UserMode::setupMode2() {
    cpu_serial->printf("Mode 2: Average Scan (X=%d) - Sequential logging\n", averaging_count);
    total_sweeps = averaging_count;
    current_sweep = 0;
    current_freq_index = 0;
    current_frequency = 1;
    measurement_state = STATE_INITIALIZING;
}

void UserMode::setupMode3() {
    cpu_serial->println("Mode 3: Continuous - Core separation");
    current_cycle = 1;
    current_freq_index = 0;
    current_frequency = 1;
    if (eis_plot) eis_plot->showNyquistPlot();
    measurement_state = STATE_INITIALIZING;
}

void UserMode::setupMode4() {
    cpu_serial->printf("Mode 4: Average Loop (X=%d) - Sequential\n", averaging_count);
    total_sweeps = averaging_count;
    current_cycle = 1;
    current_sweep = 0;
    current_freq_index = 0;
    current_frequency = 1;
    if (eis_plot) eis_plot->showNyquistPlot();
    measurement_state = STATE_INITIALIZING;
}

void UserMode::setupMode5() {
    cpu_serial->println("Mode 5: 1kHz Live Stream - Queue-based logging");
    sample_count_1khz = 0;
    last_measurement_time = 0;
    measurement_state = STATE_INITIALIZING;
}

void UserMode::setupMode6() {
    cpu_serial->printf("Mode 6: 1kHz Average (X=%d) - Sequential\n", averaging_count);
    total_sweeps = averaging_count;
    current_sweep = 0;
    sample_count_1khz = 0;
    last_measurement_time = 0;
    measurement_state = STATE_INITIALIZING;
}

// HELPER FUNCTIONS
void UserMode::startNextFrequency() {
    freq_measurement_start = millis();
    measurement_state = STATE_MEASURING_FREQUENCY;
    updateProgressDisplay();
}

void UserMode::startWaiting60s() {
    waiting_60s = true;
    wait_start_time = millis();
    wait_seconds_remaining = 60;
    measurement_state = STATE_WAITING_60S;
    
    if (button_panel) {
        button_panel->setMeasurementStatus("Waiting 60s for next cycle...");
    }
}

bool UserMode::measureSingleFrequencyNonBlocking(int freq_index, int target_device) {
    if (freq_index < 0 || freq_index >= MAX_FREQUENCIES) return false;
    
    char cmd_buf[32];
    
    // For first frequency (1007 Hz), do double read
    if (freq_index == 0) {
        // First configuration and read
        snprintf(cmd_buf, sizeof(cmd_buf), "rtff7%02x%02x 2\n", 
                 FREQ_CMD_EXP[freq_index], FREQ_CMD_MAN[freq_index]);
        nxp_point->configureNXPread(nxp_serial, "rtff60132 2\n", cmd_buf, "rtff60cff 2\n");
        delay(200);
        
        nxp_point->readNXPvalue_voltageZM(nxp_serial, cpu_serial, num_devices, voltage);
        
        double** impedances_real = new double*[1];
        double** impedances_img = new double*[1];
        impedances_real[0] = new double[MAX_DEV];
        impedances_img[0] = new double[MAX_DEV];
        
        nxp_point->readNXPvalue_real(nxp_serial, cpu_serial, num_devices, voltage, impedances_real[0], 0);
        nxp_point->readNXPvalue_imaginary(nxp_serial, cpu_serial, num_devices, voltage, impedances_img[0]);
        
        delay(500);
        
        // Second configuration and read (use this one)
        nxp_point->configureNXPread(nxp_serial, "rtff60132 2\n", cmd_buf, "rtff60cff 2\n");
        delay(500);
        
        nxp_point->readNXPvalue_voltageZM(nxp_serial, cpu_serial, num_devices, voltage);
        nxp_point->readNXPvalue_real(nxp_serial, cpu_serial, num_devices, voltage, impedances_real[0], 0);
        nxp_point->readNXPvalue_imaginary(nxp_serial, cpu_serial, num_devices, voltage, impedances_img[0]);
        
        impedance_real[freq_index] = impedances_real[0][target_device];
        impedance_imag[freq_index] = impedances_img[0][target_device];
        
        delete[] impedances_real[0];
        delete[] impedances_img[0];
        delete[] impedances_real;
        delete[] impedances_img;
        
        delay(200);
        nxp_serial->print("rtff60132 2\n");
        delay(100);
        
    } else {
        // Normal single read for other frequencies
        snprintf(cmd_buf, sizeof(cmd_buf), "rtff7%02x%02x 2\n", 
                 FREQ_CMD_EXP[freq_index], FREQ_CMD_MAN[freq_index]);
        
        nxp_point->configureNXPread(nxp_serial, "rtff60132 2\n", cmd_buf, "rtff60cff 2\n");
        delay(500);
        
        nxp_point->readNXPvalue_voltageZM(nxp_serial, cpu_serial, num_devices, voltage);
        
        double** impedances_real = new double*[1];
        double** impedances_img = new double*[1];
        impedances_real[0] = new double[MAX_DEV];
        impedances_img[0] = new double[MAX_DEV];
        
        nxp_point->readNXPvalue_real(nxp_serial, cpu_serial, num_devices, voltage, impedances_real[0], 0);
        nxp_point->readNXPvalue_imaginary(nxp_serial, cpu_serial, num_devices, voltage, impedances_img[0]);
        
        impedance_real[freq_index] = impedances_real[0][target_device];
        impedance_imag[freq_index] = impedances_img[0][target_device];
        
        delete[] impedances_real[0];
        delete[] impedances_img[0];
        delete[] impedances_real;
        delete[] impedances_img;
        
        delay(200);
        nxp_serial->print("rtff60132 2\n");
        delay(5);
    }
    
    // IMPORTANT: Read ACTUAL battery voltage (not Vzm) for display
    // This uses the main ADC voltage register (rtffe0000)
    nxp_point->readNXPvalue_voltage(nxp_serial, cpu_serial, num_devices, voltages);
    nxp_point->readNXPvalue_temperature(nxp_serial, cpu_serial, num_devices, temperatures);
    
    // Update display with actual battery voltage
    if (eis_plot && target_device < num_devices) {
        eis_plot->updateVoltageTemp(voltages[target_device], temperatures[target_device]);
    }
    
    cpu_serial->printf("Freq %d Hz: Real=%.2f mΩ, Imag=%.2f mΩ, Batt V =%.4f V\n", 
                    FREQ[freq_index], impedance_real[freq_index], impedance_imag[freq_index],
                    voltages[target_device]);
    delay(10);
    return true;
}

// bool UserMode::measure1kHzSingleRead(int target_device) {
//     char cmd_buf[32];
    
//     // IMMEDIATE stop check before any operations
//     if (stop_requested) {
//         cpu_serial->println("INSTANT STOP: Before 1kHz measurement");
//         return false;
//     }
    
//     snprintf(cmd_buf, sizeof(cmd_buf), "rtff7%02x%02x 2\n", 
//         FREQ_CMD_EXP[0], FREQ_CMD_MAN[0]); // 0 = 1007 Hz
    
//     if (first_1khz_read) {
//         cpu_serial->println("1kHz Double reading (stabilization + measurement)...");

//         // --- FIRST READ (stabilization) ---
//         nxp_point->configureNXPread(nxp_serial, "rtff60132 2\n", cmd_buf, "rtff60cff 2\n");
//         delay(200);
//         nxp_point->readNXPvalue_voltageZM(nxp_serial, cpu_serial, num_devices, voltage);

//         double tmp_real[MAX_DEV], tmp_imag[MAX_DEV];
//         nxp_point->readNXPvalue_real(nxp_serial, cpu_serial, num_devices, voltage, tmp_real, 0);
//         nxp_point->readNXPvalue_imaginary(nxp_serial, cpu_serial, num_devices, voltage, tmp_imag);

//         if (stop_requested) {
//             cpu_serial->println("INSTANT STOP: After stabilization read");
//             return false;
//         }

//         delay(300); // critical gap before second read

//         // --- SECOND READ (actual measurement we use) ---
//         nxp_point->configureNXPread(nxp_serial, "rtff60132 2\n", cmd_buf, "rtff60cff 2\n");
//         delay(200);
//         nxp_point->readNXPvalue_voltageZM(nxp_serial, cpu_serial, num_devices, voltage);

//         double real_val[MAX_DEV], imag_val[MAX_DEV];
//         nxp_point->readNXPvalue_real(nxp_serial, cpu_serial, num_devices, voltage, real_val, 0);
//         nxp_point->readNXPvalue_imaginary(nxp_serial, cpu_serial, num_devices, voltage, imag_val);

//         impedance_real[0] = real_val[target_device];
//         impedance_imag[0] = imag_val[target_device];

//         first_1khz_read = false; // next time only do single read

//     } else {
//         // --- SINGLE READ (for subsequent measurements after stabilization) ---
//         cpu_serial->println("1kHz Single reading...");
        
//         nxp_point->configureNXPread(nxp_serial, "rtff60132 2\n", cmd_buf, "rtff60cff 2\n");
//         delay(200);
//         nxp_point->readNXPvalue_voltageZM(nxp_serial, cpu_serial, num_devices, voltage);

//         double real_val[MAX_DEV], imag_val[MAX_DEV];
//         nxp_point->readNXPvalue_real(nxp_serial, cpu_serial, num_devices, voltage, real_val, 0);
//         nxp_point->readNXPvalue_imaginary(nxp_serial, cpu_serial, num_devices, voltage, imag_val);

//         impedance_real[0] = real_val[target_device];
//         impedance_imag[0] = imag_val[target_device];
//     }

//     // Send cleanup command quickly
//     nxp_serial->print("rtff60132 2\n");
//     delay(25);

//     // Update voltage/temp less often for speed
//     static int voltage_read_counter_1khz = 0;
//     if (voltage_read_counter_1khz % 10 == 0) {
//         nxp_point->readNXPvalue_voltage(nxp_serial, cpu_serial, num_devices, voltages);
//         nxp_point->readNXPvalue_temperature(nxp_serial, cpu_serial, num_devices, temperatures);

//         if (eis_plot && target_device < num_devices) {
//             eis_plot->updateVoltageTemp(voltages[target_device], temperatures[target_device]);
//         }
//     }
//     voltage_read_counter_1khz++;

//     cpu_serial->printf("1kHz Result: Real=%.2f mΩ, Imag=%.2f mΩ\n",
//                        impedance_real[0], impedance_imag[0]);

//     return true;
// }

bool UserMode::measure1kHzSingleRead(int target_device) {
    char cmd_buf[32];
    
    // IMMEDIATE stop check before any operations
    if (stop_requested) {
        cpu_serial->println("INSTANT STOP: Before 1kHz measurement");
        return false;
    }
    
    //For 1007 Hz, do proper double read like the original function
    cpu_serial->println("1kHz Double reading (stabilization + measurement)...");
    
    snprintf(cmd_buf, sizeof(cmd_buf), "rtff7%02x%02x 2\n", 
             FREQ_CMD_EXP[0], FREQ_CMD_MAN[0]); // 0 = 1007 Hz
    
    // FIRST READ - Stabilization
    nxp_point->configureNXPread(nxp_serial, "rtff60132 2\n", cmd_buf, "rtff60cff 2\n");
    
    // STOP CHECK after first configure
    if (stop_requested) {
        cpu_serial->println("INSTANT STOP: After first 1kHz configure");
        return false;
    }
    
    delay(200); // Reduced from 500ms but keep some delay for stability
    
    // First voltage read
    nxp_point->readNXPvalue_voltageZM(nxp_serial, cpu_serial, num_devices, voltage);
    
    // Allocate arrays for first read
    double** impedances_real_first = new double*[1];
    double** impedances_img_first = new double*[1];
    impedances_real_first[0] = new double[MAX_DEV];
    impedances_img_first[0] = new double[MAX_DEV];
    
    // First impedance reads (for stabilization)
    nxp_point->readNXPvalue_real(nxp_serial, cpu_serial, num_devices, voltage, impedances_real_first[0], 0);
    nxp_point->readNXPvalue_imaginary(nxp_serial, cpu_serial, num_devices, voltage, impedances_img_first[0]);
    
    // STOP CHECK after first read
    if (stop_requested) {
        delete[] impedances_real_first[0];
        delete[] impedances_img_first[0];
        delete[] impedances_real_first;
        delete[] impedances_img_first;
        cpu_serial->println("INSTANT STOP: After first 1kHz read");
        return false;
    }
    
    delay(300); // Reduced from 1000ms - critical delay between reads
    
    // SECOND READ - Actual measurement (this is the one we use)
    nxp_point->configureNXPread(nxp_serial, "rtff60132 2\n", cmd_buf, "rtff60cff 2\n");
    
    // STOP CHECK before second configure
    if (stop_requested) {
        delete[] impedances_real_first[0];
        delete[] impedances_img_first[0];
        delete[] impedances_real_first;
        delete[] impedances_img_first;
        cpu_serial->println("INSTANT STOP: Before second 1kHz configure");
        return false;
    }
    
    delay(200); // Reduced from 500ms
    
    // Second voltage read
    nxp_point->readNXPvalue_voltageZM(nxp_serial, cpu_serial, num_devices, voltage);
    
    // Allocate arrays for second read (the actual measurement)
    double** impedances_real = new double*[1];
    double** impedances_img = new double*[1];
    impedances_real[0] = new double[MAX_DEV];
    impedances_img[0] = new double[MAX_DEV];
    
    // Second impedance reads (actual measurement values)
    nxp_point->readNXPvalue_real(nxp_serial, cpu_serial, num_devices, voltage, impedances_real[0], 0);
    nxp_point->readNXPvalue_imaginary(nxp_serial, cpu_serial, num_devices, voltage, impedances_img[0]);
    
    // Store results from SECOND read (the accurate measurement)
    impedance_real[0] = impedances_real[0][target_device];
    impedance_imag[0] = impedances_img[0][target_device];
    
    // Cleanup both sets of arrays
    delete[] impedances_real_first[0];
    delete[] impedances_img_first[0];
    delete[] impedances_real_first;
    delete[] impedances_img_first;
    
    delete[] impedances_real[0];
    delete[] impedances_img[0];
    delete[] impedances_real;
    delete[] impedances_img;
    
    // Quick cleanup command
    delay(100); // Reduced from 500ms
    nxp_serial->print("rtff60132 2\n");
    delay(25);  // Reduced from 100ms
    
    // Read battery voltage and temperature less frequently for speed
    // static int voltage_read_counter_1khz = 0;
    // if (voltage_read_counter_1khz % 10 == 0) { // Every 10th sample
    //     nxp_point->readNXPvalue_voltage(nxp_serial, cpu_serial, num_devices, voltages);
    //     nxp_point->readNXPvalue_temperature(nxp_serial, cpu_serial, num_devices, temperatures);
        
    //     // Update display
    //     if (eis_plot && target_device < num_devices) {
    //         eis_plot->updateVoltageTemp(voltages[target_device], temperatures[target_device]);
    //     }
    // }
    // voltage_read_counter_1khz++;

    nxp_point->readNXPvalue_voltage(nxp_serial, cpu_serial, num_devices, voltages);
    nxp_point->readNXPvalue_temperature(nxp_serial, cpu_serial, num_devices, temperatures);
    
    // Update display
    if (eis_plot && target_device < num_devices) {
        eis_plot->updateVoltageTemp(voltages[target_device], temperatures[target_device]);
    }
    
    cpu_serial->printf("1kHz Single: Real=%.2f mΩ, Imag=%.2f mΩ\n", 
                      impedance_real[0], impedance_imag[0]);
    
    return true;
}

void UserMode::calculateAverages() {
    if (current_mode == MODE_1KHZ_AVG) {
        impedance_real[0] = accumulated_real[0] / averaging_count;
        impedance_imag[0] = accumulated_imag[0] / averaging_count;
    } else {
        for (int i = 0; i < MAX_FREQUENCIES; i++) {
            impedance_real[i] = accumulated_real[i] / averaging_count;
            impedance_imag[i] = accumulated_imag[i] / averaging_count;
        }
    }
}

bool UserMode::initializeDevices() {
    if (!nxp_serial || !cpu_serial || !nxp_point) {
        cpu_serial->println("Serial ports or NXP not initialized!");
        return false;
    }
    
    num_devices = nxp_point->Enumerate(nxp_serial, cpu_serial);
    cpu_serial->printf("Number of devices found: %d\n", num_devices);
    
    if (num_devices < 2) {
        cpu_serial->println("Not enough devices connected!");
        error_msg_box("No measurement devices detected.\nPlease check device connections and try again.");
        return false;
    }
    
    nxp_point->UniqueID(nxp_serial, cpu_serial, UID, num_devices);
    nxp_point->fixCRC(nxp_serial, cpu_serial, num_devices);
    nxp_point->getStatus(nxp_serial, cpu_serial, num_devices);
    
    nxp_point->readNXPvalue_voltage(nxp_serial, cpu_serial, num_devices, voltages);
    nxp_point->readNXPvalue_temperature(nxp_serial, cpu_serial, num_devices, temperatures);
    
    if (eis_plot && num_devices > 1) {
        eis_plot->updateVoltageTemp(voltages[1], temperatures[1]);
        cpu_serial->printf("Initial Battery Voltage: %.3f V, Temperature: %.1f C\n", 
                          voltages[1], temperatures[1]);
    }
    
    return true;
}

void UserMode::updateProgressDisplay() {
    if (!button_panel) return;
    
    switch (current_mode) {
        case MODE_SINGLE_SCAN:
        case MODE_CONTINUOUS:
            button_panel->updateMeasurementProgress(current_cycle, 1, 1, current_frequency);
            break;
        case MODE_AVERAGE_SCAN:
        case MODE_AVERAGE_LOOP:
            button_panel->updateMeasurementProgress(current_cycle, current_sweep, total_sweeps, current_frequency);
            break;
        case MODE_1KHZ_LIVE:
            button_panel->updateMeasurementProgress(0, current_frequency, 0, current_frequency);
            break;
        case MODE_1KHZ_AVG:
            button_panel->updateMeasurementProgress(0, current_sweep, total_sweeps, 0);
            break;
    }
}

// bool UserMode::checkStopRequested() {
//     if (stop_requested) {
//         cpu_serial->println("Stop detected on Core 1");
//         return true;
//     }
//     return false;
// }
// ENHANCED: Better stop detection with immediate response
bool UserMode::checkStopRequested() {
    if (stop_requested) {
        cpu_serial->println("PRIORITY STOP: Stop flag detected on Core 1");
        
        // For continuous modes, save data before stopping
        if ((current_mode == MODE_1KHZ_LIVE || current_mode == MODE_1KHZ_AVG) && 
            logging_enabled && logging_queue) {
            
            // Queue close file command
            logging_queue->enqueueCloseFile();
            cpu_serial->println("STOP: Queued file close command");
        }
        
        // For streaming modes, save data immediately
        // if (current_mode == MODE_1KHZ_LIVE && sample_count_1khz > 0) {
        //     cpu_serial->println("STOP: Saving Mode 5 data...");
        //     saveAllMode5DataToSD();
        // } else if (current_mode == MODE_1KHZ_AVG && current_sweep > 0) {
        //     cpu_serial->println("STOP: Saving Mode 6 data...");
        //     saveAllMode6DataToSD();
        // }
        
        return true;
    }
    return false;
}

void UserMode::clearAccumulators() {
    for (int i = 0; i < MAX_FREQUENCIES; i++) {
        accumulated_real[i] = 0.0;
        accumulated_imag[i] = 0.0;
    }
}

bool UserMode::checkForDuplicates() {
    int duplicate_count = 0;
    
    for (int i = 1; i < MAX_FREQUENCIES && i <= current_freq_index; i++) {
        if (fabs(impedance_real[i] - impedance_real[i-1]) < 0.1 && 
            fabs(impedance_imag[i] - impedance_imag[i-1]) < 0.1) {
            duplicate_count++;
            if (duplicate_count >= 2) {
                cpu_serial->printf("WARNING: Duplicate data detected at frequency %d\n", i);
                if (eis_plot) {
                    eis_plot->setStatusText("Duplicates!");
                }
                return true;
            }
        } else {
            duplicate_count = 0;
        }
    }
    
    return false;
}