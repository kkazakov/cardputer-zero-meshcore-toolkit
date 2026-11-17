/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "spectrum_scanner.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <thread>

namespace platform {
namespace {

constexpr float kSpanMhz = 24.0f;
constexpr float kRbwKhz = 62.5f;
constexpr float kMinFrequencyMhz = 150.0f;
constexpr float kMaxFrequencyMhz = 960.0f;
constexpr int kChunkBins = 8;
constexpr int kSettleUs = 1500;
constexpr int kDwellReads = 6;
constexpr int kReadGapUs = 400;
constexpr int kHoldDecayDb = 1;
constexpr int kBusyAboveMedianDb = 8;
constexpr int kRangePadLowDb = 4;
constexpr int kRangePadHighDb = 6;
constexpr int kRangeMinSpanDb = 30;
constexpr int kRangeDecayDb = 3;
constexpr int kFloorDbm = -130;
constexpr int kLiveRowIntervalMs = 500;
constexpr int kSimStepMs = 200;

int clamp_int(int value, int low, int high) {
    return std::max(low, std::min(high, value));
}

int floor_to_5(int value) {
    return value >= 0 ? value / 5 * 5 : -(((-value) + 4) / 5 * 5);
}

int ceil_to_5(int value) {
    return value >= 0 ? ((value + 4) / 5 * 5) : -(((-value) / 5) * 5);
}

float clamp_frequency(float mhz) {
    return std::max(kMinFrequencyMhz, std::min(kMaxFrequencyMhz, mhz));
}

} // namespace

SpectrumScanner::SpectrumScanner(MeshRadio& radio) : radio_(radio) {}

void SpectrumScanner::start(const model::MeshConfig& config, uint32_t now_ms) {
    active_ = true;
    simulated_ = radio_.simulated();
    error_.clear();
    center_mhz_ = static_cast<float>(config.freq_hz) / 1000000.0f;
    channel_bw_mhz_ = model::bandwidth_khz(config.bw_index) / 1000.0f;
    if (channel_bw_mhz_ <= 0.0f) {
        channel_bw_mhz_ = 0.0625f;
    }
    last_sim_ms_ = now_ms;

    if (!simulated_) {
        if (!radio_.ready()) {
            error_ = radio_.error().empty() ? "radio is not ready" : radio_.error();
            active_ = false;
            return;
        }
        if (!radio_.apply_spectrum_config(config, kRbwKhz)) {
            error_ = radio_.error().empty() ? "spectrum radio configuration failed" : radio_.error();
            active_ = false;
            return;
        }
        (void)radio_.set_rx_boosted_gain(true);
    }

    apply_span(now_ms);
    if (simulated_) {
        last_sim_ms_ = 0;
        simulate(now_ms);
    } else {
        (void)radio_.standby();
    }
}

void SpectrumScanner::stop() {
    active_ = false;
    if (!simulated_) {
        radio_.standby();
    }
}

void SpectrumScanner::tick(uint32_t now_ms) {
    if (!active_) {
        return;
    }
    if (simulated_) {
        simulate(now_ms);
        return;
    }
    sweep_chunk(now_ms);
}

void SpectrumScanner::sweep_chunk(uint32_t now_ms) {
    const int end = std::min(sweep_pos_ + kChunkBins, SpectrumSnapshot::kBins);
    for (int i = sweep_pos_; i < end; ++i) {
        const float frequency = clamp_frequency(start_mhz_ + static_cast<float>(i) * step_mhz_);
        (void)radio_.standby();
        if (!radio_.set_frequency_mhz(frequency)) {
            error_ = radio_.error();
            continue;
        }
        if (!radio_.start_receive()) {
            error_ = radio_.error();
            continue;
        }

        std::this_thread::sleep_for(std::chrono::microseconds(kSettleUs));
        int peak = -200;
        for (int read = 0; read < kDwellReads; ++read) {
            const float rssi = radio_.instant_rssi();
            const int value = static_cast<int>(std::lround(rssi));
            if (value >= -180 && value <= -5 && value > peak) {
                peak = value;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(kReadGapUs));
        }
        (void)radio_.standby();
        if (peak > -200) {
            bins_[i] = static_cast<int16_t>(peak);
        }
    }

    sweep_pos_ = end;
    if (sweep_pos_ >= SpectrumSnapshot::kBins) {
        sweep_pos_ = 0;
        complete_sweep(now_ms);
    } else {
        ++plot_revision_;
    }
}

void SpectrumScanner::simulate(uint32_t now_ms) {
    if (last_sim_ms_ != 0 && now_ms - last_sim_ms_ < kSimStepMs) {
        return;
    }
    last_sim_ms_ = now_ms;

    const float t = static_cast<float>(now_ms) * 0.001f;
    for (int i = 0; i < SpectrumSnapshot::kBins; ++i) {
        const float frequency = start_mhz_ + static_cast<float>(i) * step_mhz_;
        float noise = std::sin(static_cast<float>(i) * 1.37f + t * 2.1f) * 2.2f +
                      std::sin(static_cast<float>(i) * 0.41f + t * 0.7f) * 1.4f;
        float value = -112.0f + noise;

        auto add_peak = [&](float centre, float sigma, float amplitude) {
            const float d = (frequency - centre) / sigma;
            value += amplitude * std::exp(-0.5f * d * d);
        };
        add_peak(860.02f, 0.22f, 29.0f);
        add_peak(center_mhz_ - 0.15f, 0.85f, 20.0f);
        add_peak(881.20f, 0.28f, 18.0f);

        // A packet burst every few seconds near the channel, so the busy tile
        // and the waterfall show something moving.
        const int burst_phase = static_cast<int>(t * 0.5f) % 7;
        if (burst_phase == 0) {
            add_peak(center_mhz_ + 0.4f, 0.12f, 16.0f);
        }

        bins_[i] = static_cast<int16_t>(clamp_int(static_cast<int>(std::lround(value)), -125, -45));
    }

    complete_sweep(now_ms);
}

void SpectrumScanner::complete_sweep(uint32_t now_ms) {
    for (int i = 0; i < SpectrumSnapshot::kBins; ++i) {
        const int16_t value = bins_[i];
        const int held = static_cast<int>(hold_[i]) - kHoldDecayDb;
        hold_[i] = static_cast<int16_t>(std::max<int>(value, held));
        if (value > acc_[i]) {
            acc_[i] = value;
        }
    }

    update_floor();
    update_stats();
    track_range(true);
    ++plot_revision_;
    push_waterfall_row(now_ms);
    ++sweep_count_;
}

void SpectrumScanner::update_floor() {
    int minimum = 9999;
    for (int i = 0; i < SpectrumSnapshot::kBins; ++i) {
        minimum = std::min<int>(minimum, bins_[i]);
    }
    if (minimum > -20 || minimum < -190) {
        return;
    }
    if (sweep_count_ == 0) {
        floor_dbm_ = minimum;
    } else {
        floor_dbm_ = (floor_dbm_ * 3 + minimum) / 4;
    }
}

void SpectrumScanner::update_stats() {
    int peak = -200;
    int peak_index = 0;
    for (int i = 0; i < SpectrumSnapshot::kBins; ++i) {
        if (bins_[i] > peak) {
            peak = bins_[i];
            peak_index = i;
        }
    }
    peak_dbm_ = peak;
    peak_index_ = peak_index;

    int first = static_cast<int>(std::floor((center_mhz_ - channel_bw_mhz_ * 0.5f - start_mhz_) / step_mhz_));
    int last = static_cast<int>(std::ceil((center_mhz_ + channel_bw_mhz_ * 0.5f - start_mhz_) / step_mhz_));
    first = clamp_int(first, 0, SpectrumSnapshot::kBins - 1);
    last = clamp_int(last, first, SpectrumSnapshot::kBins - 1);
    int channel = -200;
    for (int i = first; i <= last; ++i) {
        channel = std::max<int>(channel, bins_[i]);
    }
    channel_dbm_ = channel;

    int16_t sorted[SpectrumSnapshot::kBins];
    std::memcpy(sorted, bins_, sizeof(sorted));
    std::nth_element(sorted, sorted + SpectrumSnapshot::kBins / 2, sorted + SpectrumSnapshot::kBins);
    const int median = sorted[SpectrumSnapshot::kBins / 2];

    busy_bits_ = (busy_bits_ << 1U) |
                 (channel > median + kBusyAboveMedianDb ? 1ULL : 0ULL);
    if (busy_count_ < 64) {
        ++busy_count_;
    }
    const uint64_t mask = busy_count_ >= 64 ? ~0ULL : ((1ULL << busy_count_) - 1ULL);
    busy_percent_ = busy_count_ > 0
                        ? static_cast<int>(__builtin_popcountll(busy_bits_ & mask) * 100 / busy_count_)
                        : 0;
}

void SpectrumScanner::track_range(bool settle) {
    int minimum = 999;
    int maximum = -999;
    for (int i = 0; i < SpectrumSnapshot::kBins; ++i) {
        minimum = std::min<int>(minimum, bins_[i]);
        maximum = std::max<int>(maximum, bins_[i]);
        maximum = std::max<int>(maximum, hold_[i]);
    }

    int target_low = minimum - kRangePadLowDb;
    int target_high = maximum + kRangePadHighDb;
    if (target_high - target_low < kRangeMinSpanDb) {
        const int grow = kRangeMinSpanDb - (target_high - target_low);
        target_low -= grow / 2;
        target_high += grow - grow / 2;
    }

    if (target_low < range_lo_) {
        range_lo_ = target_low;
    } else if (settle) {
        range_lo_ = std::min(target_low, range_lo_ + kRangeDecayDb);
    }
    if (target_high > range_hi_) {
        range_hi_ = target_high;
    } else if (settle) {
        range_hi_ = std::max(target_high, range_hi_ - kRangeDecayDb);
    }

    int low = floor_to_5(range_lo_);
    int high = ceil_to_5(range_hi_);
    if (((high - low) % 10) != 0) {
        high += 5;
    }
    if (low == disp_lo_ && high == disp_hi_) {
        return;
    }
    disp_lo_ = low;
    disp_hi_ = high;
    ++plot_revision_;
}

void SpectrumScanner::apply_span(uint32_t now_ms) {
    span_mhz_ = channel_view_ ? channel_bw_mhz_ * 16.0f : kSpanMhz;
    span_mhz_ = std::max(0.5f, std::min(4.0f, span_mhz_));
    if (!channel_view_) {
        span_mhz_ = kSpanMhz;
    }

    float start = clamp_frequency(center_mhz_ - span_mhz_ * 0.5f);
    float stop = clamp_frequency(center_mhz_ + span_mhz_ * 0.5f);
    if (stop - start < 0.2f) {
        stop = std::min(kMaxFrequencyMhz, start + 0.2f);
        start = std::max(kMinFrequencyMhz, stop - 0.2f);
    }
    start_mhz_ = start;
    span_mhz_ = stop - start;
    step_mhz_ = span_mhz_ / static_cast<float>(SpectrumSnapshot::kBins);

    for (int i = 0; i < SpectrumSnapshot::kBins; ++i) {
        bins_[i] = static_cast<int16_t>(kFloorDbm);
        hold_[i] = static_cast<int16_t>(kFloorDbm);
        acc_[i] = -200;
    }
    clear_waterfall();

    sweep_pos_ = 0;
    sweep_count_ = 0;
    peak_dbm_ = -130;
    peak_index_ = 0;
    floor_dbm_ = -120;
    channel_dbm_ = -130;
    busy_percent_ = 0;
    busy_bits_ = 0;
    busy_count_ = 0;
    range_lo_ = -130;
    range_hi_ = -80;
    disp_lo_ = -130;
    disp_hi_ = -80;
    last_row_ms_ = 0;
    (void)now_ms;
    ++plot_revision_;
}

void SpectrumScanner::clear_waterfall() {
    for (int i = 0; i < SpectrumSnapshot::kRows * SpectrumSnapshot::kBins; ++i) {
        waterfall_[i] = static_cast<int16_t>(kFloorDbm);
    }
    ++waterfall_revision_;
}

void SpectrumScanner::push_waterfall_row(uint32_t now_ms) {
    const int interval = window_ == 0 ? kLiveRowIntervalMs
                         : window_ == 1 ? 60000 / SpectrumSnapshot::kRows
                                        : 600000 / SpectrumSnapshot::kRows;
    if (last_row_ms_ != 0 && static_cast<int32_t>(now_ms - last_row_ms_) < interval) {
        return;
    }
    last_row_ms_ = now_ms;

    std::memmove(&waterfall_[SpectrumSnapshot::kBins],
                 &waterfall_[0],
                 sizeof(int16_t) * SpectrumSnapshot::kBins * (SpectrumSnapshot::kRows - 1));
    for (int i = 0; i < SpectrumSnapshot::kBins; ++i) {
        waterfall_[i] = acc_[i] > -200 ? acc_[i] : bins_[i];
        acc_[i] = -200;
    }
    ++waterfall_revision_;
}

void SpectrumScanner::set_channel_view(bool channel_view, uint32_t now_ms) {
    if (channel_view_ == channel_view) {
        return;
    }
    channel_view_ = channel_view;
    apply_span(now_ms);
    if (simulated_) {
        last_sim_ms_ = 0;
        simulate(now_ms);
    }
}

bool SpectrumScanner::channel_view() const {
    return channel_view_;
}

void SpectrumScanner::cycle_window(int delta, uint32_t now_ms) {
    window_ = (window_ + 3 + (delta % 3)) % 3;
    clear_waterfall();
    for (int i = 0; i < SpectrumSnapshot::kBins; ++i) {
        acc_[i] = -200;
    }
    last_row_ms_ = 0;
    if (simulated_) {
        last_sim_ms_ = 0;
        simulate(now_ms);
    }
}

int SpectrumScanner::window() const {
    return window_;
}

SpectrumSnapshot SpectrumScanner::snapshot() const {
    SpectrumSnapshot snapshot;
    snapshot.bins = bins_;
    snapshot.hold = hold_;
    snapshot.bin_count = SpectrumSnapshot::kBins;
    snapshot.waterfall = waterfall_;
    snapshot.waterfall_rows = SpectrumSnapshot::kRows;
    snapshot.peak_dbm = peak_dbm_;
    snapshot.peak_index = peak_index_;
    snapshot.floor_dbm = floor_dbm_;
    snapshot.channel_dbm = channel_dbm_;
    snapshot.busy_percent = busy_percent_;
    snapshot.start_mhz = start_mhz_;
    snapshot.step_mhz = step_mhz_;
    snapshot.center_mhz = center_mhz_;
    snapshot.span_mhz = span_mhz_;
    snapshot.channel_bw_mhz = channel_bw_mhz_;
    snapshot.range_lo_dbm = disp_lo_;
    snapshot.range_hi_dbm = disp_hi_;
    snapshot.channel_view = channel_view_;
    snapshot.window = window_;
    snapshot.simulated = simulated_;
    snapshot.radio_ready = radio_.ready();
    snapshot.plot_revision = plot_revision_;
    snapshot.waterfall_revision = waterfall_revision_;
    return snapshot;
}

const std::string& SpectrumScanner::error() const {
    return error_;
}

bool SpectrumScanner::active() const {
    return active_;
}

} // namespace platform
