#include <lvgl.h>            
#include <PINS_JC4827W543.h> 
#include "TAMC_GT911.h"      
#include <SD.h>
// #include <SPI.h>
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

// Make the C symbol from malaysia_flag.c visible in C++ code.
extern "C" {
  LV_IMAGE_DECLARE(malaysia_flag);   // from malaysia_flag.c
}

// Touch Controller
#define TOUCH_SDA 8
#define TOUCH_SCL 4 
#define TOUCH_INT 3
#define TOUCH_RST 38
#define TOUCH_WIDTH 480
#define TOUCH_HEIGHT 272
TAMC_GT911 touchController = TAMC_GT911(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, TOUCH_WIDTH, TOUCH_HEIGHT);

//SD definition
// #define SD_MISO     13
// #define SD_MOSI     11
// #define SD_SCLK     12
// #define SD_CS       10
static SPIClass spiSD{ HSPI };

// Core includes
#include <HardwareSerial.h>            
#include "NXPPoint.h"
#include "EISPlot.h"  
#include "ButtonPanel.h" 
#include "UserMode.h"
#include "EISLogger.h"
#include "LoggingQueue.h"

HardwareSerial NXPSerial(1);
#define CPUSerial Serial

// Main objects
NXPPoint NXPP; 
EISPlot eisPlot;
ButtonPanel buttonPanel;
UserMode userMode(&eisPlot, &NXPP);
EISLogger eisLogger;
LoggingQueue& loggingQueue = LoggingQueue::getInstance();

// Display global variables
uint32_t screenWidth;
uint32_t screenHeight;
uint32_t bufSize;
lv_display_t *disp;
lv_color_t *disp_draw_buf;

bool logging_enabled = true;

// Task handles
TaskHandle_t measurementTaskHandle = nullptr;
TaskHandle_t loggingTaskHandle = nullptr;

void measurementTask(void *parameter);
void loggingTask(void *parameter);

// Touch state tracking for debouncing
static bool last_touch_state = false;
static uint32_t last_touch_time = 0;
static lv_point_t last_touch_point = {0, 0};
static uint8_t touch_debounce_count = 0;

// LVGL callbacks
void my_print(lv_log_level_t level, const char *buf) {
    LV_UNUSED(level);
    Serial.println(buf);
    Serial.flush();
}

uint32_t millis_cb(void) {
    return millis();
}

void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
    lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
    touchController.read();
    
    uint32_t current_time = millis();
    bool current_touch_detected = (touchController.isTouched && touchController.touches > 0);
    
    // Debouncing logic
    if (current_touch_detected != last_touch_state) {
        if (touch_debounce_count < 3) {
            touch_debounce_count++;
            data->state = last_touch_state ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
            data->point = last_touch_point;
            return;
        } else {
            last_touch_state = current_touch_detected;
            touch_debounce_count = 0;
            last_touch_time = current_time;
        }
    } else {
        touch_debounce_count = 0;
    }
    
    if (current_touch_detected) {
        lv_point_t new_point;
        new_point.x = screenWidth - 1 - touchController.points[0].x;
        new_point.y = screenHeight - 1 - touchController.points[0].y;
        
        // Simple smoothing
        if (abs(new_point.x - last_touch_point.x) > 5 || abs(new_point.y - last_touch_point.y) > 5) {
            last_touch_point = new_point;
        } else if (current_time - last_touch_time > 50) {
            last_touch_point.x = (last_touch_point.x + new_point.x) / 2;
            last_touch_point.y = (last_touch_point.y + new_point.y) / 2;
            last_touch_time = current_time;
        }
        
        data->point = last_touch_point;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
        data->point = last_touch_point;
    }
}

// Measurement callbacks
void startMeasurementWithMode(int mode, int avg_count) {
    if (userMode.isMeasurementActive()) {
        CPUSerial.println("Measurement already in progress!");
        return;
    }
    
    CPUSerial.printf("Starting measurement: Mode %d, Avg Count: %d\n", mode, avg_count);
    
    // Configure UserMode
    userMode.setMode((MeasurementMode)mode);
    userMode.setAverageCount(avg_count);
    
    // Reset stop flag
    stop_requested = false;
    
    // Clear any previous data and set appropriate plot mode
    eisPlot.clearData();
    
    // Switch to time plot for modes 5 & 6
    if (mode == MODE_1KHZ_LIVE || mode == MODE_1KHZ_AVG) {
        eisPlot.switchToTimePlot();
    } 
    else {
        eisPlot.showNyquistPlot();
    }
    
    eisPlot.setStatusText("Starting...");
    
    // Start the measurement
    userMode.startMeasurement();
}

void stopMeasurement() {
    CPUSerial.println("Stop measurement requested");
    stop_requested = true;
    userMode.stopMeasurement();
    
    // Close dialog after a short delay for user feedback
    delay(500);
    buttonPanel.closeMeasurementDialog();
    eisPlot.setStatusText("Stopped");
}

