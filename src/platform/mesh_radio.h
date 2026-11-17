/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "mesh_config.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace platform {

// SX1262 transport for the Cap LoRa-1262.
//
// The radio is shared by the Discover scanner (MeshCore control packets) and
// the Spectrum scanner (RSSI sweep). Only one session is active at a time;
// callers configure the modem and then use either transmit/receive or the
// spectrum helpers. The class owns the SPI/GPIO resources for its lifetime.
//
// Desktop builds are explicitly simulated: initialize() succeeds with
// simulated() == true, nothing touches hardware, and receive() never returns
// a packet. Scanners provide their own visible simulation data.
class MeshRadio {
public:
    MeshRadio();
    ~MeshRadio();

    MeshRadio(const MeshRadio&) = delete;
    MeshRadio& operator=(const MeshRadio&) = delete;

    bool initialize(const model::MeshConfig& config, std::string& error);
    void shutdown();

    bool ready() const;
    bool simulated() const;
    const std::string& error() const;
    const std::string& detail() const;
    const std::string& spi_device() const;

    bool apply_config(const model::MeshConfig& config);
    bool apply_spectrum_config(const model::MeshConfig& config, float bandwidth_khz);

    bool transmit(const uint8_t* data, size_t length);
    bool start_receive();
    bool receive(uint8_t* data, size_t capacity, size_t& length, float& rssi, float& snr);
    bool standby();
    bool sleep();

    bool set_frequency_mhz(float frequency_mhz);
    bool set_bandwidth_khz(float bandwidth_khz);
    bool set_spreading_factor(int spreading_factor);
    bool set_coding_rate(int coding_rate);
    bool set_sync_word(int sync_word);
    bool set_preamble(int symbols);
    bool set_tx_power(int dbm);
    bool set_rx_boosted_gain(bool enabled);
    float instant_rssi();
    uint16_t irq_flags();
    bool clear_irq();
    bool is_receiving();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool ready_{false};
    bool simulated_{false};
    std::string error_;
    std::string detail_;
    std::string spi_device_;
};

} // namespace platform
