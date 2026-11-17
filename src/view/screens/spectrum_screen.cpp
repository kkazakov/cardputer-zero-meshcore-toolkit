/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "spectrum_screen.h"

#include "asset_manager.h"
#include "base_viewmodel.h"
#include "bindings.h"
#include "theme.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace screen {
namespace {

constexpr int kStatsX = 2;
constexpr int kStatsY = 1;
constexpr int kStatsW = 316;
constexpr int kStatsH = 16;
constexpr int kStatCellW = 79;

constexpr int kPlotFrameX = 30;
constexpr int kPlotFrameY = 19;
constexpr int kPlotFrameW = 288;
constexpr int kPlotFrameH = 46;
constexpr int kPlotCanvasW = 284;
constexpr int kPlotCanvasH = 42;

constexpr int kWfFrameX = 30;
constexpr int kWfFrameY = 67;
constexpr int kWfFrameW = 288;
constexpr int kWfFrameH = 30;
constexpr int kWfCanvasW = 284;
constexpr int kWfCanvasH = 26;

constexpr int kAxisY = 98;

uint32_t pixel_value(lv_color_t color) {
    return lv_color_to_u32(color) | 0xFF000000U;
}

lv_color_t mix_color(lv_color_t a, lv_color_t b, int amount) {
    amount = std::max(0, std::min(255, amount));
    const uint32_t ca = lv_color_to_u32(a);
    const uint32_t cb = lv_color_to_u32(b);
    const int ar = static_cast<int>((ca >> 16U) & 0xFFU);
    const int ag = static_cast<int>((ca >> 8U) & 0xFFU);
    const int ab = static_cast<int>(ca & 0xFFU);
    const int br = static_cast<int>((cb >> 16U) & 0xFFU);
    const int bg = static_cast<int>((cb >> 8U) & 0xFFU);
    const int bb = static_cast<int>(cb & 0xFFU);
    return lv_color_make(static_cast<uint8_t>(ar + (br - ar) * amount / 255),
                         static_cast<uint8_t>(ag + (bg - ag) * amount / 255),
                         static_cast<uint8_t>(ab + (bb - ab) * amount / 255));
}

int column_max(const int16_t* bins, int bin_count, int x, int width) {
    if (!bins || bin_count <= 0) {
        return -130;
    }
    const int first = std::min(bin_count - 1, x * bin_count / std::max(1, width));
    int last = std::min(bin_count, (x + 1) * bin_count / std::max(1, width));
    if (last <= first) {
        last = first + 1;
    }
    int maximum = -200;
    for (int i = first; i < last; ++i) {
        maximum = std::max<int>(maximum, bins[i]);
    }
    return maximum;
}

void set_canvas_pixel(uint32_t* pixels, int stride, int x, int y, lv_color_t color) {
    pixels[y * stride + x] = pixel_value(color);
}

void fill_canvas(uint32_t* pixels, int stride, int width, int height, lv_color_t color) {
    const uint32_t value = pixel_value(color);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            pixels[y * stride + x] = value;
        }
    }
}

} // namespace

SpectrumScreen::SpectrumScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

SpectrumScreen::~SpectrumScreen() {
    if (timer_) {
        lv_timer_delete(timer_);
        timer_ = nullptr;
    }
    if (plot_canvas_) {
        lv_obj_delete(plot_canvas_);
        plot_canvas_ = nullptr;
    }
    if (waterfall_canvas_) {
        lv_obj_delete(waterfall_canvas_);
        waterfall_canvas_ = nullptr;
    }
    if (ramp_canvas_) {
        lv_obj_delete(ramp_canvas_);
        ramp_canvas_ = nullptr;
    }
    if (plot_buf_) {
        lv_draw_buf_destroy(plot_buf_);
        plot_buf_ = nullptr;
    }
    if (waterfall_buf_) {
        lv_draw_buf_destroy(waterfall_buf_);
        waterfall_buf_ = nullptr;
    }
    if (ramp_buf_) {
        lv_draw_buf_destroy(ramp_buf_);
        ramp_buf_ = nullptr;
    }
}

