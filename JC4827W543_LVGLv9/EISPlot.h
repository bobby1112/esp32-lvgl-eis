#ifndef EISPlot_h
#define EISPlot_h

#include <lvgl.h>
#include <Arduino.h>
#include "MeasurementMode.h"

class EISPlot {
public:
    EISPlot();
    ~EISPlot();
    
    // Initialize the plot with container
    void init(lv_obj_t* parent = nullptr);

// Unified table system - one popup for all table types
    void showTimeDataTable(int count);
    void showImpedanceDataTable(const double* real_vals, const double* imag_vals, int count);
    void show1kHzDataTable();
    void showNoDataTable(const char* message);
    void hideTablePopup();
    
    // Mode management for table button visibility and data persistence
    void setCurrentMode(int mode);
    void updateTableButtonVisibility();
    void clearModeData(int mode);
    void saveMode5Data();
    void saveMode6Data();
    void restoreMode5Data();
    void restoreMode6Data();

    // void updateTableButtonVisibility(int mode);
    
    // Switch between plot modes
    void switchToTimePlot();   // For modes 5 & 6
    void showNyquistPlot();     // For modes 1-4
    
    // Update methods
    void updateSinglePoint(int freq_index, double real_val, double imag_val);  // For Nyquist
    void addTimePoint(double real_val, double imag_val);  // For time-based plotting
    bool updateData(const double* real_data, const double* imag_data, int data_count);  // Full dataset
    void updateVoltageTemp(double voltage, double temperature);  // Always updates when changed
    
    // Auto-scaling and range control
    void autoScale();
    void setDataRange(double real_min, double real_max, double imag_min, double imag_max);
    
    // Control methods
    void clearData();
    bool isMeasurementComplete() { return measurement_completed; }
    void resetMeasurementState();
    void setMeasurementCallback(void (*callback)(void));
    void setStatusText(const char* text);
    
    // Public access for outlier detection
    double* voltages_at_measurement;
    static const int MAX_1KHZ_SAMPLES = 1000;
    double all_1khz_real[MAX_1KHZ_SAMPLES];
    double all_1khz_imag[MAX_1KHZ_SAMPLES];
    int total_1khz_samples;
    
    // Get container and labels
    lv_obj_t* voltage_label;
    lv_obj_t* temp_label;
    lv_obj_t* getContainer() { return container; }
    lv_obj_t* getStatusLabel() { return status_label; }
    
private:
    // UI objects
    lv_obj_t* container;
    lv_obj_t* chart_container;
    lv_obj_t* chart;           // Active chart pointer
    lv_obj_t* nyquist_chart;   // Nyquist plot for modes 1-4
    lv_obj_t* time_chart;      // Time-based plot for modes 5-6
    lv_obj_t* title;
    lv_obj_t* value_label;
    lv_obj_t* table_btn;
    // lv_obj_t* data_table;     // The popup table
    lv_obj_t* status_label;
    lv_obj_t* x_axis_label;
    lv_obj_t* y_axis_label;

    lv_obj_t* table_popup;     // Main popup container
    lv_obj_t* data_table;      // Table widget inside popup
    bool table_visible;        // Track visibility state
    
    // Chart components
    lv_chart_series_t* series;              // For Nyquist plot
    lv_chart_series_t* time_series_real;    // For time plot - real values
    lv_chart_series_t* time_series_imag;    // For time plot - imaginary values
    lv_chart_cursor_t* cursor;
    
    // Data arrays for LVGL
    static const int MAX_POINTS = 19;

    lv_coord_t x_points[MAX_POINTS];
    lv_coord_t y_points[MAX_POINTS];
    lv_coord_t time_x_points[200];   // or use a const MAX_TIME_SAMPLES
    lv_coord_t time_y_points[200];
    
    // Plot mode control
    //bool table_visible;       // Track visibility
    bool time_plot_mode;
    int time_plot_count;
    int max_time_samples;
    
    // Data storage for Nyquist
    double* real_values;
    double* imag_values;
    int point_count;
    bool data_valid[MAX_POINTS];
    bool measurement_completed;
    
    // Data storage for time plot
    double* time_real_values;
    double* time_imag_values;

    // Current mode tracking
    int current_measurement_mode;
    
    // Separate data storage for Mode 5 (1kHz Live)
    double mode5_real_data[MAX_1KHZ_SAMPLES];
    double mode5_imag_data[MAX_1KHZ_SAMPLES];
    int mode5_sample_count;
    
    // Separate data storage for Mode 6 (1kHz Averaged)  
    double mode6_real_data[MAX_1KHZ_SAMPLES];
    double mode6_imag_data[MAX_1KHZ_SAMPLES];
    int mode6_sample_count;

    // Frequency mapping
    int freq_plot_order[MAX_POINTS];
    
    // Scaling parameters
    double real_min, real_max;
    double imag_min, imag_max;
    bool auto_scale_enabled;
    double min_range_span;
    
    // Voltage tracking
    double invalid_voltage_threshold;
    double last_valid_voltage;
    double last_voltage_update;
    double last_temp_update;
    int max_retries;
    
    // Callback
    static void (*measurement_callback)(void);
    
    // Internal methods
    void createChart();
    void createNyquistChart();
    void createTimeChart();
    void createAxisLabels();
    void createLabels();
    void createStatusLabel();
    void fillChartArrays();
    void refreshChart();
    void updateChartRanges();
    void calculateOptimalRange(double min_val, double max_val, double& range_min, double& range_max, int& divisions);
    void calculateBetterSpacing(double min_val, double max_val, double& range_min, double& range_max, int& divisions);
    double roundToNiceNumber(double value, bool round_up);
    void sortPointsByFrequency();
    bool isValidVoltage(double voltage);
    void autoScaleNyquistPlot();
    void autoScaleTimePlot();
    double roundToFineNumber(double value, bool round_up);
    int calculateFineDivisions(double range);
    void fillTimeChartArrays();
    // void showDataTable(const double* real_vals, const double* imag_vals, int count); 
    // void hideDataTable();
    void createTablePopup();   // Create the unified popup container
    
    // Static event callback
    static void chartEventCallback(lv_event_t* e);
    static void measurementButtonCallback(lv_event_t* e);
    static void tableButtonEventCallback(lv_event_t* e);
    //static void tableCloseCallback(lv_event_t* e);

    // Instance pointer
    static EISPlot* instance;
    
    // Unicode Omega symbol
    static const char* OHM_SYMBOL;
};

#endif