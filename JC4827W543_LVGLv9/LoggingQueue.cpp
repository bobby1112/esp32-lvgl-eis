#include "LoggingQueue.h"

/**
 * Returns the singleton instance of LoggingQueue
 * Uses static local variable to ensure thread-safe initialization
 */
LoggingQueue& LoggingQueue::getInstance() {
    static LoggingQueue instance;
    return instance;
}

/**
 * Destructor - cleans up the FreeRTOS queue if it was created
 */
LoggingQueue::~LoggingQueue() {
    if (log_queue != nullptr) {
        vQueueDelete(log_queue);
    }
}

/**
 * Initialize the logging queue with memory safety checks
 * @return true if queue created successfully, false if insufficient memory
 */
bool LoggingQueue::init() {
    // Check if already initialized
    if (log_queue != nullptr) {
        Serial.println("LoggingQueue: Already initialized");
        return true;
    }
    
    // Check available heap before creating queue
    size_t free_heap = esp_get_free_heap_size();
    size_t queue_size_bytes = QUEUE_SIZE * sizeof(LogData);
    
    Serial.printf("LoggingQueue: Free heap: %d bytes\n", free_heap);
    Serial.printf("LoggingQueue: Queue needs: %d bytes (%d items x %d bytes)\n", 
                  queue_size_bytes, QUEUE_SIZE, sizeof(LogData));
    
    if (free_heap < queue_size_bytes + 10000) {  // Need some safety margin
        Serial.printf("LoggingQueue: Insufficient memory! Need %d, have %d\n", 
                      queue_size_bytes + 10000, free_heap);
        return false;
    }
    
    // Try to create the queue
    log_queue = xQueueCreate(QUEUE_SIZE, sizeof(LogData));
    if (log_queue == nullptr) {
        Serial.printf("LoggingQueue: xQueueCreate failed! QUEUE_SIZE=%d, sizeof(LogData)=%d\n", 
                      QUEUE_SIZE, sizeof(LogData));
        
        // Try with smaller queue size
        Serial.println("LoggingQueue: Trying smaller queue size...");
        log_queue = xQueueCreate(5, sizeof(LogData));  // Reduced size
        if (log_queue == nullptr) {
            Serial.println("LoggingQueue: Even smaller queue failed!");
            return false;
        }
        Serial.println("LoggingQueue: Created with reduced size (5 items)");
    }
    
    Serial.printf("LoggingQueue: Successfully created queue at %p\n", log_queue);
    Serial.printf("LoggingQueue: Queue handle size: %d items\n", QUEUE_SIZE);
    return true;
}

bool LoggingQueue::enqueueDataset(MeasurementMode mode, const int* frequencies, 
                                 const double* real_impedances, const double* imag_impedances,
                                 int num_points, double voltage, double temperature,
                                 int avg_count, int cycle) {
    if (log_queue == nullptr) return false;
    
    LogData data;
    data.task_type = LOG_COMPLETE_DATASET;
    data.mode = mode;
    data.num_points = num_points;
    data.voltage = voltage;
    data.temperature = temperature;
    data.avg_count = avg_count;
    data.cycle = cycle;
    data.timestamp = millis();
    
    // Copy frequency and impedance arrays
    for (int i = 0; i < num_points && i < MAX_FREQUENCIES; i++) {
        data.frequencies[i] = frequencies[i];
        data.real_impedances[i] = real_impedances[i];
        data.imag_impedances[i] = imag_impedances[i];
    }
    
    // Try to enqueue (non-blocking)
    if (xQueueSend(log_queue, &data, 0) == pdTRUE) {
        return true;
    } else {
        Serial.println("LoggingQueue: Queue full, dropping dataset");
        return false;
    }
}

