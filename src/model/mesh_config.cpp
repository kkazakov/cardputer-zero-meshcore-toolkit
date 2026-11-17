/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "mesh_config.h"

#include <array>

namespace model {
namespace {

constexpr std::array<RegionPreset, kRegionCount> kPresets = {{
    {"Europe",      869618000, 8,  6, 8}, // EU/UK narrow: 4/8 coding
    {"USA / Canada", 910525000, 7,  6, 5}, // NA narrow
    {"Australia",   915800000, 10, 8, 5}, // AU915
    {"New Zealand", 917375000, 11, 8, 5}, // NZ915
    {"EU 433",      433650000, 11, 8, 5}, // EU433 long-range
    {"Custom",      0,         0,  0, 5},
}};

constexpr std::array<const char*, kBandwidthCount> kBandwidths = {{
    "7.8 kHz", "10.4 kHz", "15.6 kHz", "20.8 kHz", "31.25 kHz",
    "41.7 kHz", "62.5 kHz", "125 kHz", "250 kHz", "500 kHz",
}};

constexpr std::array<float, kBandwidthCount> kBandwidthKhz = {{
    7.8f, 10.4f, 15.6f, 20.8f, 31.25f, 41.7f, 62.5f, 125.0f, 250.0f, 500.0f,
}};

} // namespace

const char* region_name(Region region) {
    return kPresets[static_cast<int>(region)].label;
}

const RegionPreset& region_preset(Region region) {
    return kPresets[static_cast<int>(region)];
}

const char* bandwidth_label(int index) {
    if (index < 0 || index >= kBandwidthCount) {
        return "?";
    }
    return kBandwidths[index];
}

float bandwidth_khz(int index) {
    if (index < 0 || index >= kBandwidthCount) {
        return 62.5f;
    }
    return kBandwidthKhz[index];
}

int meshcore_preamble_for_sf(int sf) {
    return sf <= 8 ? 32 : 16;
}

MeshConfig default_mesh_config() {
    MeshConfig config;
    apply_region_preset(config, Region::Europe);
    return config;
}

void apply_region_preset(MeshConfig& config, Region region) {
    config.region = region;
    if (region == Region::Custom) {
        return; // preserve current manual values
    }

    const auto& preset = region_preset(region);
    config.freq_hz = preset.freq_hz;
    config.sf      = preset.sf;
    config.bw_index = preset.bw_index;
    config.cr      = preset.cr;
    config.sync_word = 0x12; // MeshCore always uses the private sync word
    config.preamble_len = meshcore_preamble_for_sf(config.sf);
}

} // namespace model
