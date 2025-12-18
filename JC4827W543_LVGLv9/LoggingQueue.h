#ifndef LOGGING_QUEUE_H
#define LOGGING_QUEUE_H

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "MeasurementMode.h"

#define MAX_FREQUENCIES 19
#define MAX_1KHZ_SAMPLES_QUEUE 200  // Increased for Mode 5/6
#define MAX_QUEUE_ITEMS 8     // Increased queue size

enum LogTaskType {
    LOG_COMPLETE_DATASET,
    LOG_SINGLE_POINT,
    LOG_START_STREAMING,
    LOG_CLOSE_FILE,
    LOG_COMPLETE_1KHZ_DATASET  
};

struct LogData {
    LogTaskType task_type;
    MeasurementMode mode;
    int num_points;
    int frequencies[MAX_FREQUENCIES];
    double real_impedances[MAX_FREQUENCIES];
    double imag_impedances[MAX_FREQUENCIES];
    
    int total_1khz_samples;
    int chunk_start;  // Starting index for this chunk
    int chunk_size;   // Number of samples in this chunk
    double all_real_1khz[MAX_1KHZ_SAMPLES_QUEUE];  // Smaller array
    double all_imag_1khz[MAX_1KHZ_SAMPLES_QUEUE];  // Smaller array
    
    double voltage;
    double temperature;
    int avg_count;
    int cycle;
    int sample_number;
    char filename[64];
    unsigned long timestamp;
};

class LoggingQueue {
public:
    static LoggingQueue& getInstance();
    
    bool init();
    
    // Standard dataset logging (Modes 1-4)
    bool enqueueDataset(MeasurementMode mode, const int* frequencies, 
                       const double* real_impedances, const double* imag_impedances,
                       int num_points, double voltage = 0.0, double temperature = 0.0,
                       int avg_count = 0, int cycle = 0);
    
    // Chunked 1kHz dataset logging (Modes 5-6) - handles large datasets in chunks
    bool enqueue1kHzDatasetChunked(MeasurementMode mode, const double* all_real_data, 
                                  const double* all_imag_data, int total_samples,
                                  double voltage = 0.0, double temperature = 0.0,
                                  int avg_count = 0, int cycle = 0);
    
    // Single point logging (for streaming)
    bool enqueueSinglePoint(int sample_num, int frequency, double real_impedance, 
                           double imag_impedance, double voltage = 0.0, double temperature = 0.0);
    
    bool enqueueStartStreaming(MeasurementMode mode, int cycle = 0);
    bool enqueueCloseFile();
    
    bool dequeue(LogData& data, TickType_t timeout = portMAX_DELAY);
    int getQueueCount();
    bool isQueueFull();
    
private:
    LoggingQueue() : log_queue(nullptr) {}
    ~LoggingQueue();
    
    QueueHandle_t log_queue;
    static const int QUEUE_SIZE = MAX_QUEUE_ITEMS;
}; 

#endif