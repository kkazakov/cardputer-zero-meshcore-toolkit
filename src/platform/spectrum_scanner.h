/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "mesh_config.h"
#include "mesh_radio.h"

#include <cstdint>
#include <string>

namespace platform {

struct SpectrumSnapshot {
    static constexpr int kBins = 160;
    static constexpr int kRows = 22;

    const int16_t* bins{nullptr};
    int bin_count{kBins};
    const int16_t* hold{nullptr};
    const int16_t* waterfall{nullptr};
    int waterfall_rows{kRows};

    int peak_dbm{-130};
    int peak_index{0};
    int floor_dbm{-120};
    int channel_dbm{-130};
    int busy_percent{0};

    float start_mhz{0.0f};
    float step_mhz{0.0f};
    float center_mhz{0.0f};
    float span_mhz{24.0f};
    float channel_bw_mhz{0.0625f};
    int range_lo_dbm{-130};
    int range_hi_dbm{-80};

    bool channel_view{false};
    int window{0}; // 0 = live, 1 = 1 min, 2 = 10 min
    bool simulated{false};
    bool radio_ready{false};

    uint32_t plot_revision{0};
    uint32_t waterfall_revision{0};
};

// Swept-RSSI spectrum analyzer for the SX1262. The radio is retuned across a
// 24 MHz span centred on the configured channel; each bin is armed in RX,
// allowed to settle, and peak-held over a short dwell. A full sweep is split
// across timer ticks so the LVGL thread keeps running. Waterfall history is
// kept in RAM as raw dBm rows; colours are a view concern.
class SpectrumScanner {
public:
    explicit SpectrumScanner(MeshRadio& radio);

    void start(const model::MeshConfig& config, uint32_t now_ms);
    void stop();
    void tick(uint32_t now_ms);

    void set_channel_view(bool channel_view, uint32_t now_ms);
    bool channel_view() const;
    void cycle_window(int delta, uint32_t now_ms);
    int window() const;

    SpectrumSnapshot snapshot() const;
    const std::string& error() const;
    bool active() const;

private:
    void apply_span(uint32_t now_ms);
    void sweep_chunk(uint32_t now_ms);
    void simulate(uint32_t now_ms);
    void complete_sweep(uint32_t now_ms);
    void update_floor();
    void update_stats();
    void track_range(bool settle);
    void push_waterfall_row(uint32_t now_ms);
    void clear_waterfall();

    MeshRadio& radio_;
    bool active_{false};
    bool simulated_{false};
    std::string error_;

    int16_t bins_[SpectrumSnapshot::kBins]{};
    int16_t hold_[SpectrumSnapshot::kBins]{};
    int16_t acc_[SpectrumSnapshot::kBins]{};
    int16_t waterfall_[SpectrumSnapshot::kRows * SpectrumSnapshot::kBins]{};

    float start_mhz_{0.0f};
    float step_mhz_{0.0f};
    float center_mhz_{0.0f};
    float span_mhz_{24.0f};
    float channel_bw_mhz_{0.0625f};

    int sweep_pos_{0};
    int peak_dbm_{-130};
    int peak_index_{0};
    int floor_dbm_{-120};
    int channel_dbm_{-130};
    int busy_percent_{0};
    int range_lo_{-130};
    int range_hi_{-80};
    int disp_lo_{-130};
    int disp_hi_{-80};
    uint64_t busy_bits_{0};
    int busy_count_{0};

    bool channel_view_{false};
    int window_{0};
    uint32_t plot_revision_{0};
    uint32_t waterfall_revision_{0};
    uint32_t last_row_ms_{0};
    uint32_t sweep_count_{0};
    uint32_t last_sim_ms_{0};
};

} // namespace platform