void SpectrumScreen::build_content(lv_obj_t* content) {
    const auto palette = view::palette(view_model().is_dark_mode());
    small_font_ = assets().load_standard_font(10);
    if (!small_font_) small_font_ = &lv_font_montserrat_10;
    const lv_font_t* value_font = assets().load_standard_font(11);
    if (!value_font) value_font = &lv_font_montserrat_12;

    static const char* const kStatTitles[4] = {"Peak", "Floor", "Channel", "Busy"};
    lv_subject_t* value_subjects[4] = {
        view_model().spectrum_peak_subject(),
        view_model().spectrum_floor_subject(),
        view_model().spectrum_channel_subject(),
        view_model().spectrum_busy_subject(),
    };
    static const char* const kValueFormats[4] = {"%d", "%d", "%d", "%d%%"};

    // One compact status strip instead of four tall cards. Label and value sit
    // on the same baseline so the two graphs below get the freed height.
    stats_bar_ = lv_obj_create(content);
    lv_obj_remove_style_all(stats_bar_);
    lv_obj_set_size(stats_bar_, kStatsW, kStatsH);
    lv_obj_set_pos(stats_bar_, kStatsX, kStatsY);
    lv_obj_set_style_radius(stats_bar_, 3, 0);
    lv_obj_set_style_bg_color(stats_bar_, palette.surface, 0);
    lv_obj_set_style_bg_opa(stats_bar_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(stats_bar_, 1, 0);
    lv_obj_set_style_border_color(stats_bar_, palette.border, 0);
    lv_obj_clear_flag(stats_bar_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(stats_bar_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(stats_bar_, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    for (int i = 0; i < 4; ++i) {
        const int cell_x = i * kStatCellW;
        stat_titles_[i] = lv_label_create(stats_bar_);
        lv_label_set_text(stat_titles_[i], kStatTitles[i]);
        lv_obj_set_style_text_font(stat_titles_[i], small_font_, 0);
        lv_obj_set_style_text_color(stat_titles_[i], palette.text, 0);
        lv_obj_set_style_text_opa(stat_titles_[i], LV_OPA_70, 0);
        lv_obj_set_pos(stat_titles_[i], cell_x + 4, 2);
        lv_obj_clear_flag(stat_titles_[i], LV_OBJ_FLAG_CLICKABLE);

        stat_values_[i] = lv_label_create(stats_bar_);
        lv_label_set_text(stat_values_[i], "--");
        lv_obj_set_width(stat_values_[i], 35);
        lv_obj_set_style_text_align(stat_values_[i], LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_style_text_font(stat_values_[i], value_font, 0);
        lv_obj_set_style_text_color(stat_values_[i], palette.text, 0);
        lv_obj_set_pos(stat_values_[i], cell_x + 40, 1);
        lv_obj_clear_flag(stat_values_[i], LV_OBJ_FLAG_CLICKABLE);
        lv_label_bind_text(stat_values_[i], value_subjects[i], kValueFormats[i]);
    }
    lv_obj_set_style_text_color(stat_values_[0], lv_color_hex(0xF5A623), 0);
    lv_obj_set_style_text_color(stat_values_[2], palette.primary, 0);

    for (int i = 0; i < 3; ++i) {
        stat_separators_[i] = lv_obj_create(stats_bar_);
        lv_obj_remove_style_all(stat_separators_[i]);
        lv_obj_set_size(stat_separators_[i], 1, 12);
        lv_obj_set_pos(stat_separators_[i], (i + 1) * kStatCellW, 2);
        lv_obj_set_style_bg_color(stat_separators_[i], palette.border, 0);
        lv_obj_set_style_bg_opa(stat_separators_[i], LV_OPA_COVER, 0);
        lv_obj_clear_flag(stat_separators_[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(stat_separators_[i], LV_OBJ_FLAG_CLICKABLE);
    }

    create_canvases(content);

    for (int i = 0; i < 3; ++i) {
        grid_labels_[i] = lv_label_create(content);
        lv_label_set_text(grid_labels_[i], "");
        lv_obj_set_width(grid_labels_[i], 27);
        lv_obj_set_style_text_align(grid_labels_[i], LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_style_text_font(grid_labels_[i], small_font_, 0);
        lv_obj_set_style_text_color(grid_labels_[i], palette.text, 0);
        lv_obj_set_style_text_opa(grid_labels_[i], LV_OPA_70, 0);
    }

    peak_tag_ = lv_label_create(content);
    lv_label_set_text(peak_tag_, "");
    lv_obj_set_style_text_font(peak_tag_, small_font_, 0);
    lv_obj_set_style_text_color(peak_tag_, lv_color_hex(0xF5A623), 0);
    lv_obj_add_flag(peak_tag_, LV_OBJ_FLAG_HIDDEN);

    status_ = lv_label_create(content);
    lv_label_set_text(status_, "");
    lv_obj_set_width(status_, 142);
    lv_obj_set_pos(status_, 172, 20);
    lv_obj_set_style_text_align(status_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(status_, small_font_, 0);
    lv_obj_set_style_text_color(status_, palette.warning, 0);
    lv_label_set_long_mode(status_, LV_LABEL_LONG_DOT);
    const char* initial_status = lv_subject_get_string(view_model().spectrum_status_subject());
    if (initial_status && initial_status[0] != '\0') {
        lv_label_set_text(status_, initial_status);
        lv_obj_remove_flag(status_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(status_, LV_OBJ_FLAG_HIDDEN);
    }

    axis_start_ = lv_label_create(content);
    lv_label_set_text(axis_start_, "");
    lv_obj_set_width(axis_start_, 100);
    lv_obj_set_pos(axis_start_, kPlotFrameX, kAxisY);
    lv_obj_set_style_text_font(axis_start_, small_font_, 0);
    lv_obj_set_style_text_color(axis_start_, palette.text, 0);
    lv_obj_set_style_text_opa(axis_start_, LV_OPA_70, 0);

    axis_center_ = lv_label_create(content);
    lv_label_set_text(axis_center_, "");
    lv_obj_set_width(axis_center_, 100);
    lv_obj_set_pos(axis_center_, 110, kAxisY);
    lv_obj_set_style_text_align(axis_center_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(axis_center_, small_font_, 0);
    lv_obj_set_style_text_color(axis_center_, palette.text, 0);
    lv_obj_set_style_text_opa(axis_center_, LV_OPA_70, 0);

    axis_end_ = lv_label_create(content);
    lv_label_set_text(axis_end_, "");
    lv_obj_set_width(axis_end_, 100);
    lv_obj_set_pos(axis_end_, 218, kAxisY);
    lv_obj_set_style_text_align(axis_end_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(axis_end_, small_font_, 0);
    lv_obj_set_style_text_color(axis_end_, palette.text, 0);
    lv_obj_set_style_text_opa(axis_end_, LV_OPA_70, 0);

    reactive::observe_obj(status_, view_model().spectrum_status_subject(), status_changed, this);
    reactive::observe_obj(root(), view_model().dark_mode_subject(), theme_changed, this);

    static const uint8_t kStops[6][3] = {
        {10, 12, 18}, {38, 22, 92}, {118, 30, 138},
        {204, 58, 92}, {246, 140, 38}, {252, 232, 128},
    };
    for (int i = 0; i < 64; ++i) {
        const int t = i * 5 * 256 / 63;
        int segment = t >> 8;
        if (segment > 4) segment = 4;
        const int fraction = t - (segment << 8);
        uint8_t color[3];
        for (int k = 0; k < 3; ++k) {
            color[k] = static_cast<uint8_t>(kStops[segment][k] +
                                            (kStops[segment + 1][k] - kStops[segment][k]) * fraction / 256);
        }
        heat_lut_[i] = lv_color_make(color[0], color[1], color[2]);
    }

    view_model().poll_spectrum();
    const auto& snapshot = view_model().spectrum_snapshot();
    last_plot_revision_ = snapshot.plot_revision;
    last_waterfall_revision_ = snapshot.waterfall_revision;
    draw_plot(snapshot);
    draw_waterfall(snapshot);
    draw_ramp(snapshot);
    update_overlays(snapshot);

    timer_ = lv_timer_create(timer_cb, 10, this);
}

void SpectrumScreen::create_canvases(lv_obj_t* content) {
    const auto palette = view::palette(view_model().is_dark_mode());

    plot_frame_ = lv_obj_create(content);
    lv_obj_remove_style_all(plot_frame_);
    lv_obj_set_size(plot_frame_, kPlotFrameW, kPlotFrameH);
    lv_obj_set_pos(plot_frame_, kPlotFrameX, kPlotFrameY);
    lv_obj_set_style_radius(plot_frame_, 4, 0);
    lv_obj_set_style_bg_color(plot_frame_, palette.surface, 0);
    lv_obj_set_style_bg_opa(plot_frame_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(plot_frame_, 1, 0);
    lv_obj_set_style_border_color(plot_frame_, palette.border, 0);
    lv_obj_clear_flag(plot_frame_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(plot_frame_, LV_OBJ_FLAG_CLICKABLE);

    plot_buf_ = lv_draw_buf_create(kPlotCanvasW, kPlotCanvasH, LV_COLOR_FORMAT_XRGB8888, LV_STRIDE_AUTO);
    if (plot_buf_) {
        plot_canvas_ = lv_canvas_create(plot_frame_);
        lv_canvas_set_draw_buf(plot_canvas_, plot_buf_);
        lv_obj_set_pos(plot_canvas_, 2, 2);
        lv_obj_clear_flag(plot_canvas_, LV_OBJ_FLAG_CLICKABLE);
    }

    waterfall_frame_ = lv_obj_create(content);
    lv_obj_remove_style_all(waterfall_frame_);
    lv_obj_set_size(waterfall_frame_, kWfFrameW, kWfFrameH);
    lv_obj_set_pos(waterfall_frame_, kWfFrameX, kWfFrameY);
    lv_obj_set_style_radius(waterfall_frame_, 4, 0);
    lv_obj_set_style_bg_color(waterfall_frame_, lv_color_hex(0x070A10), 0);
    lv_obj_set_style_bg_opa(waterfall_frame_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(waterfall_frame_, 1, 0);
    lv_obj_set_style_border_color(waterfall_frame_, palette.border, 0);
    lv_obj_clear_flag(waterfall_frame_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(waterfall_frame_, LV_OBJ_FLAG_CLICKABLE);

    waterfall_buf_ = lv_draw_buf_create(kWfCanvasW, kWfCanvasH, LV_COLOR_FORMAT_XRGB8888, LV_STRIDE_AUTO);
    if (waterfall_buf_) {
        waterfall_canvas_ = lv_canvas_create(waterfall_frame_);
        lv_canvas_set_draw_buf(waterfall_canvas_, waterfall_buf_);
        lv_obj_set_pos(waterfall_canvas_, 2, 2);
        lv_obj_clear_flag(waterfall_canvas_, LV_OBJ_FLAG_CLICKABLE);
    }

    ramp_frame_ = lv_obj_create(content);
    lv_obj_remove_style_all(ramp_frame_);
    lv_obj_set_size(ramp_frame_, 10, kWfCanvasH + 4);
    lv_obj_set_pos(ramp_frame_, 20, kWfFrameY);
    lv_obj_set_style_radius(ramp_frame_, 2, 0);
    lv_obj_set_style_bg_color(ramp_frame_, palette.surface, 0);
    lv_obj_set_style_bg_opa(ramp_frame_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ramp_frame_, 1, 0);
    lv_obj_set_style_border_color(ramp_frame_, palette.border, 0);
    lv_obj_clear_flag(ramp_frame_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(ramp_frame_, LV_OBJ_FLAG_CLICKABLE);

    ramp_buf_ = lv_draw_buf_create(6, kWfCanvasH, LV_COLOR_FORMAT_XRGB8888, LV_STRIDE_AUTO);
    if (ramp_buf_) {
        ramp_canvas_ = lv_canvas_create(ramp_frame_);
        lv_canvas_set_draw_buf(ramp_canvas_, ramp_buf_);
        lv_obj_set_pos(ramp_canvas_, 2, 2);
        lv_obj_clear_flag(ramp_canvas_, LV_OBJ_FLAG_CLICKABLE);
    }
}

void SpectrumScreen::draw_plot(const platform::SpectrumSnapshot& snapshot) {
    if (!plot_canvas_ || !plot_buf_ || !plot_buf_->data || !snapshot.bins ||
        snapshot.bin_count <= 0) {
        return;
    }
    const auto palette = view::palette(view_model().is_dark_mode());
    const auto background = palette.surface;
    const auto accent = palette.primary;
    const auto grid = palette.border;
    const int width = static_cast<int>(plot_buf_->header.w);
    const int height = static_cast<int>(plot_buf_->header.h);
    const int stride = static_cast<int>(plot_buf_->header.stride / sizeof(uint32_t));
    auto* pixels = reinterpret_cast<uint32_t*>(plot_buf_->data);

    fill_canvas(pixels, stride, width, height, background);

    auto frequency_to_x = [&](float frequency) {
        if (snapshot.step_mhz <= 0.0f) {
            return 0;
        }
        return static_cast<int>((frequency - snapshot.start_mhz) / (snapshot.step_mhz * snapshot.bin_count) *
                                static_cast<float>(width));
    };
    auto value_to_y = [&](int value) {
        const int low = snapshot.range_lo_dbm;
        const int high = snapshot.range_hi_dbm;
        if (high <= low) {
            return height - 1;
        }
        value = std::max(low, std::min(high, value));
        return (height - 1) - (value - low) * (height - 1) / (high - low);
    };

    // Our channel's filter band, then the dashed dB grid.
    int channel_left = frequency_to_x(snapshot.center_mhz - snapshot.channel_bw_mhz * 0.5f);
    int channel_right = frequency_to_x(snapshot.center_mhz + snapshot.channel_bw_mhz * 0.5f);
    if (channel_right <= channel_left) {
        channel_right = channel_left + 1;
    }
    const lv_color_t channel_fill = mix_color(background, accent, 20);
    for (int x = std::max(0, channel_left); x < std::min(width, channel_right); ++x) {
        for (int y = 0; y < height; ++y) {
            set_canvas_pixel(pixels, stride, x, y, channel_fill);
        }
    }

    const int grid_rows[3] = {0, (height - 1) / 2, height - 1};
    for (int row = 0; row < 3; ++row) {
        for (int x = 0; x < width; x += 3) {
            set_canvas_pixel(pixels, stride, x, grid_rows[row], grid);
        }
    }

    const int center_x = frequency_to_x(snapshot.center_mhz);
    if (center_x >= 0 && center_x < width) {
        for (int y = 0; y < height; y += 4) {
            set_canvas_pixel(pixels, stride, center_x, y, palette.text_disabled);
        }
    }

    int previous_y = -1;
    for (int x = 0; x < width; ++x) {
        const int value = column_max(snapshot.bins, snapshot.bin_count, x, width);
        const int y = value_to_y(value);
        for (int row = y + 1; row < height; ++row) {
            const int strength = 14 + 96 * (height - row) / std::max(1, height);
            set_canvas_pixel(pixels, stride, x, row, mix_color(background, accent, strength));
        }
        if (previous_y < 0) {
            set_canvas_pixel(pixels, stride, x, y, accent);
        } else {
            const int first = std::min(previous_y, y);
            const int last = std::max(previous_y, y);
            for (int row = first; row <= last; ++row) {
                set_canvas_pixel(pixels, stride, x, row, accent);
            }
        }
        if (y + 1 < height) {
            set_canvas_pixel(pixels, stride, x, y + 1, accent);
        }
        previous_y = y;
    }

    int previous_hold = -1;
    if (snapshot.hold) {
        for (int x = 0; x < width; ++x) {
            const int value = column_max(snapshot.hold, snapshot.bin_count, x, width);
            const int y = value_to_y(value);
            if ((x & 3) < 2) {
                if (previous_hold < 0) {
                    set_canvas_pixel(pixels, stride, x, y, lv_color_hex(0xF5A623));
                } else {
                    const int first = std::min(previous_hold, y);
                    const int last = std::max(previous_hold, y);
                    for (int row = first; row <= last; ++row) {
                        set_canvas_pixel(pixels, stride, x, row, lv_color_hex(0xF5A623));
                    }
                }
            }
            previous_hold = y;
        }
    }

    lv_obj_invalidate(plot_canvas_);
}

void SpectrumScreen::draw_waterfall(const platform::SpectrumSnapshot& snapshot) {
    if (!waterfall_canvas_ || !waterfall_buf_ || !waterfall_buf_->data || !snapshot.waterfall ||
        snapshot.bin_count <= 0 || snapshot.waterfall_rows <= 0) {
        return;
    }
    const int width = static_cast<int>(waterfall_buf_->header.w);
    const int height = static_cast<int>(waterfall_buf_->header.h);
    const int stride = static_cast<int>(waterfall_buf_->header.stride / sizeof(uint32_t));
    auto* pixels = reinterpret_cast<uint32_t*>(waterfall_buf_->data);

    const int low = snapshot.floor_dbm - 3;
    const int high = snapshot.floor_dbm + 24;
    const int range = std::max(1, high - low);
    for (int y = 0; y < height; ++y) {
        const int row = std::min(snapshot.waterfall_rows - 1, y * snapshot.waterfall_rows / height);
        for (int x = 0; x < width; ++x) {
            const int bin = std::min(snapshot.bin_count - 1, x * snapshot.bin_count / width);
            const int value = snapshot.waterfall[row * snapshot.bin_count + bin];
            int index = (value - low) * 63 / range;
            index = std::max(0, std::min(63, index));
            set_canvas_pixel(pixels, stride, x, y, heat_lut_[index]);
        }
    }
    lv_obj_invalidate(waterfall_canvas_);
}

void SpectrumScreen::draw_ramp(const platform::SpectrumSnapshot& snapshot) {
    if (!ramp_canvas_ || !ramp_buf_ || !ramp_buf_->data) {
        return;
    }
    const int width = static_cast<int>(ramp_buf_->header.w);
    const int height = static_cast<int>(ramp_buf_->header.h);
    const int stride = static_cast<int>(ramp_buf_->header.stride / sizeof(uint32_t));
    auto* pixels = reinterpret_cast<uint32_t*>(ramp_buf_->data);
    const int low = snapshot.floor_dbm - 3;
    const int high = snapshot.floor_dbm + 24;
    const int range = std::max(1, high - low);
    for (int y = 0; y < height; ++y) {
        const int value = high - (high - low) * y / std::max(1, height - 1);
        int index = (value - low) * 63 / range;
        index = std::max(0, std::min(63, index));
        for (int x = 0; x < width; ++x) {
            set_canvas_pixel(pixels, stride, x, y, heat_lut_[index]);
        }
    }
    lv_obj_invalidate(ramp_canvas_);
}

void SpectrumScreen::update_overlays(const platform::SpectrumSnapshot& snapshot) {
    if (!small_font_) {
        return;
    }
    const int values[3] = {snapshot.range_hi_dbm,
                           (snapshot.range_hi_dbm + snapshot.range_lo_dbm) / 2,
                           snapshot.range_lo_dbm};
    const int rows[3] = {kPlotFrameY + 1,
                         kPlotFrameY + kPlotCanvasH / 2 - 6,
                         kPlotFrameY + kPlotCanvasH - 12};
    char text[24];
    for (int i = 0; i < 3; ++i) {
        std::snprintf(text, sizeof(text), "%d", values[i]);
        lv_label_set_text(grid_labels_[i], text);
        lv_obj_set_pos(grid_labels_[i], 0, rows[i]);
    }

    const bool channel_view = snapshot.channel_view;
    const char* format = channel_view ? "%.3f" : "%.1f";
    std::snprintf(text, sizeof(text), format, static_cast<double>(snapshot.start_mhz));
    lv_label_set_text(axis_start_, text);
    std::snprintf(text, sizeof(text), format, static_cast<double>(snapshot.center_mhz));
    lv_label_set_text(axis_center_, text);
    std::snprintf(text, sizeof(text), format,
                  static_cast<double>(snapshot.start_mhz + snapshot.step_mhz * snapshot.bin_count));
    lv_label_set_text(axis_end_, text);

    const int busy = lv_subject_get_int(view_model().spectrum_busy_subject());
    const auto palette = view::palette(view_model().is_dark_mode());
    lv_obj_set_style_text_color(stat_values_[3],
                                busy >= 30 ? palette.error
                                           : busy >= 10 ? palette.warning : palette.success,
                                0);

    if (snapshot.peak_dbm <= -125 || snapshot.step_mhz <= 0.0f) {
        lv_obj_add_flag(peak_tag_, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    const float peak_frequency = snapshot.start_mhz +
                                 snapshot.step_mhz * static_cast<float>(snapshot.peak_index);
    std::snprintf(text, sizeof(text), channel_view ? "%.3f" : "%.2f",
                  static_cast<double>(peak_frequency));
    lv_label_set_text(peak_tag_, text);
    lv_obj_remove_flag(peak_tag_, LV_OBJ_FLAG_HIDDEN);

    const int plot_width = kPlotCanvasW;
    const float span = snapshot.step_mhz * snapshot.bin_count;
    const int x_in_plot = span > 0.0f
                              ? static_cast<int>((peak_frequency - snapshot.start_mhz) / span * plot_width)
                              : 0;
    lv_obj_update_layout(peak_tag_);
    const int text_width = lv_obj_get_width(peak_tag_);
    int x = kPlotFrameX + 2 + x_in_plot - text_width / 2;
    x = std::max(kPlotFrameX + 2, std::min(kPlotFrameX + kPlotFrameW - text_width - 2, x));
    const int low = snapshot.range_lo_dbm;
    const int high = snapshot.range_hi_dbm;
    int y = kPlotFrameY + 2;
    if (high > low) {
        const int clamped = std::max(low, std::min(high, snapshot.peak_dbm));
        y = kPlotFrameY + 2 + (kPlotCanvasH - 1) -
            (clamped - low) * (kPlotCanvasH - 1) / (high - low) -
            lv_font_get_line_height(small_font_);
    }
    y = std::max(kPlotFrameY + 1, y);
    lv_obj_set_pos(peak_tag_, x, y);
}

void SpectrumScreen::apply_theme() {
    const auto palette = view::palette(view_model().is_dark_mode());
    if (stats_bar_) {
        lv_obj_set_style_bg_color(stats_bar_, palette.surface, 0);
        lv_obj_set_style_border_color(stats_bar_, palette.border, 0);
    }
    for (int i = 0; i < 4; ++i) {
        if (stat_titles_[i]) {
            lv_obj_set_style_text_color(stat_titles_[i], palette.text, 0);
            lv_obj_set_style_text_opa(stat_titles_[i], LV_OPA_70, 0);
        }
    }
    for (auto* separator : stat_separators_) {
        if (separator) lv_obj_set_style_bg_color(separator, palette.border, 0);
    }
    if (stat_values_[1]) lv_obj_set_style_text_color(stat_values_[1], palette.text, 0);
    if (stat_values_[2]) lv_obj_set_style_text_color(stat_values_[2], palette.primary, 0);
    if (plot_frame_) {
        lv_obj_set_style_bg_color(plot_frame_, palette.surface, 0);
        lv_obj_set_style_border_color(plot_frame_, palette.border, 0);
    }
    if (waterfall_frame_) lv_obj_set_style_border_color(waterfall_frame_, palette.border, 0);
    if (ramp_frame_) {
        lv_obj_set_style_bg_color(ramp_frame_, palette.surface, 0);
        lv_obj_set_style_border_color(ramp_frame_, palette.border, 0);
    }
    for (auto* label : grid_labels_) {
        if (label) {
            lv_obj_set_style_text_color(label, palette.text, 0);
            lv_obj_set_style_text_opa(label, LV_OPA_70, 0);
        }
    }
    for (auto* label : {axis_start_, axis_center_, axis_end_}) {
        if (label) {
            lv_obj_set_style_text_color(label, palette.text, 0);
            lv_obj_set_style_text_opa(label, LV_OPA_70, 0);
        }
    }
    if (status_) lv_obj_set_style_text_color(status_, palette.warning, 0);
}

void SpectrumScreen::force_redraw() {
    last_plot_revision_ = UINT32_MAX;
    last_waterfall_revision_ = UINT32_MAX;
}

void SpectrumScreen::timer_cb(lv_timer_t* timer) {
    auto* screen = static_cast<SpectrumScreen*>(lv_timer_get_user_data(timer));
    if (!screen) {
        return;
    }
    screen->view_model().poll_spectrum();
    const auto& snapshot = screen->view_model().spectrum_snapshot();
    if (snapshot.plot_revision != screen->last_plot_revision_) {
        screen->last_plot_revision_ = snapshot.plot_revision;
        screen->draw_plot(snapshot);
        screen->draw_ramp(snapshot);
        screen->update_overlays(snapshot);
    }
    if (snapshot.waterfall_revision != screen->last_waterfall_revision_) {
        screen->last_waterfall_revision_ = snapshot.waterfall_revision;
        screen->draw_waterfall(snapshot);
    }
}

void SpectrumScreen::status_changed(lv_observer_t* observer, lv_subject_t* subject) {
    auto* screen = static_cast<SpectrumScreen*>(lv_observer_get_user_data(observer));
    if (!screen || !screen->status_) {
        return;
    }
    const char* text = lv_subject_get_string(subject);
    if (text && text[0] != '\0') {
        lv_label_set_text(screen->status_, text);
        lv_obj_remove_flag(screen->status_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(screen->status_, LV_OBJ_FLAG_HIDDEN);
    }
}

void SpectrumScreen::theme_changed(lv_observer_t* observer, lv_subject_t*) {
    auto* screen = static_cast<SpectrumScreen*>(lv_observer_get_user_data(observer));
    if (screen) {
        screen->apply_theme();
        screen->force_redraw();
    }
}

} // namespace screen