bool LoggingQueue::enqueue1kHzDatasetChunked(MeasurementMode mode, const double* all_real_data, 
                                            const double* all_imag_data, int total_samples,
                                            double voltage, double temperature,
                                            int avg_count, int cycle) {
    if (log_queue == nullptr) {
        Serial.println("LoggingQueue: Queue not initialized for chunked logging");
        return false;
    }
    
    if (total_samples == 0) {
        Serial.println("LoggingQueue: No data to chunk");
        return true;
    }
    
    const int chunk_size = MAX_1KHZ_SAMPLES_QUEUE;
    int chunks_sent = 0;
    
    Serial.printf("LoggingQueue: Chunking %d samples into %d-sample chunks\n", 
                  total_samples, chunk_size);
    
    // Split large dataset into smaller chunks
    for (int start_idx = 0; start_idx < total_samples; start_idx += chunk_size) {
        LogData data;
        data.task_type = LOG_COMPLETE_1KHZ_DATASET;
        data.mode = mode;
        data.total_1khz_samples = total_samples;  // Total across all chunks
        data.chunk_start = start_idx;
        data.chunk_size = min(chunk_size, total_samples - start_idx);
        data.voltage = voltage;
        data.temperature = temperature;
        data.avg_count = avg_count;
        data.cycle = cycle;
        data.timestamp = millis();
        
        // Copy chunk data
        for (int i = 0; i < data.chunk_size; i++) {
            data.all_real_1khz[i] = all_real_data[start_idx + i];
            data.all_imag_1khz[i] = all_imag_data[start_idx + i];
        }
        
        // Try to enqueue chunk
        if (xQueueSend(log_queue, &data, pdMS_TO_TICKS(100)) == pdTRUE) {
            chunks_sent++;
            Serial.printf("LoggingQueue: Sent chunk %d/%d (samples %d-%d)\n", 
                         chunks_sent, (total_samples + chunk_size - 1) / chunk_size,
                         start_idx, start_idx + data.chunk_size - 1);
        } else {
            Serial.printf("LoggingQueue: Failed to send chunk %d (queue full)\n", chunks_sent + 1);
            return false;
        }
    }
    
    Serial.printf("LoggingQueue: Successfully chunked dataset into %d chunks\n", chunks_sent);
    return true;
}

bool LoggingQueue::enqueueSinglePoint(int sample_num, int frequency, double real_impedance, 
                                     double imag_impedance, double voltage, double temperature) {
    if (log_queue == nullptr) return false;
    
    LogData data;
    data.task_type = LOG_SINGLE_POINT;
    data.sample_number = sample_num;
    data.frequencies[0] = frequency;
    data.real_impedances[0] = real_impedance;
    data.imag_impedances[0] = imag_impedance;
    data.voltage = voltage;
    data.temperature = temperature;
    data.timestamp = millis();
    
    // Try to enqueue (non-blocking)
    if (xQueueSend(log_queue, &data, 0) == pdTRUE) {
        return true;
    } else {
        Serial.printf("LoggingQueue: Queue full, dropping sample %d\n", sample_num);
        return false;
    }
}

bool LoggingQueue::enqueueStartStreaming(MeasurementMode mode, int cycle) {
    if (log_queue == nullptr) return false;
    
    LogData data;
    data.task_type = LOG_START_STREAMING;
    data.mode = mode;
    data.cycle = cycle;
    data.timestamp = millis();
    
    return (xQueueSend(log_queue, &data, 0) == pdTRUE);
}

bool LoggingQueue::enqueueCloseFile() {
    if (log_queue == nullptr) return false;
    
    LogData data;
    data.task_type = LOG_CLOSE_FILE;
    data.timestamp = millis();
    
    return (xQueueSend(log_queue, &data, 0) == pdTRUE);
}

bool LoggingQueue::dequeue(LogData& data, TickType_t timeout) {
    if (log_queue == nullptr) return false;
    
    return (xQueueReceive(log_queue, &data, timeout) == pdTRUE);
}

int LoggingQueue::getQueueCount() {
    if (log_queue == nullptr) return 0;
    return uxQueueMessagesWaiting(log_queue);
}

bool LoggingQueue::isQueueFull() {
    if (log_queue == nullptr) return true;
    return (uxQueueSpacesAvailable(log_queue) == 0);
}