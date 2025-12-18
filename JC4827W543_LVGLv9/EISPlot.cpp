#include "EISPlot.h"
#include <math.h>

// Static instance pointer
EISPlot* EISPlot::instance = nullptr;

// Static measurement callback pointer
void (*EISPlot::measurement_callback)(void) = nullptr;

// Unicode Omega symbol (Ω) as UTF-8
const char* EISPlot::OHM_SYMBOL = "Ω"; // \u2126 "u03A9" Ω U+03A9
// LV_FONT_DECLARE(monsterrat_10_ohm);
// LV_FONT_DECLARE(monsterrat_12_ohm);

EISPlot::EISPlot() : container(nullptr), chart_container(nullptr), chart(nullptr), 
                     nyquist_chart(nullptr), time_chart(nullptr), title(nullptr),  
                     value_label(nullptr), voltage_label(nullptr), temp_label(nullptr), table_btn(nullptr), status_label(nullptr),
                     table_popup(nullptr), data_table(nullptr), table_visible(false),
                     series(nullptr), time_series_real(nullptr), time_series_imag(nullptr), 
                     cursor(nullptr), real_values(nullptr), imag_values(nullptr), 
                     time_real_values(nullptr), time_imag_values(nullptr),
                     voltages_at_measurement(nullptr),
                     measurement_completed(false), auto_scale_enabled(true),
                     last_valid_voltage(0.0), min_range_span(50.0),
                     invalid_voltage_threshold(2.0), max_retries(3),
                     real_min(1000), real_max(1300), 
                     imag_min(100), imag_max(120),
                     time_plot_mode(false), time_plot_count(0), max_time_samples(200),
                     last_voltage_update(0), last_temp_update(0), current_measurement_mode(MODE_NONE),  // NEW
                     mode5_sample_count(0), mode6_sample_count(0), total_1khz_samples(0){
    instance = this;

    // Initialize data validity array
    for (int i = 0; i < MAX_POINTS; i++) {
        data_valid[i] = false;
        freq_plot_order[i] = i; 
    }

// Initialize 1kHz sample arrays
    for (int i = 0; i < MAX_1KHZ_SAMPLES; i++) {
        all_1khz_real[i] = 0.0;
        all_1khz_imag[i] = 0.0;
        mode5_real_data[i] = 0.0;  // NEW
        mode5_imag_data[i] = 0.0;  // NEW
        mode6_real_data[i] = 0.0;  // NEW
        mode6_imag_data[i] = 0.0;  // NEW
    }
}

EISPlot::~EISPlot() {
    // Safe cleanup of unified table system
    if (table_popup) {
        lv_obj_del(table_popup);
        table_popup = nullptr;
        data_table = nullptr;  // Deleted with popup
    }
    if (real_values) delete[] real_values;
    if (imag_values) delete[] imag_values;
    if (time_real_values) delete[] time_real_values;
    if (time_imag_values) delete[] time_imag_values;
    if (voltages_at_measurement) delete[] voltages_at_measurement;
    instance = nullptr;
}

void EISPlot::init(lv_obj_t* parent) {
    if (!parent) parent = lv_screen_active();
    
    // Create main container
    container = lv_obj_create(parent);
    lv_obj_set_size(container, 480, 272);
    lv_obj_align(container, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 0, 0);
    
    createChart();
    createLabels();
    //createStatusLabel();
    clearData();
}

