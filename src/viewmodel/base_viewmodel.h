/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "subjects.h"
#include "base_model.h"
#include "discover_scanner.h"
#include "spectrum_scanner.h"

#include "lvgl.h"

#include <array>
#include <cstdint>
#include <memory>

namespace viewmodel {

class BaseViewModel {
public:
    BaseViewModel();
    ~BaseViewModel();

    BaseViewModel(const BaseViewModel&) = delete;
    BaseViewModel& operator=(const BaseViewModel&) = delete;

    lv_subject_t* title_subject();
    lv_subject_t* dark_mode_subject();
    lv_subject_t* current_page_subject();
    lv_subject_t* quit_requested_subject();
    lv_subject_t* carousel_selection_subject();
    lv_subject_t* settings_focus_subject();
    lv_subject_t* node_name_subject();
    lv_subject_t* region_subject();
    lv_subject_t* freq_hz_subject();
    lv_subject_t* sf_subject();
    lv_subject_t* bw_index_subject();
    lv_subject_t* cr_subject();
    lv_subject_t* tx_power_subject();
    lv_subject_t* sync_word_subject();
    lv_subject_t* preamble_subject();
    lv_subject_t* settings_edit_active_subject();
    lv_subject_t* node_edit_active_subject();
    lv_subject_t* node_edit_cursor_subject();
    lv_subject_t* lora_ok_subject();
    lv_subject_t* lora_error_subject();
    lv_subject_t* discover_summary_subject();
    lv_subject_t* discover_list_subject();
    lv_subject_t* discover_scanning_subject();
    lv_subject_t* discover_scroll_subject();
    lv_subject_t* spectrum_peak_subject();
    lv_subject_t* spectrum_floor_subject();
    lv_subject_t* spectrum_channel_subject();
    lv_subject_t* spectrum_busy_subject();
    lv_subject_t* spectrum_view_subject();
    lv_subject_t* spectrum_window_subject();
    lv_subject_t* spectrum_status_subject();

    bool is_dark_mode() const;
    void set_dark_mode(bool enabled);
    void toggle_dark_mode();

    model::MeshConfig mesh_config() const;

    model::AppPage current_page() const;
    void set_current_page(model::AppPage page);
    void show_home();
    void show_settings();
    void show_discover();
    void show_spectrum();
    void show_lora_error();

    int carousel_selection() const;
    void move_carousel(int delta);
    void activate_selection();

    void move_settings_focus(int delta);
    bool settings_edit_active() const;
    void begin_settings_edit();
    void commit_settings_edit();
    void cancel_settings_edit();
    void adjust_focused_value(int delta);

    void toggle_node_edit();
    void cancel_node_edit();
    void move_node_cursor(int delta);
    void cycle_node_char(int delta);
    void insert_node_char(char character);
    void backspace_node_char();

    void set_mesh_config(const model::MeshConfig& config);
    void set_lora_status(bool ok, const char* error);

    // Discover page actions and polling.
    void poll_discover();
    void toggle_discover_scan();
    void scroll_discover(int delta);

    // Spectrum page actions and polling.
    void poll_spectrum();
    void toggle_spectrum_view();
    void cycle_spectrum_window(int delta);
    const platform::SpectrumSnapshot& spectrum_snapshot() const;

    void shutdown_radio();
    void request_quit();

private:
    void publish_all();
    void publish_mesh();
    void publish_discover(bool force);
    void publish_spectrum(bool force);
    void start_discover();
    void start_spectrum();
    void stop_feature_sessions();
    bool ensure_radio(std::string& error);
    static const char* page_title(model::AppPage page);
    static constexpr int kDiscoverVisibleRows = 5;

    model::BaseModel model_;
    reactive::StringSubject<32> title_subject_;
    reactive::BoolSubject dark_mode_subject_;
    reactive::IntSubject current_page_subject_;
    reactive::BoolSubject quit_requested_subject_;
    reactive::IntSubject carousel_selection_subject_;
    reactive::IntSubject settings_focus_subject_;
    reactive::StringSubject<model::kNodeNameBuffer> node_name_subject_;
    reactive::IntSubject region_subject_;
    reactive::IntSubject freq_hz_subject_;
    reactive::IntSubject sf_subject_;
    reactive::IntSubject bw_index_subject_;
    reactive::IntSubject cr_subject_;
    reactive::IntSubject tx_power_subject_;
    reactive::IntSubject sync_word_subject_;
    reactive::IntSubject preamble_subject_;
    reactive::BoolSubject settings_edit_active_subject_;
    reactive::BoolSubject node_edit_active_subject_;
    reactive::IntSubject node_edit_cursor_subject_;
    reactive::BoolSubject lora_ok_subject_;
    reactive::StringSubject<96> lora_error_subject_;
    reactive::StringSubject<320> discover_summary_subject_;
    reactive::StringSubject<1800> discover_list_subject_;
    reactive::BoolSubject discover_scanning_subject_;
    reactive::IntSubject discover_scroll_subject_;
    reactive::IntSubject spectrum_peak_subject_;
    reactive::IntSubject spectrum_floor_subject_;
    reactive::IntSubject spectrum_channel_subject_;
    reactive::IntSubject spectrum_busy_subject_;
    reactive::IntSubject spectrum_view_subject_;
    reactive::IntSubject spectrum_window_subject_;
    reactive::StringSubject<128> spectrum_status_subject_;

    std::array<char, model::kNodeNameBuffer> node_name_before_edit_{};
    model::MeshConfig mesh_before_edit_{};

    std::unique_ptr<platform::MeshRadio> radio_;
    std::unique_ptr<platform::DiscoverScanner> discover_;
    std::unique_ptr<platform::SpectrumScanner> spectrum_;
    platform::SpectrumSnapshot spectrum_snapshot_{};

    int discover_scroll_{0};
    uint32_t discover_published_revision_{0};
    uint32_t spectrum_published_revision_{0};
    uint32_t last_discover_publish_ms_{0};
};

} // namespace viewmodel
