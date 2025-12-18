#ifndef ButtonPanel_h
#define ButtonPanel_h

#include <lvgl.h>
#include <Arduino.h>
#include "MeasurementMode.h"

void error_msg_box(const char* message);

class ButtonPanel {
public:
    ButtonPanel();
    ~ButtonPanel();
    
    // Initialize the button panel with mode selection
    void init(lv_obj_t* parent);
    
    // Set measurement start callback
    void setStartMeasurementCallback(void (*callback)(int mode, int avg_count));
    void setStopMeasurementCallback(void (*callback)(void));
    void setModeChangeCallback(void (*callback)(int mode)) {
        mode_change_callback = callback;
    }
    
    // Get current selected mode
    int getCurrentMode() { return current_mode; }
    
    // Get current averaging count
    int getAveragingCount() { return averaging_count; }

    bool getMeasurementRunning() { return is_measurement_running; }
    
    // Update mode display
    void updateModeDisplay();
    
    // Show unified message box for all modes
    void showMeasurementDialog(int mode, int avg_count);
    void updateMeasurementProgress(int cycle, int sweep, int total_sweeps, int frequency, int seconds_remaining = -1);
    void closeMeasurementDialog();

    // Set measurement status in dialog
    void setMeasurementStatus(const char* status);
    
    // Enable/disable start/stop buttons
    void setMeasurementRunning(bool running);
    
    // Get the main container
    lv_obj_t* getContainer() { return container; }

private:
    // UI objects
    lv_obj_t* container;
    lv_obj_t* mode_container;
    lv_obj_t* mode_info_panel;
    lv_obj_t* mode_label;
    lv_obj_t* desc_label;
    
    // Mode selection radio buttons
    lv_obj_t* mode_radios[6];  // For 6 modes
    lv_obj_t* mode_labels[6];
    
    // Averaging slider (only shown for certain modes)
    lv_obj_t* avg_slider_container;
    lv_obj_t* avg_slider;
    lv_obj_t* avg_label;
    
    // Start and Stop buttons (side by side)
    lv_obj_t* start_btn;
    lv_obj_t* stop_btn;
    
    // Unified message dialog for all measurement modes
    lv_obj_t* msgbox;
    lv_obj_t* progress_text_area;
    lv_obj_t* status_text_area;
    lv_obj_t* stop_btn_msgbox;
    lv_timer_t* progress_timer;
    lv_timer_t* countdown_timer;  // For 60-second countdown
    
    // Progress tracking state
    bool is_measurement_running;
    bool stop_in_progress; 
    volatile int current_cycle;
    volatile int current_sweep;
    volatile int total_sweeps;
    volatile int current_frequency;
    volatile int countdown_seconds;
    volatile bool update_progress_flag;
    volatile bool update_countdown_flag;
    
    // Current state
    int current_mode;
    int averaging_count;
    
    // Callbacks
    static void (*start_measurement_callback)(int mode, int avg_count);
    static void (*stop_measurement_callback)(void);
    void (*mode_change_callback)(int mode);
    
    // Internal methods
    void createModeInfoPanel();
    void createModeSelection();
    void createAveragingSlider();
    void createControlButtons();  // Creates both start and stop buttons
    void updateAveragingSliderVisibility();
    void updateProgressText();
    
    // Mode descriptions
    const char* getModeDescription(int mode);
    const char* getDialogTitle(int mode);
    const char* getDialogMessage(int mode, int avg_count);
    
    // Timer callbacks
    static void progressTimerCallback(lv_timer_t* timer);
    static void countdownTimerCallback(lv_timer_t* timer);
    
    // Static event callbacks
    static void modeRadioEventCallback(lv_event_t* e);
    static void avgSliderEventCallback(lv_event_t* e);
    static void startButtonEventCallback(lv_event_t* e);
    static void stopButtonEventCallback(lv_event_t* e);
    static void msgboxEventCallback(lv_event_t* e);
    
    // Instance pointer for static callbacks
    static ButtonPanel* instance;
};

#endif