void EISPlot::createChart() {
    // Create scrollable container for chart
    chart_container = lv_obj_create(container);
    lv_obj_set_size(chart_container, 320, 150);
    lv_obj_align(chart_container, LV_ALIGN_LEFT_MID, 20, -10);
    lv_obj_set_style_bg_opa(chart_container, LV_OPA_10, 0);
    lv_obj_set_style_border_width(chart_container, 1, 0);
    lv_obj_set_style_border_color(chart_container, lv_color_hex(0x404040), 0);
    lv_obj_set_style_radius(chart_container, 4, 0);
    
    // Enable scrolling
    lv_obj_set_scroll_dir(chart_container, LV_DIR_ALL);
    lv_obj_set_style_pad_all(chart_container, 5, 0);
    lv_obj_set_scrollbar_mode(chart_container, LV_SCROLLBAR_MODE_AUTO);

    // Create Nyquist chart
    nyquist_chart = lv_chart_create(chart_container);
    lv_obj_set_size(nyquist_chart, 300, 400);
    lv_obj_align(nyquist_chart, LV_ALIGN_CENTER, 0, 0);
    
    // Configure chart for scatter plot (X-Y data)
    lv_chart_set_type(nyquist_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_range(nyquist_chart, LV_CHART_AXIS_PRIMARY_X, (int)real_min, (int)real_max);
    lv_chart_set_range(nyquist_chart, LV_CHART_AXIS_PRIMARY_Y, (int)imag_min, (int)imag_max);
    lv_chart_set_point_count(nyquist_chart, MAX_POINTS);
    
    // Style the chart
    lv_obj_set_style_size(nyquist_chart, 16, 16, LV_PART_ITEMS);  // Point size
    lv_obj_set_style_bg_color(nyquist_chart, lv_color_hex(0x000000), 0);  // Black background
    lv_obj_set_style_line_color(nyquist_chart, lv_color_hex(0x404040), LV_PART_MAIN);  // Grid color
    lv_obj_set_style_line_width(nyquist_chart, 3, LV_PART_ITEMS);  // Thicker connecting lines
    lv_chart_set_div_line_count(nyquist_chart, 8, 10);  // Grid lines
    
    // Add series for impedance data (red points)
    series = lv_chart_add_series(nyquist_chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
    
    // Enable clickable and add cursor
    lv_obj_add_flag(nyquist_chart, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(nyquist_chart, 15, LV_PART_MAIN);  // Add padding for better touch response
    cursor = lv_chart_add_cursor(nyquist_chart, lv_palette_main(LV_PALETTE_BLUE), 
                                (lv_dir_t)(LV_DIR_LEFT | LV_DIR_BOTTOM));
    
    // Add event callback
    lv_obj_add_event_cb(nyquist_chart, chartEventCallback, LV_EVENT_ALL, this);
    
    // Create time chart (for modes 5-6) - configured as scatter plot like Nyquist
    time_chart = lv_chart_create(chart_container);
    lv_obj_set_size(time_chart, 300, 400);
    lv_obj_align(time_chart, LV_ALIGN_CENTER, 0, 0);
    
    // Configure time chart as scatter plot (Real vs Imaginary, like Nyquist)
    lv_chart_set_type(time_chart, LV_CHART_TYPE_SCATTER);
    lv_chart_set_range(time_chart, LV_CHART_AXIS_PRIMARY_X, 0, 200);  // Will be auto-scaled
    lv_chart_set_range(time_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 200);  // Will be auto-scaled
    lv_chart_set_point_count(time_chart, max_time_samples);
    
    // Style the time chart (same as Nyquist)
    lv_obj_set_style_size(time_chart, 12, 12, LV_PART_ITEMS);  // Slightly smaller points for live data
    lv_obj_set_style_bg_color(time_chart, lv_color_hex(0x000000), 0);  // Black background
    lv_obj_set_style_line_color(time_chart, lv_color_hex(0x404040), LV_PART_MAIN);  // Grid color
    lv_chart_set_div_line_count(time_chart, 8, 10);  // Grid lines
    
    // Add series for real vs imaginary impedance (live scatter plot)
    time_series_real = lv_chart_add_series(time_chart, lv_palette_main(LV_PALETTE_GREEN), 
                                           LV_CHART_AXIS_PRIMARY_Y);
    
    // Enable clickable for time chart too
    lv_obj_add_flag(time_chart, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(time_chart, 15, LV_PART_MAIN);
    
    // Initially hide time chart
    lv_obj_add_flag(time_chart, LV_OBJ_FLAG_HIDDEN);
    
    // Set nyquist_chart as default active chart
    chart = nyquist_chart;
    
    // Chart title
    title = lv_label_create(container);
    lv_label_set_text(title, "EIS Nyquist Plot (Real vs Imaginary)");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);   
    lv_obj_align_to(title, chart_container, LV_ALIGN_OUT_TOP_MID, 0, -10);
    
    // Axis labels
    lv_obj_t* x_label = lv_label_create(container);
    char buf[64];
    snprintf(buf, sizeof(buf), "Real Impedance (m%s)", OHM_SYMBOL);
    lv_label_set_text(x_label, buf);
    lv_obj_set_style_text_font(x_label, &montserrat_10_ohm, 0);  
    lv_obj_set_style_text_color(x_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align_to(x_label, chart_container, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    x_axis_label = x_label;

    // Create canvas for rotated Y-axis label
    lv_obj_t* y_label_canvas = lv_canvas_create(container);
    lv_color_t parent_bg = lv_obj_get_style_bg_color(container, LV_PART_MAIN);
    
    const int canvas_width = 180;
    const int canvas_height = 20;
    static lv_color_t y_label_cbuf[180*20];

    lv_canvas_set_buffer(y_label_canvas, y_label_cbuf, canvas_width, canvas_height, LV_COLOR_FORMAT_RGB565); 
    // lv_canvas_fill_bg(y_label_canvas, lv_color_hex(0x080A0F), LV_OPA_TRANSP);
    lv_canvas_fill_bg(y_label_canvas, parent_bg, LV_OPA_COVER);

    lv_obj_align_to(y_label_canvas, container, LV_ALIGN_LEFT_MID, 0, 100);
    
    lv_draw_label_dsc_t y_label_dsc;
    lv_draw_label_dsc_init(&y_label_dsc);
    y_label_dsc.color = lv_color_hex(0xFFFFCC);
    y_label_dsc.align = LV_TEXT_ALIGN_CENTER;
    y_label_dsc.font = &montserrat_10_ohm; 

    char y_buf[64];
    snprintf(y_buf, sizeof(y_buf), "Imaginary Impedance (m%s)", OHM_SYMBOL);
    y_label_dsc.text = y_buf;

    lv_layer_t layer;
    lv_canvas_init_layer(y_label_canvas, &layer);

    // Define the area where text will be drawn
    lv_area_t text_area;
    text_area.x1 = 5;
    text_area.y1 = 5;
    text_area.x2 = canvas_width - 5;
    text_area.y2 = canvas_height - 5;

    lv_draw_label(&layer, &y_label_dsc, &text_area);
    lv_canvas_finish_layer(y_label_canvas, &layer);
    lv_obj_set_style_transform_rotation(y_label_canvas, 2700, 0);
    lv_obj_move_background(y_label_canvas);
    y_axis_label = y_label_canvas;
}

void EISPlot::createLabels() {
    // Value display label (bottom left)
    value_label = lv_label_create(container);
    lv_label_set_text(value_label, "Click a point to see values");
    lv_obj_set_style_text_font(value_label, &montserrat_10_ohm, 0);
    lv_obj_set_style_text_color(value_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align_to(value_label, chart_container, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 15);
    
    // Voltage label (large font, blue)
    voltage_label = lv_label_create(container);
    lv_label_set_text(voltage_label, "V: -.--- V");
    lv_obj_set_style_text_font(voltage_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(voltage_label, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_align_to(voltage_label, value_label, LV_ALIGN_OUT_BOTTOM_LEFT, 100, 5);
    
    // Temperature label (large font, orange)
    temp_label = lv_label_create(container);
    lv_label_set_text(temp_label, "T: --.- °C");
    lv_obj_set_style_text_font(temp_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(temp_label, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_obj_align_to(temp_label, voltage_label, LV_ALIGN_OUT_RIGHT_MID, 60, 0);

    // Create table button with safety checks
    table_btn = lv_btn_create(container);
    if (!table_btn) {
        Serial.println("EISPlot: Failed to create table_btn");
        return;
    }
    
    lv_obj_set_size(table_btn, 30, 20);
    lv_obj_align_to(table_btn, temp_label, LV_ALIGN_OUT_TOP_MID, 15, -5);  // Adjusted alignment
    lv_obj_set_style_bg_color(table_btn, lv_palette_main(LV_PALETTE_BLUE), 0);

    lv_obj_t* icon = lv_label_create(table_btn);
    if (!icon) {
        Serial.println("EISPlot: Failed to create table button icon");
        return;
    }
    lv_label_set_text(icon, LV_SYMBOL_LIST);   // Table/list icon
    lv_obj_center(icon);

    lv_obj_add_event_cb(table_btn, tableButtonEventCallback, LV_EVENT_CLICKED, this);
    lv_obj_add_flag(table_btn, LV_OBJ_FLAG_HIDDEN);  // Hidden by default
    
    Serial.println("EISPlot: All labels and table button created successfully");
}

void EISPlot::createStatusLabel() {
    // Create status label above refresh button
    status_label = lv_label_create(container);
    lv_label_set_text(status_label, "Ready");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xFFFF00), 0);  // Yellow
    lv_obj_set_style_text_align(status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(status_label, title, LV_ALIGN_TOP_RIGHT, 120, -15);
}

void EISPlot::createTablePopup() {
    // Create main popup container
    table_popup = lv_obj_create(lv_screen_active());
    lv_obj_set_size(table_popup, 350, 200);
    lv_obj_center(table_popup);
    lv_obj_set_style_bg_color(table_popup, lv_color_hex(0x2f2f2f), 0);
    lv_obj_set_style_border_width(table_popup, 2, 0);
    lv_obj_set_style_border_color(table_popup, lv_color_hex(0x606060), 0);
    lv_obj_set_style_radius(table_popup, 8, 0);
    lv_obj_set_style_pad_all(table_popup, 8, 0);
    
    // Create close button OUTSIDE the table (in popup container)
    lv_obj_t* close_btn = lv_btn_create(table_popup);
    lv_obj_set_size(close_btn, 30, 30);
    lv_obj_align(close_btn, LV_ALIGN_TOP_RIGHT, 5, 0);
    lv_obj_set_style_bg_color(close_btn, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_radius(close_btn, 15, 0);
    
    lv_obj_t* close_label = lv_label_create(close_btn);
    lv_label_set_text(close_label, LV_SYMBOL_CLOSE);
    lv_obj_center(close_label);
    lv_obj_set_style_text_color(close_label, lv_color_hex(0xFFFFFF), 0);
    
    // Close button callback
    lv_obj_add_event_cb(close_btn, [](lv_event_t* e) {
        EISPlot* self = static_cast<EISPlot*>(lv_event_get_user_data(e));
        if (self) {
            self->hideTablePopup();
        }
    }, LV_EVENT_CLICKED, this);
    
    // Create scrollable container for table content
    lv_obj_t* scroll_container = lv_obj_create(table_popup);
    lv_obj_set_size(scroll_container, 320, 150);
    lv_obj_align(scroll_container, LV_ALIGN_BOTTOM_MID, 0, -3);
    lv_obj_set_style_bg_opa(scroll_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(scroll_container, 0, 0);
    lv_obj_set_style_pad_all(scroll_container, 5, 0);
    lv_obj_set_scroll_dir(scroll_container, (lv_dir_t)(LV_DIR_VER | LV_DIR_HOR));
    lv_obj_set_scrollbar_mode(scroll_container, LV_SCROLLBAR_MODE_AUTO);
    
    // Create table inside scroll container
    data_table = lv_table_create(scroll_container);
    lv_obj_set_size(data_table, 350, LV_SIZE_CONTENT);
    lv_obj_align(data_table, LV_ALIGN_TOP_LEFT, -10, -30);
    lv_obj_set_style_text_font(data_table, &montserrat_12_ohm, 0); 
    lv_obj_set_style_bg_color(data_table, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_text_color(data_table, lv_color_hex(0xFFFFFF), 0);
}

void EISPlot::hideTablePopup() {
    if (table_popup) {
        lv_obj_del(table_popup);
        table_popup = nullptr;
        data_table = nullptr;  // Table is deleted with popup
    }
    table_visible = false;
    Serial.println("Table popup closed safely");
}

void EISPlot::showNoDataTable(const char* message) {
    if (table_visible) {
        hideTablePopup();
    }
    
    createTablePopup();
    
    // Configure table for "no data" message
    lv_table_set_col_cnt(data_table, 1);
    lv_table_set_row_cnt(data_table, 2);
    lv_table_set_col_width(data_table, 0, 340);
    
    // Header
    lv_table_set_cell_value(data_table, 0, 0, "Data Status");
    lv_obj_set_style_text_align(data_table, LV_TEXT_ALIGN_CENTER, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(data_table, lv_palette_main(LV_PALETTE_BLUE), LV_PART_ITEMS);
    
    // Message
    lv_table_set_cell_value(data_table, 1, 0, message);
    
    table_visible = true;
}

void EISPlot::show1kHzDataTable() {
    if (total_1khz_samples == 0) {
        showNoDataTable("No 1kHz data recorded yet");
        return;
    }
    
    if (table_visible) {
        hideTablePopup();
    }
    
    createTablePopup();
    
    // Configure table for 1kHz data
    lv_table_set_col_cnt(data_table, 3);
    lv_table_set_col_width(data_table, 0, 90);   // Sample
    lv_table_set_col_width(data_table, 1, 120);  // Real
    lv_table_set_col_width(data_table, 2, 120);  // Imaginary
    
    // Headers
    lv_table_set_cell_value(data_table, 1, 0, "Sample");
    lv_table_set_cell_value(data_table, 1, 1, "Real (mΩ)");
    lv_table_set_cell_value(data_table, 1, 2, "Imag (mΩ)");
    
    // Determine how many rows to show
    int rows_to_show = total_1khz_samples;
    int max_display_rows = 300;  // Limit for performance
    bool truncated = false;
    
    if (rows_to_show > max_display_rows) {
        rows_to_show = max_display_rows;
        truncated = true;
    }
    
    // Set table size
    lv_table_set_row_cnt(data_table, rows_to_show + 1 + (truncated ? 1 : 0));
    
    // Fill data rows
    for (int i = 0; i < rows_to_show; i++) {
        char buf[32];
        
        snprintf(buf, sizeof(buf), "%d", i + 1);
        lv_table_set_cell_value(data_table, i + 2, 0, buf);
        
        snprintf(buf, sizeof(buf), "%.4f", all_1khz_real[i]);
        lv_table_set_cell_value(data_table, i + 2, 1, buf);
        
        snprintf(buf, sizeof(buf), "%.4f", all_1khz_imag[i]);
        lv_table_set_cell_value(data_table, i + 2, 2, buf);
    }
    
    // Add truncation info if needed
    if (truncated) {
        lv_table_set_cell_value(data_table, rows_to_show + 1, 0, "...");
        lv_table_set_cell_value(data_table, rows_to_show + 1, 1, "More data");
        char total_info[32];
        snprintf(total_info, sizeof(total_info), "Total: %d", total_1khz_samples);
        lv_table_set_cell_value(data_table, rows_to_show + 1, 2, total_info);
    }
    
    table_visible = true;
    Serial.printf("Showing 1kHz data table: %d/%d samples\n", rows_to_show, total_1khz_samples);
}

void EISPlot::showTimeDataTable(int count) {  // no need
    if (count == 0 || !time_real_values) {
        showNoDataTable("No time series data available");
        return;
    }
    
    if (table_visible) {
        hideTablePopup();
    }
    
    createTablePopup();
    
    // Configure table for time data
    lv_table_set_col_cnt(data_table, 3);
    lv_table_set_col_width(data_table, 0, 80);
    lv_table_set_col_width(data_table, 1, 130);
    lv_table_set_col_width(data_table, 2, 130);
    
    // Headers
    lv_table_set_cell_value(data_table, 0, 0, "Point");
    lv_table_set_cell_value(data_table, 0, 1, "Real (mΩ)");
    lv_table_set_cell_value(data_table, 0, 2, "Imag (mΩ)");
    
    int display_count = (count > 50) ? 50 : count;
    lv_table_set_row_cnt(data_table, display_count + 1);
    
    for (int i = 0; i < display_count; i++) {
        char buf[32];
        
        snprintf(buf, sizeof(buf), "%d", i + 1);
        lv_table_set_cell_value(data_table, i + 1, 0, buf);
        
        snprintf(buf, sizeof(buf), "%.4f", time_real_values[i]);
        lv_table_set_cell_value(data_table, i + 1, 1, buf);
        
        snprintf(buf, sizeof(buf), "%.4f", time_imag_values[i]);
        lv_table_set_cell_value(data_table, i + 1, 2, buf);
    }
    
    table_visible = true;
}

void EISPlot::showImpedanceDataTable(const double* real_vals, const double* imag_vals, int count) { // no need
    if (count == 0 || !real_vals || !imag_vals) {
        showNoDataTable("No impedance data available");
        return;
    }
    
    if (table_visible) {
        hideTablePopup();
    }
    
    createTablePopup();
    
    // Configure table for impedance data
    lv_table_set_col_cnt(data_table, 3);
    lv_table_set_col_width(data_table, 0, 80);
    lv_table_set_col_width(data_table, 1, 130);
    lv_table_set_col_width(data_table, 2, 130);
    
    // Headers
    lv_table_set_cell_value(data_table, 0, 0, "Index");
    lv_table_set_cell_value(data_table, 0, 1, "Real (mΩ)");
    lv_table_set_cell_value(data_table, 0, 2, "Imag (mΩ)");
    
    lv_table_set_row_cnt(data_table, count + 1);
    
    for (int i = 0; i < count; i++) {
        char buf[32];
        
        snprintf(buf, sizeof(buf), "%d", i + 1);
        lv_table_set_cell_value(data_table, i + 1, 0, buf);
        
        snprintf(buf, sizeof(buf), "%.4f", real_vals[i]);
        lv_table_set_cell_value(data_table, i + 1, 1, buf);
        
        snprintf(buf, sizeof(buf), "%.4f", imag_vals[i]);
        lv_table_set_cell_value(data_table, i + 1, 2, buf);
    }
    
    table_visible = true;
}

// void EISPlot::showTimeDataTable(int count) {
//     showDataTable(time_real_values, time_imag_values, count);
// }

// void EISPlot::showImpedanceDataTable(const double* real_vals, const double* imag_vals, int count) {
//     showDataTable(real_vals, imag_vals, count);
// }

// // Updated showDataTable with close button:
// void EISPlot::showDataTable(const double* real_vals, const double* imag_vals, int count) {
//     if (data_table) lv_obj_del(data_table); // remove old
    
//     data_table = lv_table_create(lv_screen_active());
//     lv_obj_set_size(data_table, 300, 250);
//     lv_obj_center(data_table);
//     lv_table_set_col_cnt(data_table, 3);
    
//     lv_obj_set_style_text_font(data_table, &montserrat_12_ohm, 0);

//     // Header
//     lv_table_set_cell_value(data_table, 0, 0, "Index");
//     lv_table_set_cell_value(data_table, 0, 1, "Real (mΩ)");
//     lv_table_set_cell_value(data_table, 0, 2, "Imag (mΩ)");

//     for (int i = 0; i < count; i++) {
//         char buf[32];
//         snprintf(buf, sizeof(buf), "%d", i+1);
//         lv_table_set_cell_value(data_table, i+1, 0, buf);

//         snprintf(buf, sizeof(buf), "%.3f", real_vals[i]);
//         lv_table_set_cell_value(data_table, i+1, 1, buf);

//         snprintf(buf, sizeof(buf), "%.3f", imag_vals[i]);
//         lv_table_set_cell_value(data_table, i+1, 2, buf);
//     }

//     // Enable scroll if many rows
//     lv_obj_set_scroll_dir(data_table, LV_DIR_VER);
//     lv_obj_set_scrollbar_mode(data_table, LV_SCROLLBAR_MODE_AUTO);

//     // Add close button
//     lv_obj_t* close_btn = lv_btn_create(data_table);
//     lv_obj_set_size(close_btn, 60, 30);
//     lv_obj_align(close_btn, LV_ALIGN_TOP_RIGHT, -5, 5);
//     lv_obj_set_style_bg_color(close_btn, lv_palette_main(LV_PALETTE_RED), 0);
    
//     lv_obj_t* close_label = lv_label_create(close_btn);
//     lv_label_set_text(close_label, LV_SYMBOL_CLOSE);
//     lv_obj_center(close_label);
    
//     lv_obj_add_event_cb(close_btn, tableCloseCallback, LV_EVENT_CLICKED, this);

//     table_visible = true;
// }
// void EISPlot::hideDataTable() {
//     if (data_table) {
//         lv_obj_del(data_table);
//         data_table = nullptr;
//     }
//     table_visible = false;
// }

// // NEW: Function to show all 1kHz data in table
// void EISPlot::show1kHzDataTable() {
//     if (total_1khz_samples == 0) {
//         // Show empty table with message
//         if (data_table) lv_obj_del(data_table);
        
//         data_table = lv_table_create(lv_screen_active());
//         lv_obj_set_size(data_table, 350, 250);
//         lv_obj_center(data_table);
//         lv_table_set_col_cnt(data_table, 1);
        
//         lv_table_set_cell_value(data_table, 0, 0, "No data recorded yet");
        
//         // Add close button
//         lv_obj_t* close_btn = lv_btn_create(data_table);
//         lv_obj_set_size(close_btn, 60, 30);
//         lv_obj_align(close_btn, LV_ALIGN_TOP_RIGHT, -5, 5);
//         lv_obj_set_style_bg_color(close_btn, lv_palette_main(LV_PALETTE_RED), 0);
        
//         lv_obj_t* close_label = lv_label_create(close_btn);
//         lv_label_set_text(close_label, LV_SYMBOL_CLOSE);
//         lv_obj_center(close_label);
        
//         lv_obj_add_event_cb(close_btn, tableCloseCallback, LV_EVENT_CLICKED, this);
//         table_visible = true;
//         return;
//     }
    
//     if (data_table) lv_obj_del(data_table);
    
//     data_table = lv_table_create(lv_screen_active());
//     lv_obj_set_size(data_table, 400, 300);
//     lv_obj_center(data_table);
//     lv_table_set_col_cnt(data_table, 3);
    
//     // Set smaller font for more data
//     lv_obj_set_style_text_font(data_table, &lv_font_montserrat_10, 0);

//     // Header
//     lv_table_set_cell_value(data_table, 0, 0, "Sample");
//     lv_table_set_cell_value(data_table, 0, 1, "Real (mΩ)");
//     lv_table_set_cell_value(data_table, 0, 2, "Imag (mΩ)");

//     // Add all recorded samples
//     int rows_to_show = total_1khz_samples;
//     if (rows_to_show > 50) rows_to_show = 50;  // Limit to 50 rows for performance
    
//     for (int i = 0; i < rows_to_show; i++) {
//         char buf[32];
//         snprintf(buf, sizeof(buf), "%d", i + 1);
//         lv_table_set_cell_value(data_table, i + 1, 0, buf);

//         snprintf(buf, sizeof(buf), "%.4f", all_1khz_real[i]);
//         lv_table_set_cell_value(data_table, i + 1, 1, buf);

//         snprintf(buf, sizeof(buf), "%.4f", all_1khz_imag[i]);
//         lv_table_set_cell_value(data_table, i + 1, 2, buf);
//     }
    
//     if (total_1khz_samples > 50) {
//         // Add info row about truncation
//         lv_table_set_cell_value(data_table, 51, 0, "...");
//         lv_table_set_cell_value(data_table, 51, 1, "More data");
//         char total_info[32];
//         snprintf(total_info, sizeof(total_info), "Total: %d", total_1khz_samples);
//         lv_table_set_cell_value(data_table, 51, 2, total_info);
//     }

//     // Enable scroll
//     lv_obj_set_scroll_dir(data_table, LV_DIR_VER);
//     lv_obj_set_scrollbar_mode(data_table, LV_SCROLLBAR_MODE_AUTO);

//     // Add close button
//     lv_obj_t* close_btn = lv_btn_create(data_table);
//     lv_obj_set_size(close_btn, 60, 30);
//     lv_obj_align(close_btn, LV_ALIGN_TOP_RIGHT, -5, 5);
//     lv_obj_set_style_bg_color(close_btn, lv_palette_main(LV_PALETTE_RED), 0);
    
//     lv_obj_t* close_label = lv_label_create(close_btn);
//     lv_label_set_text(close_label, LV_SYMBOL_CLOSE);
//     lv_obj_center(close_label);
    
//     lv_obj_add_event_cb(close_btn, tableCloseCallback, LV_EVENT_CLICKED, this);

//     table_visible = true;
// }

// void EISPlot::showNoDataTable(const char* message) {
//     if (data_table) lv_obj_del(data_table);
    
//     data_table = lv_table_create(lv_screen_active());
//     lv_obj_set_size(data_table, 350, 200);
//     lv_obj_center(data_table);
//     lv_table_set_col_cnt(data_table, 1);
//     lv_table_set_row_cnt(data_table, 2);
    
//     lv_obj_set_style_text_font(data_table, &lv_font_montserrat_14, 0);
    
//     // Header
//     lv_table_set_cell_value(data_table, 0, 0, "Data Status");
    
//     // Message
//     lv_table_set_cell_value(data_table, 1, 0, message);
    
//     // Style the message cell
//     lv_obj_set_style_text_align(data_table, LV_TEXT_ALIGN_CENTER, LV_PART_ITEMS);
    
//     // Add close button
//     lv_obj_t* close_btn = lv_btn_create(data_table);
//     lv_obj_set_size(close_btn, 60, 30);
//     lv_obj_align(close_btn, LV_ALIGN_TOP_RIGHT, -5, 5);
//     lv_obj_set_style_bg_color(close_btn, lv_palette_main(LV_PALETTE_RED), 0);
    
//     lv_obj_t* close_label = lv_label_create(close_btn);
//     lv_label_set_text(close_label, LV_SYMBOL_CLOSE);
//     lv_obj_center(close_label);
    
//     lv_obj_add_event_cb(close_btn, tableCloseCallback, LV_EVENT_CLICKED, this);
    
//     table_visible = true;
// }

// Add to EISPlot.cpp - Mode Management Implementation
void EISPlot::setCurrentMode(int mode) {
    // Save current data if switching away from mode 5 or 6
    if (current_measurement_mode == MODE_1KHZ_LIVE && total_1khz_samples > 0) {
        saveMode5Data();
    } else if (current_measurement_mode == MODE_1KHZ_AVG && total_1khz_samples > 0) {
        saveMode6Data();
    }
    
    current_measurement_mode = mode;
    updateTableButtonVisibility();
    
    // Clear current data if switching to modes 1-4
    if (mode >= MODE_SINGLE_SCAN && mode <= MODE_AVERAGE_LOOP) {
        // Clear 1kHz data when switching to frequency sweep modes
        total_1khz_samples = 0;
        for (int i = 0; i < MAX_1KHZ_SAMPLES; i++) {
            all_1khz_real[i] = 0.0;
            all_1khz_imag[i] = 0.0;
        }
        time_plot_count = 0;
    } else if (mode == MODE_1KHZ_LIVE) {
        // Restore Mode 5 data if available
        restoreMode5Data();
    } else if (mode == MODE_1KHZ_AVG) {
        // Restore Mode 6 data if available
        restoreMode6Data();
    }
}

// void EISPlot::updateTableButtonVisibility() {
//     if (!table_btn) return;
    
//     // Show table button only for modes 5 and 6
//     if (current_measurement_mode == MODE_1KHZ_LIVE || current_measurement_mode == MODE_1KHZ_AVG) {
//         lv_obj_clear_flag(table_btn, LV_OBJ_FLAG_HIDDEN);
//     } else {
//         lv_obj_add_flag(table_btn, LV_OBJ_FLAG_HIDDEN);
//     }
// }

void EISPlot::updateTableButtonVisibility() {
    if (!table_btn) return;
    
    // Show table button only for modes 5 and 6
    if (current_measurement_mode == MODE_1KHZ_LIVE || current_measurement_mode == MODE_1KHZ_AVG) {
        lv_obj_clear_flag(table_btn, LV_OBJ_FLAG_HIDDEN);
        Serial.printf("Table button shown for mode %d\n", current_measurement_mode);
    } else {
        lv_obj_add_flag(table_btn, LV_OBJ_FLAG_HIDDEN);
        Serial.printf("Table button hidden for mode %d\n", current_measurement_mode);
    }
}

void EISPlot::saveMode5Data() {
    if (total_1khz_samples == 0) return;
    
    mode5_sample_count = total_1khz_samples;
    for (int i = 0; i < total_1khz_samples && i < MAX_1KHZ_SAMPLES; i++) {
        mode5_real_data[i] = all_1khz_real[i];
        mode5_imag_data[i] = all_1khz_imag[i];
    }
    
    Serial.printf("Saved Mode 5 data: %d samples\n", mode5_sample_count);
}

void EISPlot::saveMode6Data() {
    if (total_1khz_samples == 0) return;
    
    mode6_sample_count = total_1khz_samples;
    for (int i = 0; i < total_1khz_samples && i < MAX_1KHZ_SAMPLES; i++) {
        mode6_real_data[i] = all_1khz_real[i];
        mode6_imag_data[i] = all_1khz_imag[i];
    }
    
    Serial.printf("Saved Mode 6 data: %d samples\n", mode6_sample_count);
}

void EISPlot::restoreMode5Data() {
    if (mode5_sample_count == 0) return;
    
    total_1khz_samples = mode5_sample_count;
    for (int i = 0; i < mode5_sample_count && i < MAX_1KHZ_SAMPLES; i++) {
        all_1khz_real[i] = mode5_real_data[i];
        all_1khz_imag[i] = mode5_imag_data[i];
    }
    
    Serial.printf("Restored Mode 5 data: %d samples\n", total_1khz_samples);
}

void EISPlot::restoreMode6Data() {
    if (mode6_sample_count == 0) return;
    
    total_1khz_samples = mode6_sample_count;
    for (int i = 0; i < mode6_sample_count && i < MAX_1KHZ_SAMPLES; i++) {
        all_1khz_real[i] = mode6_real_data[i];
        all_1khz_imag[i] = mode6_imag_data[i];
    }
    
    Serial.printf("Restored Mode 6 data: %d samples\n", total_1khz_samples);
}

void EISPlot::clearModeData(int mode) {
    if (mode == MODE_1KHZ_LIVE) {
        mode5_sample_count = 0;
        for (int i = 0; i < MAX_1KHZ_SAMPLES; i++) {
            mode5_real_data[i] = 0.0;
            mode5_imag_data[i] = 0.0;
        }
        Serial.println("Cleared Mode 5 data");
    } else if (mode == MODE_1KHZ_AVG) {
        mode6_sample_count = 0;
        for (int i = 0; i < MAX_1KHZ_SAMPLES; i++) {
            mode6_real_data[i] = 0.0;
            mode6_imag_data[i] = 0.0;
        }
        Serial.println("Cleared Mode 6 data");
    }
}

void EISPlot::switchToTimePlot() {
    if (!time_plot_mode) {
        time_plot_mode = true;
        time_plot_count = 0;
        
        // Allocate time arrays if needed
        if (!time_real_values) {
            time_real_values = new double[max_time_samples];
            time_imag_values = new double[max_time_samples];
            for (int i = 0; i < max_time_samples; i++) {
                time_real_values[i] = 0.0;
                time_imag_values[i] = 0.0;
            }
        }
        
        // Hide Nyquist chart, show time chart
        lv_obj_add_flag(nyquist_chart, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(time_chart, LV_OBJ_FLAG_HIDDEN);
        chart = time_chart;
        
        // Update title - keep same labels since it's still Real vs Imaginary
        //lv_label_set_text(title, "1kHz Live Impedance (Real vs Imaginary)");
        
        // Clear time series data
        lv_chart_set_all_value(time_chart, time_series_real, LV_CHART_POINT_NONE);
    }
}

void EISPlot::showNyquistPlot() {
    if (time_plot_mode) {
        time_plot_mode = false;
        
        // Hide time chart, show Nyquist chart
        lv_obj_add_flag(time_chart, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(nyquist_chart, LV_OBJ_FLAG_HIDDEN);
        chart = nyquist_chart;
        
        // Update title and labels
        lv_label_set_text(title, "EIS Nyquist Plot (Real vs Imaginary)");
    }
}

void EISPlot::addTimePoint(double real_val, double imag_val) {
    if (!time_plot_mode) {
        switchToTimePlot();
    }
    
    if (time_plot_count < max_time_samples) {
        // Store values
        time_real_values[time_plot_count] = real_val;
        time_imag_values[time_plot_count] = imag_val;
        time_plot_count++;
    } else {
        // Shift data if we've reached max samples (circular buffer)
        for (int i = 0; i < max_time_samples - 1; i++) {
            time_real_values[i] = time_real_values[i + 1];
            time_imag_values[i] = time_imag_values[i + 1];
        }
        time_real_values[max_time_samples - 1] = real_val;
        time_imag_values[max_time_samples - 1] = imag_val;
    }
    
    // NEW: Store ALL samples in permanent 1kHz array (for complete data table)
    if (total_1khz_samples < MAX_1KHZ_SAMPLES) {
        all_1khz_real[total_1khz_samples] = real_val;
        all_1khz_imag[total_1khz_samples] = imag_val;
        total_1khz_samples++;
    }

    // Auto-scale with tight scaling for small variations
    if (auto_scale_enabled) {
        autoScaleTimePlot();
    }
    
    // Update chart data using external arrays
    fillTimeChartArrays();
    
    // Update value label
    if (value_label) {
        lv_label_set_text_fmt(value_label, 
                             "Sample %d: \nReal=%.2f m%s\nImag=%.2f m%s", 
                             time_plot_count, real_val, OHM_SYMBOL, imag_val, OHM_SYMBOL);
    }
    
    lv_chart_refresh(time_chart);
}

bool EISPlot::isValidVoltage(double voltage) {
    // Filter out invalid voltage readings (typically around 1.2V or negative)
    return (voltage > invalid_voltage_threshold && voltage < 10.0);  // Reasonable battery voltage range
}

double EISPlot::roundToNiceNumber(double value, bool round_up) {
    double magnitude = pow(10, floor(log10(abs(value))));
    double normalized = value / magnitude;
    
    double nice_normalized;
    if (round_up) {
        if (normalized <= 1) nice_normalized = 1;
        else if (normalized <= 2) nice_normalized = 2;
        else if (normalized <= 5) nice_normalized = 5;
        else nice_normalized = 10;
    } else {
        if (normalized >= 10) nice_normalized = 10;
        else if (normalized >= 5) nice_normalized = 5;
        else if (normalized >= 2) nice_normalized = 2;
        else nice_normalized = 1;
    }
    
    return nice_normalized * magnitude;
}

double EISPlot::roundToFineNumber(double value, bool round_up) {
    if (fabs(value) < 0.1) return round_up ? 0.1 : 0.0; // Handle very small values
    
    double magnitude = pow(10, floor(log10(fabs(value))));
    double normalized = value / magnitude;
    
    double fine_normalized;
    if (round_up) {
        if (normalized <= 0.5) fine_normalized = 0.5;
        else if (normalized <= 1) fine_normalized = 1;
        else if (normalized <= 2) fine_normalized = 2;
        else if (normalized <= 5) fine_normalized = 5;
        else fine_normalized = 10;
    } else {
        if (normalized >= 10) fine_normalized = 10;
        else if (normalized >= 5) fine_normalized = 5;
        else if (normalized >= 2) fine_normalized = 2;
        else if (normalized >= 1) fine_normalized = 1;
        else fine_normalized = 0.5;
    }
    
    return fine_normalized * magnitude;
}

int EISPlot::calculateFineDivisions(double range) {
    if (range <= 1) return 10;      // Very fine divisions for small ranges
    else if (range <= 5) return 10;
    else if (range <= 10) return 8;
    else if (range <= 50) return 8;
    else return 6;                  // Fewer divisions for larger ranges
}

void EISPlot::calculateBetterSpacing(double min_val, double max_val, double& range_min, double& range_max, int& divisions) {
    if (min_val == max_val) {
        // Handle edge case
        range_min = min_val - min_range_span/2;
        range_max = max_val + min_range_span/2;
        divisions = 6;
        return;
    }
    
    double data_range = max_val - min_val;
    
    // Ensure minimum range for good spacing (especially for close points)
    if (data_range < min_range_span) {
        double center = (min_val + max_val) / 2;
        double expand = (min_range_span - data_range) / 2;
        min_val = center - data_range/2 - expand;
        max_val = center + data_range/2 + expand;
        data_range = min_range_span;
    }
    
    // Add 20% padding for better visualization (more than default 10%)
    double padding = data_range * 0.2;
    double padded_min = min_val - padding;
    double padded_max = max_val + padding;
    
    // Round to nice numbers
    range_min = roundToNiceNumber(padded_min, false);
    range_max = roundToNiceNumber(padded_max, true);
    
    // Calculate optimal number of divisions (6-12 for better precision)
    double range = range_max - range_min;
    double step = roundToNiceNumber(range / 8, true);
    divisions = (int)(range / step);
    
    // Ensure reasonable number of divisions
    if (divisions < 6) divisions = 6;
    if (divisions > 12) divisions = 12;
    
    Serial.printf("Better spacing: data_range=%.2f, final_range=%.2f, divisions=%d\n", 
                  max_val - min_val, range_max - range_min, divisions);
}

void EISPlot::calculateOptimalRange(double min_val, double max_val, double& range_min, double& range_max, int& divisions) {
    calculateBetterSpacing(min_val, max_val, range_min, range_max, divisions);
}

void EISPlot::autoScale() {
    if (!auto_scale_enabled) return;
    
    // Different auto-scaling logic for time plots vs Nyquist plots
    if (time_plot_mode) {
        autoScaleTimePlot();
    } else {
        autoScaleNyquistPlot();
    }
}

void EISPlot::autoScaleNyquistPlot() {
    if (point_count == 0) return;
    
    // Find min/max values from valid data points
    double real_min_data = 1e6, real_max_data = -1e6;
    double imag_min_data = 1e6, imag_max_data = -1e6;
    bool has_data = false;
    
    for (int i = 0; i < point_count; i++) {
        if (data_valid[i]) {
            has_data = true;
            if (real_values[i] < real_min_data) real_min_data = real_values[i];
            if (real_values[i] > real_max_data) real_max_data = real_values[i];
            if (imag_values[i] < imag_min_data) imag_min_data = imag_values[i];
            if (imag_values[i] > imag_max_data) imag_max_data = imag_values[i];
        }
    }
    
    if (!has_data) return;
    
    Serial.printf("Nyquist data range: Real [%.2f, %.2f], Imag [%.2f, %.2f]\n", 
                  real_min_data, real_max_data, imag_min_data, imag_max_data);

    // X-axis (Real) - Always start from 0 for Nyquist plots
    real_min = 0;  // Fixed to 0
    
    // Calculate real_max with padding and rounding
    double real_padding = real_max_data * 0.1;  // 10% padding
    double padded_real_max = real_max_data + real_padding;
    real_max = roundToNiceNumber(padded_real_max, true);
    
    // Ensure minimum range
    if (real_max < 100) real_max = 100;
    
    // Y-axis (Imaginary) - Calculate optimal ranges
    int x_divisions, y_divisions;
    calculateOptimalRange(imag_min_data, imag_max_data, imag_min, imag_max, y_divisions);
    
    // Calculate X divisions for 0-to-max range
    double x_range = real_max - real_min;
    double x_step = roundToNiceNumber(x_range / 8, true);
    x_divisions = (int)(x_range / x_step);
    if (x_divisions < 4) x_divisions = 8;
    if (x_divisions > 10) x_divisions = 10;
    
    // Update chart ranges and grid
    updateChartRanges();
    lv_chart_set_div_line_count(chart, x_divisions, y_divisions);
    
    Serial.printf("Nyquist auto-scale: Real [%.1f, %.1f], Imag [%.1f, %.1f]\n", 
                  real_min, real_max, imag_min, imag_max);
}

void EISPlot::autoScaleTimePlot() {
    if (time_plot_count == 0) return;
    
    // Find min/max from time data for tight scaling
    double real_min_data = 1e6, real_max_data = -1e6;
    double imag_min_data = 1e6, imag_max_data = -1e6;
    bool has_data = false;
    
    for (int i = 0; i < time_plot_count; i++) {
        has_data = true;
        if (time_real_values[i] < real_min_data) real_min_data = time_real_values[i];
        if (time_real_values[i] > real_max_data) real_max_data = time_real_values[i];
        if (time_imag_values[i] < imag_min_data) imag_min_data = time_imag_values[i];
        if (time_imag_values[i] > imag_max_data) imag_max_data = time_imag_values[i];
    }
    
    if (!has_data) return;
    
    // For 1kHz mode, we want very tight scaling since variations are small
    double min_real_range = 5.0;  // Minimum 5 mΩ range for Real
    double min_imag_range = 2.0;  // Minimum 2 mΩ range for Imaginary
    double padding_percent = 0.05; // 5% padding (much less than 10%)
    
    // Calculate Real axis range with tight scaling
    double real_range = real_max_data - real_min_data;
    if (real_range < min_real_range) {
        double center = (real_min_data + real_max_data) / 2;
        real_min_data = center - min_real_range / 2;
        real_max_data = center + min_real_range / 2;
        real_range = min_real_range;
    }
    
    double real_padding = real_range * padding_percent;
    double time_real_min = real_min_data - real_padding;
    double time_real_max = real_max_data + real_padding;
    
    // Calculate Imaginary axis range with tight scaling
    double imag_range = imag_max_data - imag_min_data;
    if (imag_range < min_imag_range) {
        double center = (imag_min_data + imag_max_data) / 2;
        imag_min_data = center - min_imag_range / 2;
        imag_max_data = center + min_imag_range / 2;
        imag_range = min_imag_range;
    }
    
    double imag_padding = imag_range * padding_percent;
    double time_imag_min = imag_min_data - imag_padding;
    double time_imag_max = imag_max_data + imag_padding;
    
    // Round to fine numbers (smaller steps for better precision)
    time_real_min = roundToFineNumber(time_real_min, false);
    time_real_max = roundToFineNumber(time_real_max, true);
    time_imag_min = roundToFineNumber(time_imag_min, false);
    time_imag_max = roundToFineNumber(time_imag_max, true);
    
    // Set chart ranges
    lv_chart_set_range(time_chart, LV_CHART_AXIS_PRIMARY_X, (int)time_real_min, (int)time_real_max);
    lv_chart_set_range(time_chart, LV_CHART_AXIS_PRIMARY_Y, (int)time_imag_min, (int)time_imag_max);
    
    // Calculate fine divisions for small ranges
    double real_range_final = time_real_max - time_real_min;
    double imag_range_final = time_imag_max - time_imag_min;
    
    // More divisions for smaller ranges to show detail
    int x_divisions = calculateFineDivisions(real_range_final);
    int y_divisions = calculateFineDivisions(imag_range_final);
    
    lv_chart_set_div_line_count(time_chart, x_divisions, y_divisions);
    
    Serial.printf("Time plot fine-scale: Real [%.2f, %.2f], Imag [%.2f, %.2f] (x_div=%d, y_div=%d)\n", 
                  time_real_min, time_real_max, time_imag_min, time_imag_max, x_divisions, y_divisions);
}

void EISPlot::updateChartRanges() {
    if (chart) {
        lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_X, -200, (int)real_max);
        lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, (int)imag_min, (int)imag_max);
    }
}

void EISPlot::updateSinglePoint(int freq_index, double real_val, double imag_val) {
    // For time plot mode (modes 5-6), add to time chart
    if (time_plot_mode) {
        addTimePoint(real_val, imag_val);
        return;
    }
    
    // For Nyquist plot mode (modes 1-4)
    if (!chart || !series || freq_index < 0 || freq_index >= MAX_POINTS) return;
    
    // Allocate arrays if not already done
    if (!real_values) {
        real_values = new double[MAX_POINTS];
        imag_values = new double[MAX_POINTS];
        for (int i = 0; i < MAX_POINTS; i++) {
            real_values[i] = 0.0;
            imag_values[i] = 0.0;
            data_valid[i] = false;
        }
    }
    
    // Store the new data point
    real_values[freq_index] = real_val;
    imag_values[freq_index] = imag_val;
    data_valid[freq_index] = true;
    
    // Update point count to include this index
    if (freq_index >= point_count) {
        point_count = freq_index + 1;
    }

    // For single point updates, conservative auto-scaling
    static double last_real = 0, last_imag = 0;
    static int scale_counter = 0;
    
    bool should_scale = false;
    if (point_count == 1) {
        should_scale = true; // First point
    } else if (scale_counter++ % 10 == 0) {
        // Auto-scale every 10 updates to avoid constant scaling
        should_scale = true;
    } else if (abs(real_val - last_real) > (real_max - real_min) * 0.2 || 
               abs(imag_val - last_imag) > (imag_max - imag_min) * 0.2) {
        // Scale if values changed significantly (>20% of current range)
        should_scale = true;
    }
    
    if (should_scale && auto_scale_enabled) {
        autoScale();
    }
    
    last_real = real_val;
    last_imag = imag_val;
    
    // Update the chart arrays and refresh
    fillChartArrays();
    refreshChart();
    
    // Optional: Print debug info
    if (freq_index == 0) { // Only for 1kHz measurements
        static int debug_counter = 0;
        if (debug_counter++ % 20 == 0) { // Print every 20 updates
            Serial.printf("updateSinglePoint: Real=%.2f, Imag=%.2f, Points=%d\n", 
                         real_val, imag_val, point_count);
        }
    }
}

bool EISPlot::updateData(const double* real_data, const double* imag_data, int data_count) {
    if (!chart || !series || data_count <= 0) return false;
    
    // Switch to Nyquist plot if in time mode
    if (time_plot_mode) {
        showNyquistPlot();
    }
    
    Serial.println("EISPlot: Checking data for outliers and duplicates...");
    
    // Store data count
    point_count = (data_count > MAX_POINTS) ? MAX_POINTS : data_count;
    
    // Allocate or reallocate arrays
    if (real_values) delete[] real_values;
    if (imag_values) delete[] imag_values;
    if (voltages_at_measurement) delete[] voltages_at_measurement;
    real_values = new double[point_count];
    imag_values = new double[point_count];
    voltages_at_measurement = new double[point_count];
        
    // Copy data and mark as valid
    for (int i = 0; i < point_count; i++) {
        real_values[i] = real_data[i];
        imag_values[i] = imag_data[i];
        data_valid[i] = true;
    }

    // Auto-scale based on all data
    autoScale();
    
    // Sort points by frequency for proper line connection
    sortPointsByFrequency();

    // Mark measurement as complete
    measurement_completed = true;
    
    // Fill X/Y arrays and refresh
    fillChartArrays();
    refreshChart();

    // Update status to "Done"
    setStatusText("Done");
    return true;
}

void EISPlot::sortPointsByFrequency() {
    // Create frequency-to-index mapping for proper plotting order
    for (int i = 0; i < MAX_POINTS; i++) {
        freq_plot_order[i] = i;  // Default order matches frequency index
    }
}

void EISPlot::updateVoltageTemp(double voltage, double temperature) {
    // Always update if values have changed significantly
    bool voltage_changed = fabs(voltage - last_voltage_update) > 0.01;
    bool temp_changed = fabs(temperature - last_temp_update) > 0.1;
    
    if (voltage_changed || temp_changed) {
        if (isValidVoltage(voltage)) {
            last_valid_voltage = voltage;
            if (voltage_label) {
                lv_label_set_text_fmt(voltage_label, "V: %.3f V", voltage);
            }
            last_voltage_update = voltage;
        } else {
            // Use last valid voltage or show invalid
            if (voltage_label) {
                if (last_valid_voltage > 0) {
                    lv_label_set_text_fmt(voltage_label, "V: %.3f V*", last_valid_voltage);  // * indicates last valid
                } else {
                    lv_label_set_text(voltage_label, "V: -- ");
                }
            }
        }
        
        if (temp_label && temp_changed) {
            lv_label_set_text_fmt(temp_label, "T: %.1f °C", temperature);
            last_temp_update = temperature;
        }
    }
}

void EISPlot::setDataRange(double real_min_val, double real_max_val, double imag_min_val, double imag_max_val) {
    real_min = real_min_val;
    real_max = real_max_val;
    imag_min = imag_min_val;
    imag_max = imag_max_val;
    
    auto_scale_enabled = false;  // Disable auto-scaling when manually set
    updateChartRanges();
}

void EISPlot::setStatusText(const char* text) {
    if (status_label) {
        lv_label_set_text(status_label, text);
        
        // Change color based on status
        if (strcmp(text, "Processing...") == 0 || strcmp(text, "Measuring...") == 0) {
            lv_obj_set_style_text_color(status_label, lv_color_hex(0xFFFF00), 0);  // Yellow
        } else if (strcmp(text, "Done") == 0 || strcmp(text, "Complete") == 0) {
            lv_obj_set_style_text_color(status_label, lv_color_hex(0x00FF00), 0);  // Green
        } else if (strcmp(text, "Duplicates!") == 0) {
            lv_obj_set_style_text_color(status_label, lv_color_hex(0xFF8800), 0);  // Orange
        } else if (strcmp(text, "Error") == 0 || strcmp(text, "Failed") == 0) {
            lv_obj_set_style_text_color(status_label, lv_color_hex(0xFF4444), 0);  // Red
        } else {
            lv_obj_set_style_text_color(status_label, lv_color_hex(0xFFFFFF), 0);  // White
        }
    }
}

void EISPlot::clearData() {
    // Clear all data points
    for (int i = 0; i < MAX_POINTS; i++) {
        x_points[i] = 0;
        y_points[i] = 0;
        data_valid[i] = false;
    }
    point_count = 0;
    measurement_completed = false;
    auto_scale_enabled = true;

    // Reset to default ranges
    real_min = 700; 
    real_max = 1300;
    imag_min = 100; 
    imag_max = 120;
    updateChartRanges();
    
    // Clear time plot data
    time_plot_count = 0;
    if (time_real_values) {
        for (int i = 0; i < max_time_samples; i++) {
            time_real_values[i] = 0.0;
            time_imag_values[i] = 0.0;
        }
    }
    
    total_1khz_samples = 0;

    // NEW: Clear 1kHz permanent storage
    for (int i = 0; i < MAX_1KHZ_SAMPLES; i++) {
        all_1khz_real[i] = 0.0;
        all_1khz_imag[i] = 0.0;
    }

    // Switch back to Nyquist plot if in time mode
    if (time_plot_mode) {
        showNyquistPlot();
    }

    if (chart && series) {
        refreshChart();
    }

    setStatusText("Ready");
}

void EISPlot::resetMeasurementState() {
    clearData();
    if (value_label) {
        lv_label_set_text(value_label, "Starting new measurement...");
    }
    setStatusText("Processing...");
}

void EISPlot::setMeasurementCallback(void (*callback)(void)) {
    measurement_callback = callback;
}

void EISPlot::fillChartArrays() {
    if (!real_values || !imag_values) return;

    int plot_points = 0;
    for (int i = 0; i < point_count; i++) {
        if (data_valid[i]) {
            // Clamp to declared ranges to avoid surprises
            double rx = real_values[i];
            double iy = imag_values[i];
            if (rx < real_min) rx = real_min;
            if (rx > real_max) rx = real_max;
            if (iy < imag_min) iy = imag_min;
            if (iy > imag_max) iy = imag_max;
            
            x_points[plot_points] = (lv_coord_t)lrint(rx);
            y_points[plot_points] = (lv_coord_t)lrint(iy);
            plot_points++;
        }
    }

    // Fill remaining points with the last valid point to avoid artifacts
    for (int i = plot_points; i < MAX_POINTS; i++) {
        if (plot_points > 0) {
            x_points[i] = x_points[plot_points - 1];
            y_points[i] = y_points[plot_points - 1];
        } else {
            x_points[i] = (lv_coord_t)real_min;
            y_points[i] = (lv_coord_t)imag_min;
        }
    }
}

void EISPlot::fillTimeChartArrays() {
    if (!time_real_values || !time_imag_values) return;

    int n = time_plot_count;
    if (n > max_time_samples) n = max_time_samples;   // safety
    
    // Fill arrays with time data points (Real as X, Imaginary as Y)
    for (int i = 0; i < time_plot_count; i++) {
        time_x_points[i] = (lv_coord_t)lrint(time_real_values[i]);
        time_y_points[i] = (lv_coord_t)lrint(time_imag_values[i]);
    }
    
    // Fill remaining points with last valid point to avoid artifacts
    for (int i = time_plot_count; i < max_time_samples; i++) {
        if (time_plot_count > 0) {
            time_x_points[i] = time_x_points[time_plot_count - 1];
            time_y_points[i] = time_y_points[time_plot_count - 1];
        } else {
            x_points[i] = 0;
            y_points[i] = 0;
        }
    }
    
    // Set external arrays
    lv_chart_set_ext_x_array(time_chart, time_series_real, time_x_points);
    lv_chart_set_ext_y_array(time_chart, time_series_real, time_y_points);
    lv_chart_set_point_count(time_chart, time_plot_count > 0 ? time_plot_count : 1);
}

void EISPlot::refreshChart() {
    if (!chart || !series) return;
    
    // Count valid points
    int valid_points = 0;
    for (int i = 0; i < point_count; i++) {
        if (data_valid[i]) valid_points++;
    }
    
    // Set external arrays and refresh
    lv_chart_set_ext_x_array(chart, series, x_points);
    lv_chart_set_ext_y_array(chart, series, y_points);
    lv_chart_set_point_count(chart, valid_points > 0 ? valid_points : 1);
    lv_chart_refresh(chart);
}

void EISPlot::chartEventCallback(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* obj = lv_event_get_target_obj(e);
    
    if (!instance) return;
    
    if (code == LV_EVENT_VALUE_CHANGED || code == LV_EVENT_PRESSED) {
        int32_t point_id = lv_chart_get_pressed_point(obj);
        
        if (point_id != LV_CHART_POINT_NONE && point_id < instance->point_count) {
            // Find the actual data point (considering only valid points)
            int actual_index = -1;
            int valid_count = 0;
            
            for (int i = 0; i < instance->point_count; i++) {
                if (instance->data_valid[i]) {
                    if (valid_count == point_id) {
                        actual_index = i;
                        break;
                    }
                    valid_count++;
                }
            }
            
            if (actual_index >= 0) {
                // Set cursor to the clicked point
                lv_chart_set_cursor_point(obj, instance->cursor, instance->series, point_id);

                static const double FREQUENCIES[19] = {
                    1007, 4943, 3906, 3082, 2471, 1953, 1541, 1220, 977,
                    770, 610, 488, 385, 305, 242, 192, 152, 122, 97
                };

                // Update value display with Omega symbol
                if (instance->value_label && instance->real_values && instance->imag_values) {
                    double real_val = instance->real_values[actual_index];
                    double imag_val = instance->imag_values[actual_index];
                    double frequency = (actual_index < 19) ? FREQUENCIES[actual_index] : 0;
                    
                    lv_label_set_text_fmt(instance->value_label, 
                                         "Point %d: %.0f Hz\nReal = %.2f m%s\nImag = %.2f m%s", 
                                         actual_index, frequency, real_val, EISPlot::OHM_SYMBOL,
                                         imag_val, EISPlot::OHM_SYMBOL);
                }
            }
        }
    }
}

void EISPlot::measurementButtonCallback(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_CLICKED && instance) {
        Serial.println("Measurement button pressed");
        instance->resetMeasurementState();
        
        // Call the external measurement callback if set
        if (measurement_callback) {
            measurement_callback();
        }
    }
}

void EISPlot::tableButtonEventCallback(lv_event_t* e) {
    EISPlot* self = static_cast<EISPlot*>(lv_event_get_user_data(e));
    if (!self) return;

    if (self->table_visible) {
        // Close if already open
        self->hideTablePopup();
    } else {
        // Show appropriate table based on current mode
        if (self->current_measurement_mode == MODE_1KHZ_LIVE) {
            // Mode 5: Show all recorded 1kHz live samples
            if (self->mode5_sample_count > 0 || self->total_1khz_samples > 0) {
                self->show1kHzDataTable();
            } else {
                self->showNoDataTable("Mode 5: No 1kHz live data recorded yet.\nStart a measurement to collect data.");
            }
        } else if (self->current_measurement_mode == MODE_1KHZ_AVG) {
            // Mode 6: Show all averaged samples  
            if (self->mode6_sample_count > 0 || self->total_1khz_samples > 0) {
                self->show1kHzDataTable();
            } else {
                self->showNoDataTable("Mode 6: No 1kHz averaged data recorded yet.\nStart a measurement to collect data.");
            }
        } else {
            // Safety fallback - should not happen since button is hidden
            self->showNoDataTable("Table not available for this mode.\nSwitch to Mode 5 or Mode 6.");
        }
    }
}