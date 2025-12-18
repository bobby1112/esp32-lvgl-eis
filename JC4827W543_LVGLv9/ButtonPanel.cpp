#include "ButtonPanel.h"
#include <math.h>

// Static instance pointer
ButtonPanel* ButtonPanel::instance = nullptr;

static bool stop_button_pressed = false;
static unsigned long stop_button_time = 0;

// Static callback pointers
void (*ButtonPanel::start_measurement_callback)(int mode, int avg_count) = nullptr;
void (*ButtonPanel::stop_measurement_callback)(void) = nullptr;

ButtonPanel::ButtonPanel() : container(nullptr), mode_info_panel(nullptr), mode_label(nullptr),
                             desc_label(nullptr), avg_slider_container(nullptr),
                             avg_slider(nullptr), avg_label(nullptr), start_btn(nullptr), 
                             stop_btn(nullptr), msgbox(nullptr), progress_text_area(nullptr),
                             status_text_area(nullptr), stop_btn_msgbox(nullptr),
                             progress_timer(nullptr), countdown_timer(nullptr),
                             is_measurement_running(false), stop_in_progress(false),
                             current_mode(MODE_NONE), averaging_count(3),
                             current_cycle(0), current_sweep(0), total_sweeps(0), 
                             current_frequency(0), countdown_seconds(0),
                             update_progress_flag(false), update_countdown_flag(false) {
    instance = this;

    // Initialize radio button pointers
    for (int i = 0; i < 6; i++) {
        mode_radios[i] = nullptr;
        mode_labels[i] = nullptr;
    }
}

ButtonPanel::~ButtonPanel() {
    if (progress_timer) {
        lv_timer_del(progress_timer);
    }
    if (countdown_timer) {
        lv_timer_del(countdown_timer);
    }
    instance = nullptr;
}

void ButtonPanel::init(lv_obj_t* parent) {
    if (!parent) parent = lv_screen_active();
    
    // Create main container for the button panel
    container = lv_obj_create(parent);
    lv_obj_set_size(container, 130, 260);
    lv_obj_align(container, LV_ALIGN_RIGHT_MID, -5, 0);
    
    // Style the container
    lv_obj_set_style_bg_opa(container, LV_OPA_20, 0);
    lv_obj_set_style_border_width(container, 2, 0);
    lv_obj_set_style_border_color(container, lv_color_hex(0x606060), 0);
    lv_obj_set_style_radius(container, 8, 0);
    lv_obj_set_style_pad_all(container, 8, 0);
    
    createModeInfoPanel();
    createModeSelection();
    createAveragingSlider();
    createControlButtons();
    
    updateModeDisplay();
}

