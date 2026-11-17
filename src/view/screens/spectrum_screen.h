/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_screen.h"
#include "spectrum_scanner.h"

#include "lvgl.h"

#include <array>
#include <cstdint>

namespace screen {

// Handheld swept-RSSI spectrum analyzer: four stat tiles, a 160-bin trace, a
// colour waterfall and a frequency axis. The plot and waterfall are 1:1
// canvases so a redraw is a memory write, not an LVGL chart rebuild.
class SpectrumScreen : public BaseScreen {
public:
    SpectrumScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);
    ~SpectrumScreen() override;

private:
    void build_content(lv_obj_t* content) override;

    void create_canvases(lv_obj_t* content);
    void draw_plot(const platform::SpectrumSnapshot& snapshot);
    void draw_waterfall(const platform::SpectrumSnapshot& snapshot);
    void draw_ramp(const platform::SpectrumSnapshot& snapshot);
    void update_overlays(const platform::SpectrumSnapshot& snapshot);
    void apply_theme();
    void force_redraw();

    static void timer_cb(lv_timer_t* timer);
    static void status_changed(lv_observer_t* observer, lv_subject_t* subject);
    static void theme_changed(lv_observer_t* observer, lv_subject_t* subject);

    lv_obj_t* stats_bar_{nullptr};
    lv_obj_t* stat_titles_[4]{};
    lv_obj_t* stat_values_[4]{};
    lv_obj_t* stat_separators_[3]{};
    lv_obj_t* plot_frame_{nullptr};
    lv_obj_t* plot_canvas_{nullptr};
    lv_draw_buf_t* plot_buf_{nullptr};
    lv_obj_t* waterfall_frame_{nullptr};
    lv_obj_t* ramp_frame_{nullptr};
    lv_obj_t* waterfall_canvas_{nullptr};
    lv_draw_buf_t* waterfall_buf_{nullptr};
    lv_obj_t* ramp_canvas_{nullptr};
    lv_draw_buf_t* ramp_buf_{nullptr};
    lv_obj_t* grid_labels_[3]{};
    lv_obj_t* peak_tag_{nullptr};
    lv_obj_t* status_{nullptr};
    lv_obj_t* axis_start_{nullptr};
    lv_obj_t* axis_center_{nullptr};
    lv_obj_t* axis_end_{nullptr};
    lv_timer_t* timer_{nullptr};
    const lv_font_t* small_font_{nullptr};

    std::array<lv_color_t, 64> heat_lut_{};
    uint32_t last_plot_revision_{UINT32_MAX};
    uint32_t last_waterfall_revision_{UINT32_MAX};
};

} // namespace screen