// Measurement Task - Runs on Core 1 (All measurements, display, UI)
void measurementTask(void *parameter) {
    Serial.printf("Measurement task starting on core %d\n", xPortGetCoreID());
    
    while (true) {
        // Handle LVGL (display and touch)
        lv_task_handler();
        
        // Check for stop requests and handle immediately // new added
        if (stop_requested) {
            if (userMode.isMeasurementActive()) {
                Serial.println("Main Task: Stop detected, calling stopMeasurement");
                userMode.stopMeasurement();
                stop_requested = false; // Clear the flag after handling
            } else {
                stop_requested = false; // Clear flag if already stopped
            }
        }

        // Update measurements
        if (userMode.isMeasurementActive()) {
            userMode.updateTimers();
        }
        
        // Graphics update
        #ifdef DIRECT_MODE
        #if defined(CANVAS) || defined(RGB_PANEL) || defined(DSI_PANEL)
            gfx->flush();
        #else
            gfx->draw16bitRGBBitmap(0, 0, (uint16_t *)disp_draw_buf, screenWidth, screenHeight);
        #endif
        #else
        #ifdef CANVAS
            gfx->flush();
        #endif
        #endif
        
        // Small delay to prevent task starvation
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// Logging Task - Runs on Core 0 (Only SD card operations)
void loggingTask(void *parameter) {
    Serial.printf("Logging task starting on core %d\n", xPortGetCoreID());
    
    LogData logData;
    int processed_count = 0;
    
    while (true) {
        // Wait for logging tasks from the queue
        if (loggingQueue.dequeue(logData, pdMS_TO_TICKS(1000))) {
            processed_count++;
            
            switch (logData.task_type) {
                case LOG_COMPLETE_DATASET:
                    if (eisLogger.isReady()) {
                        bool success = eisLogger.logCompleteDataset(
                            logData.mode, logData.frequencies, 
                            logData.real_impedances, logData.imag_impedances,
                            logData.num_points, logData.voltage, logData.temperature,
                            logData.avg_count, logData.cycle
                        );
                        Serial.printf("Core 0: Dataset logged %s (#%d)\n", 
                                    success ? "successfully" : "FAILED", processed_count);
                    }
                    break;
                    
                case LOG_SINGLE_POINT:
                    if (eisLogger.isReady()) {
                        bool success = eisLogger.logDataPoint(
                            logData.sample_number, logData.frequencies[0],
                            logData.real_impedances[0], logData.imag_impedances[0],
                            logData.voltage, logData.temperature
                        );
                        if (!success && logData.sample_number % 100 == 0) {
                            Serial.printf("Core 0: Failed to log sample %d\n", logData.sample_number);
                        }
                    }
                    break;
                    
                case LOG_START_STREAMING:
                    if (eisLogger.isReady()) {
                        bool success = eisLogger.startNewLog(logData.mode, logData.cycle);
                        Serial.printf("Core 0: Stream log %s\n", success ? "started" : "FAILED");
                    }
                    break;
                    
                case LOG_CLOSE_FILE:
                    if (eisLogger.isReady()) {
                        eisLogger.closeCurrentLog();
                        Serial.println("Core 0: Log file closed");
                    }
                    break;
            }
            
            // Report queue status periodically
            if (processed_count % 50 == 0) {
                Serial.printf("Core 0: Processed %d items, queue: %d\n", 
                            processed_count, loggingQueue.getQueueCount());
            }
        }
        
        // Small delay when no work to do
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
void setup() {
    // Initialize serial communications
    NXPSerial.begin(1000000, SERIAL_8N1, 18, 17);
    CPUSerial.begin(115200);
    CPUSerial.println("EIS Monitor with UserMode Integration");
    
    String LVGL_Arduino = String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
    Serial.println(LVGL_Arduino);

    // Initialize UserMode with serial ports
    userMode.setSerialPorts(&NXPSerial, &CPUSerial);
    delay(2000);

   // Initialize SD Card (will be used by logging task on Core 0)
    spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (SD.begin(SD_CS, spiSD, 10000000)) {
        Serial.println("SD Card initialized for Core 0 logging");
        // Create data directory
        if (!SD.exists("/data")) {
            SD.mkdir("/data");
        }
        logging_enabled = true;
    } else {
        Serial.println("SD Card failed - logging disabled");
        logging_enabled = false;
    }
    
    // Initialize logging queue
    if (!loggingQueue.init()) {
        Serial.println("Failed to initialize logging queue");
        logging_enabled = false;
    }
    
    // Initialize EIS Logger (will run on Core 0)
    if (logging_enabled && eisLogger.init()) {
        Serial.println("EIS Logger ready for Core 0");
    } else {
        Serial.println("EIS Logger failed");
        logging_enabled = false;
    }

    // Initialize Display
    if (!gfx->begin()) {
        Serial.println("gfx->begin() failed!");
        while (true) { /* no need to continue */ }
    }

    // Set the backlight
    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);
    gfx->fillScreen(RGB565_BLACK);
    gfx->setRotation(2);

    // Initialize touch device
    touchController.begin();
    touchController.setRotation(ROTATION_INVERTED);

    // Initialize LVGL
    lv_init();
    lv_tick_set_cb(millis_cb);

#if LV_USE_LOG != 0
    lv_log_register_print_cb(my_print);
#endif

    screenWidth = gfx->width();
    screenHeight = gfx->height();
    bufSize = screenWidth * 40;

    disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!disp_draw_buf) {
        disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_8BIT);
    }
    
    if (!disp_draw_buf) {
        Serial.println("LVGL disp_draw_buf allocate failed!");
        while (true) { /* no need to continue */ }
    } else {
        disp = lv_display_create(screenWidth, screenHeight);
        lv_display_set_flush_cb(disp, my_disp_flush);
        lv_display_set_buffers(disp, disp_draw_buf, NULL, bufSize * 2, LV_DISPLAY_RENDER_MODE_PARTIAL);
        lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_0);

        // Create input device
        lv_indev_t *indev = lv_indev_create();
        lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(indev, my_touchpad_read);

        // Improve touch responsiveness
        lv_indev_set_long_press_time(indev, 400);
        lv_indev_set_long_press_repeat_time(indev, 100);
        lv_indev_set_scroll_limit(indev, 10);

        // Set dark theme
        lv_theme_t * th = lv_theme_default_init(disp, 
                                              lv_palette_main(LV_PALETTE_BLUE), 
                                              lv_palette_main(LV_PALETTE_RED),
                                              true, LV_FONT_DEFAULT);
        lv_disp_set_theme(disp, th);

        // Initialize EIS Plot
        eisPlot.init();
        
        // Initialize Button Panel
        buttonPanel.init(lv_screen_active());

        // Set up callbacks with lambda functions
        buttonPanel.setStartMeasurementCallback([&userMode](int mode, int avg_count) {
            userMode.setMode((MeasurementMode)mode);
            userMode.setAverageCount(avg_count);
            userMode.startMeasurement();
        });

        buttonPanel.setStopMeasurementCallback([&userMode]() {
            userMode.stopMeasurement();
        });

        buttonPanel.setModeChangeCallback([&userMode](int mode) {
            userMode.setMode((MeasurementMode)mode);
        });
        
        // Link ButtonPanel to UserMode
        userMode.setButtonPanel(&buttonPanel);
        userMode.setLoggingQueue(&loggingQueue);
        userMode.enableLogging(logging_enabled);

        // Create title label
        lv_obj_t *title_label = lv_label_create(lv_screen_active());
        lv_label_set_text(title_label, "EIS Battery Monitor v" GFX_STR(LVGL_VERSION_MAJOR) "." GFX_STR(LVGL_VERSION_MINOR));
        lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
        lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 5);

        // Display malaysia flag
        uint16_t scale = (uint16_t)((uint32_t)256 * 62 / 500);
        lv_obj_t* flag = lv_image_create(lv_screen_active());
        lv_image_set_src(flag, &malaysia_flag);
        lv_obj_align(flag, LV_ALIGN_TOP_MID, -210, -130);
        lv_image_set_scale(flag, scale);   

        // Create instruction label
        lv_obj_t *instruction_label = lv_label_create(lv_screen_active());
        lv_label_set_text(instruction_label, "Select measurement mode and click Start");
        lv_obj_set_style_text_font(instruction_label, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(instruction_label, lv_color_hex(0xFFFF00), 0);
        lv_obj_align(instruction_label, LV_ALIGN_BOTTOM_LEFT, 10, -5);
        
        // Create progress info label
        // lv_obj_t *progress_label = lv_label_create(lv_screen_active());
        // lv_label_set_text(progress_label, "Ready");
        // lv_obj_set_style_text_font(progress_label, &lv_font_montserrat_12, 0);
        // lv_obj_set_style_text_color(progress_label, lv_color_hex(0x00FF00), 0);
        // lv_obj_align(progress_label, LV_ALIGN_BOTTOM_RIGHT, -10, -5);
    }
    // Create FreeRTOS tasks
    Serial.println("Creating separated tasks...");
    
    // Core 1: All measurements, display, UI (high priority)
    xTaskCreatePinnedToCore(
        measurementTask,
        "MeasurementTask",
        8192,
        NULL,
        3,  // High priority
        &measurementTaskHandle,
        1   // Core 1
    );
    
    // Core 0: Only logging (lower priority)
    xTaskCreatePinnedToCore(
        loggingTask,
        "LoggingTask", 
        8192,
        NULL,
        1,  // Lower priority
        &loggingTaskHandle,
        0   // Core 0
    );
    
    if (measurementTaskHandle == nullptr || loggingTaskHandle == nullptr) {
        Serial.println("Failed to create tasks!");
        while (true) { delay(1000); }
    }
    
    Serial.println("Sequential logging architecture ready!");
    Serial.println("Core 1: Measurements + Display | Core 0: SD Logging");

    CPUSerial.println("Setup completed. Select mode and press Start to begin.");
}

void loop() {
    // Monitor system health
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 15000) {
        Serial.printf("System: Free heap %u KB, Queue: %d items\n", 
                     ESP.getFreeHeap() / 1024, loggingQueue.getQueueCount());
        lastCheck = millis();
    }
    delay(1000);
}