void ButtonPanel::createModeInfoPanel() {
    // Create mode info panel at the top
    mode_info_panel = lv_obj_create(container);
    lv_obj_set_size(mode_info_panel, 110, 60);
    lv_obj_align(mode_info_panel, LV_ALIGN_TOP_MID, 0, 5);
    lv_obj_set_style_bg_opa(mode_info_panel, LV_OPA_30, 0);
    lv_obj_set_style_border_width(mode_info_panel, 1, 0);
    lv_obj_set_style_radius(mode_info_panel, 4, 0);
    lv_obj_set_style_pad_all(mode_info_panel, 5, 0);
    
    // Mode label
    mode_label = lv_label_create(mode_info_panel);
    lv_label_set_text(mode_label, "Mode: None");
    lv_obj_set_style_text_font(mode_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(mode_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(mode_label, LV_ALIGN_TOP_LEFT, 0, 0);
    
    // Description label
    desc_label = lv_label_create(mode_info_panel);
    lv_label_set_text(desc_label, "Select mode");
    lv_obj_set_style_text_font(desc_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(desc_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_align_to(desc_label, mode_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 3);
}

void ButtonPanel::createModeSelection() {
    // Create scrollable container for mode selection
    lv_obj_t* mode_container = lv_obj_create(container);
    lv_obj_set_size(mode_container, 110, 90);
    lv_obj_align_to(mode_container, mode_info_panel, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    lv_obj_set_style_bg_opa(mode_container, LV_OPA_10, 0);
    lv_obj_set_style_border_width(mode_container, 1, 0);
    lv_obj_set_style_radius(mode_container, 4, 0);
    lv_obj_set_style_pad_all(mode_container, 5, 0);
    lv_obj_set_style_pad_gap(mode_container, 3, 0);
    
    // Enable scrolling
    lv_obj_set_scroll_dir(mode_container, LV_DIR_VER);
    lv_obj_set_flex_flow(mode_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(mode_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    // Mode definitions
    const char* mode_texts[] = {
        "Mode 1",
        "Mode 2", 
        "Mode 3",
        "Mode 4",
        "Mode 5",
        "Mode 6"
    };
    
    // Create radio buttons for each mode
    for (int i = 0; i < 6; i++) {
        // Create container for radio button and label
        lv_obj_t* radio_container = lv_obj_create(mode_container);
        lv_obj_set_size(radio_container, 95, 18);
        lv_obj_set_style_bg_opa(radio_container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(radio_container, 0, 0);
        lv_obj_set_style_pad_all(radio_container, 0, 0);
        lv_obj_set_flex_flow(radio_container, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(radio_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
        
        // Create radio button
        mode_radios[i] = lv_checkbox_create(radio_container);
        lv_obj_set_size(mode_radios[i], 15, 15);
        lv_obj_add_flag(mode_radios[i], LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_add_event_cb(mode_radios[i], modeRadioEventCallback, LV_EVENT_CLICKED, this);
        lv_obj_set_user_data(mode_radios[i], (void*)(intptr_t)(i + 1));
        
        // Create label
        mode_labels[i] = lv_label_create(radio_container);
        lv_label_set_text(mode_labels[i], mode_texts[i]);
        lv_obj_set_style_text_font(mode_labels[i], &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(mode_labels[i], lv_color_hex(0xFFFFFF), 0);
    }
}

void ButtonPanel::createAveragingSlider() {
    // Create averaging slider container (initially hidden)
    avg_slider_container = lv_obj_create(container);
    lv_obj_set_size(avg_slider_container, 110, 35);
    lv_obj_align_to(avg_slider_container, container, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_set_style_bg_opa(avg_slider_container, LV_OPA_20, 0);
    lv_obj_set_style_border_width(avg_slider_container, 1, 0);
    lv_obj_set_style_radius(avg_slider_container, 4, 0);
    lv_obj_set_style_pad_all(avg_slider_container, 5, 0);
    
    // Averaging label
    avg_label = lv_label_create(avg_slider_container);
    lv_label_set_text(avg_label, "Avg X: 3");
    lv_obj_set_style_text_font(avg_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(avg_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(avg_label, LV_ALIGN_TOP_LEFT, 0, 0);
    
    // Averaging slider
    avg_slider = lv_slider_create(avg_slider_container);
    lv_obj_set_size(avg_slider, 90, 15);
    lv_obj_align_to(avg_slider, avg_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 3);
    lv_slider_set_range(avg_slider, 2, 10);
    lv_slider_set_value(avg_slider, 3, LV_ANIM_OFF);
    lv_obj_add_event_cb(avg_slider, avgSliderEventCallback, LV_EVENT_VALUE_CHANGED, this);

    // Style the knob
    lv_obj_set_style_width(avg_slider, 3, LV_PART_KNOB);
    lv_obj_set_style_height(avg_slider, 3, LV_PART_KNOB);
    lv_obj_set_style_bg_color(avg_slider, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    lv_obj_set_style_radius(avg_slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    
    // Initially hide the averaging slider
    lv_obj_add_flag(avg_slider_container, LV_OBJ_FLAG_HIDDEN);
}

void ButtonPanel::createControlButtons() {

    // Create start button
    start_btn = lv_btn_create(container);
    lv_obj_set_size(start_btn, 55, 30);
    lv_obj_align(start_btn, LV_ALIGN_BOTTOM_LEFT, 0, -5);
    lv_obj_set_style_bg_color(start_btn, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_set_style_radius(start_btn, 6, 0);
    
    lv_obj_t* start_label = lv_label_create(start_btn);
    lv_label_set_text(start_label, "START");
    lv_obj_set_style_text_font(start_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(start_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(start_label);
    
    lv_obj_add_event_cb(start_btn, startButtonEventCallback, LV_EVENT_CLICKED, this);
    
    // Create stop button (beside start button)
    stop_btn = lv_btn_create(container);
    lv_obj_set_size(stop_btn, 55, 30);
    lv_obj_align_to(stop_btn, start_btn, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    lv_obj_set_style_bg_color(stop_btn, lv_color_hex(0xFF4444), 0);
    lv_obj_set_style_radius(stop_btn, 6, 0);
    
    lv_obj_set_style_bg_color(stop_btn, lv_color_hex(0xCC0000), LV_STATE_PRESSED);
    lv_obj_set_style_transform_scale(stop_btn, 240, LV_STATE_PRESSED);
    
    lv_obj_t* stop_label = lv_label_create(stop_btn);
    lv_label_set_text(stop_label, LV_SYMBOL_STOP " STOP");
    lv_obj_set_style_text_font(stop_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(stop_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(stop_label);
    
    lv_obj_add_event_cb(stop_btn, stopButtonEventCallback, LV_EVENT_CLICKED, this);
    
    // Initially disable stop button
    lv_obj_add_state(stop_btn, LV_STATE_DISABLED);
    lv_obj_set_style_bg_opa(stop_btn, LV_OPA_30, 0);
}

void error_msg_box(const char* message) {
    lv_obj_t* error_msgbox = lv_msgbox_create(lv_screen_active());
    lv_msgbox_add_title(error_msgbox, "Error");
    lv_msgbox_add_text(error_msgbox, message);
    lv_msgbox_add_close_button(error_msgbox);
    lv_obj_set_style_bg_color(error_msgbox, lv_color_hex(0x2f2f2f), 0);
    lv_obj_set_style_text_color(error_msgbox, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(error_msgbox);
}

void ButtonPanel::updateAveragingSliderVisibility() {
    // Show averaging slider for modes that need it (Mode 2, 4, 6)
    bool show_slider = (current_mode == MODE_AVERAGE_SCAN || 
                       current_mode == MODE_AVERAGE_LOOP || 
                       current_mode == MODE_1KHZ_AVG);
    
    if (show_slider) {
        lv_obj_clear_flag(avg_slider_container, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(avg_slider_container, LV_OBJ_FLAG_HIDDEN);
    }
}

const char* ButtonPanel::getModeDescription(int mode) {
    switch (mode) {
        case MODE_SINGLE_SCAN:   return "Single scan";
        case MODE_AVERAGE_SCAN:  return "Average X scans";
        case MODE_CONTINUOUS:    return "Single scan\nLoop every 60s";
        case MODE_AVERAGE_LOOP:  return "Avg X scans\n + Loop every 60s";
        case MODE_1KHZ_LIVE:     return "Live 1kHz stream";
        case MODE_1KHZ_AVG:      return "1kHz X samples";
        default:                 return "Select mode";
    }
}

const char* ButtonPanel::getDialogTitle(int mode) {
    switch (mode) {
        case MODE_SINGLE_SCAN:   return "Single Measurement";
        case MODE_AVERAGE_SCAN:  return "Averaging Measurement";
        case MODE_CONTINUOUS:    return "Continuous Mode";
        case MODE_AVERAGE_LOOP:  return "Average Loop Mode";
        case MODE_1KHZ_LIVE:     return "1kHz Live Stream";
        case MODE_1KHZ_AVG:      return "1kHz Average";
        default:                 return "Measurement";
    }
}

const char* ButtonPanel::getDialogMessage(int mode, int avg_count) {
    static char message[200];
    
    switch (mode) {
        case MODE_SINGLE_SCAN:
            snprintf(message, sizeof(message), "Performing single scan...\nMeasuring 19 frequencies");
            break;
        case MODE_AVERAGE_SCAN:
            snprintf(message, sizeof(message), "Performing %d scans for averaging...\nMeasuring 19 frequencies per scan", avg_count);
            break;
        case MODE_CONTINUOUS:
            snprintf(message, sizeof(message), "Continuous measurement started.\nScans every 60 seconds.\nPress STOP to end.");
            break;
        case MODE_AVERAGE_LOOP:
            snprintf(message, sizeof(message), "Average loop started (%d per cycle).\nRepeats every 60 seconds.\nPress STOP to end.", avg_count);
            break;
        case MODE_1KHZ_LIVE:
            snprintf(message, sizeof(message), "1kHz live stream started.\nReal-time impedance monitoring.\nPress STOP to end.");
            break;
        case MODE_1KHZ_AVG:
            snprintf(message, sizeof(message), "Measuring 1kHz %d times...\nAveraging samples...", avg_count);
            break;
        default:
            snprintf(message, sizeof(message), "Starting measurement...");
            break;
    }
    
    return message;
}

void ButtonPanel::updateModeDisplay() {
    if (!mode_label || !desc_label) return;
    
    if (current_mode == MODE_NONE) {
        lv_label_set_text(mode_label, "Mode: None");
        lv_label_set_text(desc_label, "Select mode");
    } else {
        lv_label_set_text_fmt(mode_label, "Mode: %d", current_mode);
        lv_label_set_text(desc_label, getModeDescription(current_mode));
    }
    
    updateAveragingSliderVisibility();
}

void ButtonPanel::showMeasurementDialog(int mode, int avg_count) {
    // Close existing dialog if any
    if (msgbox) {
        lv_obj_delete(msgbox);
        msgbox = nullptr;
    }
    
    // Initialize state
    is_measurement_running = true;
    current_cycle = 0;
    current_sweep = 0;
    total_sweeps = 0;
    current_frequency = 0;
    countdown_seconds = 0;
    update_progress_flag = false;
    update_countdown_flag = false;
    
    // Create message box - ALL MODES GET A DIALOG
    msgbox = lv_msgbox_create(lv_screen_active());
    lv_msgbox_add_title(msgbox, getDialogTitle(mode));
    
    // Add main message text
    lv_msgbox_add_text(msgbox, getDialogMessage(mode, avg_count));
    
    // Add progress text area - ALL MODES GET PROGRESS TRACKING
    progress_text_area = lv_msgbox_add_text(msgbox, "\n\n");
    lv_obj_set_style_text_align(progress_text_area, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(progress_text_area, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(progress_text_area, lv_color_hex(0x00FF00), 0);
    
    // Add status text area for all modes
    status_text_area = lv_msgbox_add_text(msgbox, "\nStatus: Initializing...");
    lv_obj_set_style_text_align(status_text_area, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(status_text_area, lv_color_hex(0xFFFF00), 0);
    
    // Add STOP button for modes 3, 4, 5 ONLY
    if (mode == MODE_CONTINUOUS || mode == MODE_AVERAGE_LOOP || mode == MODE_1KHZ_LIVE) {
        stop_btn_msgbox = lv_btn_create(msgbox);
        lv_obj_set_size(stop_btn_msgbox, 80, 30);
        lv_obj_align(stop_btn_msgbox, LV_ALIGN_BOTTOM_MID, 0, -10);
        lv_obj_set_style_bg_color(stop_btn_msgbox, lv_color_hex(0xFF4444), 0);
        lv_obj_set_style_radius(stop_btn_msgbox, 6, 0);

        lv_obj_t* stop_label = lv_label_create(stop_btn_msgbox);
        lv_label_set_text(stop_label, LV_SYMBOL_STOP " STOP");
        lv_obj_center(stop_label);
        lv_obj_set_style_text_color(stop_label, lv_color_hex(0xFFFFFF), 0);

        lv_obj_add_event_cb(stop_btn_msgbox, stopButtonEventCallback, LV_EVENT_CLICKED, this);
    } 
    
    // Style the message box
    lv_obj_set_style_bg_color(msgbox, lv_color_hex(0x2f2f2f), 0);
    lv_obj_set_style_text_color(msgbox, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(msgbox);
    
    // Add event callback
    lv_obj_add_event_cb(msgbox, msgboxEventCallback, LV_EVENT_VALUE_CHANGED, this);
    
    // Create timers for modes that need them
    if (mode == MODE_CONTINUOUS || mode == MODE_AVERAGE_LOOP) {
        if (!progress_timer) {
            progress_timer = lv_timer_create(progressTimerCallback, 500, this);
        }
        if (!countdown_timer) {
            countdown_timer = lv_timer_create(countdownTimerCallback, 1000, this);
        }
    }
    
    Serial.printf("Measurement dialog created for mode %d\n", mode);
}

void ButtonPanel::updateMeasurementProgress(int cycle, int sweep, int total_sweeps_for_cycle, int frequency, int seconds_remaining) {
    current_cycle = cycle;
    current_sweep = sweep;
    total_sweeps = total_sweeps_for_cycle;
    current_frequency = frequency;
    if (seconds_remaining >= 0) {
        countdown_seconds = seconds_remaining;
        update_countdown_flag = true;
    }
    update_progress_flag = true;
    
    // Update progress text immediately for all modes
    if (progress_text_area) {
        updateProgressText();
    }
}

void ButtonPanel::updateProgressText() {
    if (!progress_text_area) return;
    
    char progress_text[200];
    
    switch (current_mode) {
        case MODE_SINGLE_SCAN:  // mode 1
        {
            int percent = (current_frequency * 100) / 19;
            snprintf(progress_text, sizeof(progress_text), 
                    "Freq %d/19 (%d%%)", 
                    current_frequency, percent);
            break;
        }
        
        case MODE_AVERAGE_SCAN: // mode 2
        {
            int total_measurements = (current_sweep - 1) * 19 + current_frequency;
            int total_needed = total_sweeps * 19;
            int percent = (total_measurements * 100) / total_needed;
            snprintf(progress_text, sizeof(progress_text),
                    "Sweep %d/%d, Freq %d/19 (%d%%)",
                    current_sweep, total_sweeps, current_frequency, percent);
            break;
        }

        case MODE_CONTINUOUS:   // mode 3
        {
            int percent = (current_frequency * 100) / 19;
            snprintf(progress_text, sizeof(progress_text),
                    "Cycle %d | Sweep 1/1 | Freq %d/19 (%d%%)",
                    current_cycle, current_frequency, percent);
            break;
        }

        case MODE_AVERAGE_LOOP: // mode 4
        {
            int total_measurements = (current_sweep - 1) * 19 + current_frequency;
            int total_needed = total_sweeps * 19;
            int percent = (total_measurements * 100) / total_needed;
            snprintf(progress_text, sizeof(progress_text),
                    "Cycle %d | Sweep %d/%d | Freq %d/19 (%d%%)",
                    current_cycle, current_sweep, total_sweeps, current_frequency, percent);
            break;
        }
        
        case MODE_1KHZ_LIVE: {  // mode 5
            snprintf(progress_text, sizeof(progress_text),
                    "Sample %d streaming...", current_frequency);
            break;
        }
        
        case MODE_1KHZ_AVG: {   // mode 6
            int percent = (current_sweep * 100) / total_sweeps;
            snprintf(progress_text, sizeof(progress_text),
                    "Sample %d/%d (%d%%)",
                    current_sweep, total_sweeps, percent);
            break;
        }
        
        default:
            snprintf(progress_text, sizeof(progress_text), "Processing...");
            break;
    }
    
    lv_label_set_text(progress_text_area, progress_text);
}

void ButtonPanel::setMeasurementStatus(const char* status) {
    if (status_text_area) {
        lv_label_set_text_fmt(status_text_area, "\nStatus: %s", status);
        
        // Change color based on status
        if (strstr(status, "Processing") || strstr(status, "Measuring")) {
            lv_obj_set_style_text_color(status_text_area, lv_color_hex(0xFFFF00), 0);
        } else if (strstr(status, "Done") || strstr(status, "Complete")) {
            lv_obj_set_style_text_color(status_text_area, lv_color_hex(0x00FF00), 0);
            // Enable close button for modes 1, 2, 6
            // if (msgbox && (current_mode == MODE_SINGLE_SCAN || current_mode == MODE_AVERAGE_SCAN || current_mode == MODE_1KHZ_AVG)) {
            //     lv_obj_t* close_btn = lv_msgbox_add_close_button(msgbox);
            //     if (close_btn) {
            //         lv_obj_clear_state(close_btn, LV_STATE_DISABLED);
            //     }
            // }
        } else if (strstr(status, "Error") || strstr(status, "Failed")) {
            lv_obj_set_style_text_color(status_text_area, lv_color_hex(0xFF4444), 0);
        } else {
            lv_obj_set_style_text_color(status_text_area, lv_color_hex(0xFFFFFF), 0);
        }
    }
}

void ButtonPanel::closeMeasurementDialog() {
    is_measurement_running = false;
    
    // Delete timers
    if (progress_timer) {
        lv_timer_del(progress_timer);
        progress_timer = nullptr;
    }
    if (countdown_timer) {
        lv_timer_del(countdown_timer);
        countdown_timer = nullptr;
    }

    // Close message box
    if (msgbox) {
        lv_obj_delete(msgbox);
        msgbox = nullptr;
    }

    stop_btn_msgbox = nullptr;
    progress_text_area = nullptr;
    status_text_area = nullptr;

    // Re-enable main panel stop button
    // lv_obj_clear_state(stop_btn, LV_STATE_DISABLED);
    // lv_obj_set_style_bg_opa(stop_btn, LV_OPA_80, 0);
    // Re-enable start button, disable stop button
    setMeasurementRunning(false);
    
    Serial.println("Measurement dialog closed");
}

void ButtonPanel::setMeasurementRunning(bool running) {
    is_measurement_running = running;
    
    if (running) {
        lv_obj_add_state(start_btn, LV_STATE_DISABLED);
        lv_obj_set_style_bg_opa(start_btn, LV_OPA_30, 0);
        
        lv_obj_clear_state(stop_btn, LV_STATE_DISABLED);
        lv_obj_set_style_bg_opa(stop_btn, LV_OPA_80, 0);
    } else {
        lv_obj_clear_state(start_btn, LV_STATE_DISABLED);
        lv_obj_set_style_bg_opa(start_btn, LV_OPA_80, 0);
        
        lv_obj_add_state(stop_btn, LV_STATE_DISABLED);
        lv_obj_set_style_bg_opa(stop_btn, LV_OPA_30, 0);
    }
}

void ButtonPanel::setStartMeasurementCallback(void (*callback)(int mode, int avg_count)) {
    start_measurement_callback = callback;
}

void ButtonPanel::setStopMeasurementCallback(void (*callback)(void)) {
    stop_measurement_callback = callback;
}

void ButtonPanel::modeRadioEventCallback(lv_event_t* e) {
    if (!instance) return;
    
    lv_obj_t* obj = lv_event_get_target_obj(e);
    int mode = (int)(intptr_t)lv_obj_get_user_data(obj);
    
    // Uncheck all other radio buttons
    for (int i = 0; i < 6; i++) {
        if (instance->mode_radios[i] && instance->mode_radios[i] != obj) {
            lv_obj_clear_state(instance->mode_radios[i], LV_STATE_CHECKED);
        }
    }
    
    // Update current mode
    if (lv_obj_has_state(obj, LV_STATE_CHECKED)) {
        instance->current_mode = mode;
    } else {
        instance->current_mode = MODE_NONE;
    }
    
    instance->updateModeDisplay();
    
    // IMPORTANT: Notify external systems about mode change
    // This should be connected to your UserMode instance
    // Add a callback mechanism to ButtonPanel.h:
    if (instance->mode_change_callback) {
        instance->mode_change_callback(instance->current_mode);
    }
    
    Serial.printf("Mode selected: %d\n", instance->current_mode);
}

void ButtonPanel::avgSliderEventCallback(lv_event_t* e) {
    if (!instance) return;
    
    lv_obj_t* obj = lv_event_get_target_obj(e);
    int value = lv_slider_get_value(obj);
    instance->averaging_count = value;
    
    if (instance->avg_label) {
        lv_label_set_text_fmt(instance->avg_label, "Avg X: %d", value);
    }
    
    Serial.printf("Averaging count: %d\n", value);
}

void ButtonPanel::startButtonEventCallback(lv_event_t* e) {
    if (!instance || instance->is_measurement_running) return;
    
    // Validate mode selection
    if (instance->current_mode == MODE_NONE) {
        // Quick error message for no mode selected
        error_msg_box("Please select a measurement mode first!");
        return;
    }
    
    Serial.printf("Starting measurement: Mode %d, Avg Count: %d\n", 
                  instance->current_mode, instance->averaging_count);
    
    // Set measurement state
    instance->setMeasurementRunning(true);
    
    // Show appropriate dialog
    instance->showMeasurementDialog(instance->current_mode, instance->averaging_count);
    
    // Call measurement callback
    if (start_measurement_callback) {
        start_measurement_callback(instance->current_mode, instance->averaging_count);
    }
}

void ButtonPanel::msgboxEventCallback(lv_event_t* e) { // new v
    if (!instance) return;
    
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* btn = lv_event_get_target_obj(e);
    
    if (code == LV_EVENT_CLICKED && btn == instance->stop_btn_msgbox) {
        Serial.println("STOP button in msgbox clicked");
        
        // Set global stop flag
        extern volatile bool stop_requested;
        stop_requested = true;
        
        // Update status
        instance->setMeasurementStatus("Stopping...");
        
        // Call stop callback
        if (stop_measurement_callback) {
            stop_measurement_callback();
        }
    }
}

void ButtonPanel::stopButtonEventCallback(lv_event_t* e) {
    if (!instance || !instance->is_measurement_running) return;
    
    // Prevent duplicate stop processing
    //static bool stop_in_progress = false;
    if (instance->stop_in_progress) {
        Serial.println("Stop already in progress, ignoring duplicate");
        return;
    }
    instance->stop_in_progress = true;

    Serial.println("STOP button pressed - requesting measurement stop");
    
    // Set global stop flag
    extern volatile bool stop_requested;
    stop_requested = true;
    Serial.println("Stop flag set to TRUE");
    
    // Update status immediately on this thread
    instance->setMeasurementStatus("Stopping...");
    Serial.println("Status updated to Stopping");
    
    lv_timer_handler();
    
    // Call stop callback if available
    if (stop_measurement_callback) {
        stop_measurement_callback();
    }
    
    lv_timer_handler();

    if (stop_measurement_callback) {
        stop_measurement_callback();
        Serial.println("Stop callback called");
    }
    instance->stop_in_progress = false;
    // Don't try to close dialog here - let UserMode handle it
    Serial.println("Stop request sent to UserMode");
}

void ButtonPanel::progressTimerCallback(lv_timer_t* timer) {
    ButtonPanel* panel = (ButtonPanel*)lv_timer_get_user_data(timer);
    if (!panel || !panel->is_measurement_running || !panel->progress_text_area) return;
    
    // Only update if flag is set
    if (!panel->update_progress_flag) return;
    
    char progress_text[200];
    
    if (panel->current_mode == MODE_CONTINUOUS) {
        // Mode 3: Single sweep per cycle
        int percent = (panel->current_frequency * 100) / 19;
        if (panel->current_frequency > 0) {
            snprintf(progress_text, sizeof(progress_text),
                    "Cycle %d | Freq %d/19 (%d%%)",
                    panel->current_cycle, panel->current_frequency, percent);
        } else {
            snprintf(progress_text, sizeof(progress_text),
                    "Cycle: %d\nInitializing...",
                    panel->current_cycle);
        }
    } else if (panel->current_mode == MODE_AVERAGE_LOOP) {
        // Mode 4: Multiple sweeps averaged per cycle
        int total_measurements = (panel->current_sweep - 1) * 19 + panel->current_frequency;
        int total_needed = panel->total_sweeps * 19;
        int percent = (total_measurements * 100) / total_needed;
        if (panel->current_frequency > 0) {
            snprintf(progress_text, sizeof(progress_text),
                    "Cycle %d | Sweep %d/%d | Freq %d/19 (%d%%)",
                    panel->current_cycle, panel->current_sweep, panel->total_sweeps,
                    panel->current_frequency, percent);
        } else {
            snprintf(progress_text, sizeof(progress_text),
                    "Cycle %d | Sweep %d/%d | Freq 0/19 (0%%)",
                    panel->current_cycle, panel->current_sweep, panel->total_sweeps);
        }
    }
    
    lv_label_set_text(panel->progress_text_area, progress_text);
    lv_obj_center(panel->msgbox);
    
    panel->update_progress_flag = false;
}

void ButtonPanel::countdownTimerCallback(lv_timer_t* timer) {
    ButtonPanel* panel = (ButtonPanel*)lv_timer_get_user_data(timer);
    if (!panel || !panel->is_measurement_running) return;
    
    // Only update countdown if flag is set
    if (!panel->update_countdown_flag) return;
    
    if (panel->countdown_seconds > 0) {
        panel->countdown_seconds--;
        Serial.printf("Countdown: %d seconds remaining\n", panel->countdown_seconds);
        
        // Update status with countdown
        char status_text[50];
        snprintf(status_text, sizeof(status_text), "Waiting %d seconds...", panel->countdown_seconds);
        panel->setMeasurementStatus(status_text);
        
        if (panel->countdown_seconds == 0) {
            Serial.println("60 second wait period completed - starting next cycle");
            panel->setMeasurementStatus("Starting next cycle...");
            panel->update_countdown_flag = false;
        }
    }
}
