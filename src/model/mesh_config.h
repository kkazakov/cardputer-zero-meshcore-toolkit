/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>

namespace model {

// User-editable node name length and the storage buffer size (includes NUL).
constexpr int kMaxNodeName    = 20;
constexpr int kNodeNameBuffer = 24;

// Regulatory / community regions. "Custom" keeps the manually entered radio
// parameters so the operator can set an arbitrary frequency plan.
enum class Region {
    Europe    = 0,
    UsCanada  = 1,
    Australia = 2,
    NewZealand = 3,
    Eu433     = 4,
    Custom    = 5,
};

constexpr int kRegionCount = 6;

const char* region_name(Region region);

struct RegionPreset {
    const char* label;
    std::uint32_t freq_hz;
    int sf;
    int bw_index;
    int cr; // 5..8 -> coding rate 4/5 .. 4/8
};

const RegionPreset& region_preset(Region region);

// Canonical LoRa bandwidths, indexed by bw_index (0..9).
constexpr int kBandwidthCount = 10;
const char* bandwidth_label(int index);
float bandwidth_khz(int index);

// MeshCore derives its LoRa preamble length from the spreading factor
// (32 symbols for SF <= 8, 16 above). Keep presets on that rule so discovery
// requests are heard by stock MeshCore nodes.
int meshcore_preamble_for_sf(int sf);

// Primary MeshCore radio settings. Mirrors MeshCore's `lora` namespace and
// NodePrefs (https://github.com/meshcore-dev/MeshCore) so the on-device config
// stays portable to a real MeshCore firmware build.
struct MeshConfig {
    char node_name[kNodeNameBuffer] = "MeshToolkit";
    Region region                    = Region::Europe;
    std::uint32_t freq_hz            = 869618000; // Europe default
    int sf                           = 8;
    int bw_index                     = 6;          // 62.5 kHz
    int cr                           = 5;          // 4/5
    int tx_power_dbm                 = 22;
    int sync_word                    = 0x12; // MeshCore private sync word
    int preamble_len                 = 32;
};

MeshConfig default_mesh_config();

// Applies a region preset to the radio fields. `Region::Custom` leaves the
// current manual values untouched.
void apply_region_preset(MeshConfig& config, Region region);

} // namespace model
