/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "subjects.h"
#include "base_model.h"

#include "lvgl.h"

#include <array>

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
    lv_subject_t* node_edit_active_subject();
    lv_subject_t* node_edit_cursor_subject();
    lv_subject_t* lora_ok_subject();
    lv_subject_t* lora_error_subject();

    bool is_dark_mode() const;
    void set_dark_mode(bool enabled);
    void toggle_dark_mode();

    model::MeshConfig mesh_config() const;

    model::AppPage current_page() const;
    void set_current_page(model::AppPage page);
    void show_home();
    void show_settings();
    void show_test1();
    void show_test2();
    void show_lora_error();

    int carousel_selection() const;
    void move_carousel(int delta);
    void activate_selection();

    void move_settings_focus(int delta);
    void adjust_focused_value(int delta);

    void toggle_node_edit();
    void cancel_node_edit();
    void move_node_cursor(int delta);
    void cycle_node_char(int delta);
    void backspace_node_char();

    void set_mesh_config(const model::MeshConfig& config);
    void set_lora_status(bool ok, const char* error);

    void request_quit();

private:
    void publish_all();
    void publish_mesh();

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
    reactive::BoolSubject node_edit_active_subject_;
    reactive::IntSubject node_edit_cursor_subject_;
    reactive::BoolSubject lora_ok_subject_;
    reactive::StringSubject<96> lora_error_subject_;

    std::array<char, model::kNodeNameBuffer> node_name_before_edit_{};
};

} // namespace viewmodel
