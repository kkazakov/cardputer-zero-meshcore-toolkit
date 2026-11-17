/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "mesh_config.h"
#include "mesh_radio.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace platform {

struct DiscoverNode {
    uint8_t pubkey[32]{};
    uint8_t pubkey_len{0};
    uint8_t node_type{0};
    int8_t rssi{0};
    int8_t snr_q4{0};
    int8_t their_snr_q4{0};
    uint8_t hops{0};
    uint32_t first_ms{0};
    uint32_t last_ms{0};
    uint16_t heard{0};
    bool name_valid{false};
    char name[32]{};
};

struct DiscoverStats {
    int total{0};
    int repeaters{0};
    int companions{0};
    int rooms{0};
    int sensors{0};
    bool scanning{false};
    bool simulated{false};
    bool radio_ready{false};
};

// Active MeshCore node discovery: one zero-hop NODE_DISCOVER_REQ per sweep,
// then listen for NODE_DISCOVER_RESP replies. ADVERT packets heard while
// listening feed a small display-name cache (the discovery reply itself only
// carries the public key). All work happens on the caller's thread from
// tick(); a callback is never invoked from inside the driver.
class DiscoverScanner {
public:
    explicit DiscoverScanner(MeshRadio& radio);

    void start(const model::MeshConfig& config, uint32_t now_ms);
    void stop();
    void tick(uint32_t now_ms);

    void set_scanning(bool scanning);
    bool scanning() const;
    bool active() const;

    const DiscoverNode* nodes() const;
    size_t node_count() const;
    const DiscoverStats& stats() const;
    uint32_t revision() const;
    const std::string& error() const;

    static constexpr size_t kMaxNodes = 64;

private:
    void poll_receive(uint32_t now_ms);
    void transmit_sweep(uint32_t now_ms);
    void upsert_node(const uint8_t* pubkey,
                     uint8_t pubkey_len,
                     uint8_t node_type,
                     int8_t rssi,
                     int8_t snr_q4,
                     int8_t their_snr_q4,
                     uint8_t hops,
                     uint32_t now_ms);
    void upsert_name(const uint8_t* pubkey, uint8_t node_type, const char* name, uint32_t now_ms);
    const char* lookup_name(const uint8_t* pubkey) const;
    void update_stats();
    void simulate(uint32_t now_ms);

    MeshRadio& radio_;
    bool active_{false};
    bool scanning_{true};
    bool simulated_{false};
    uint32_t tag_{0};
    uint32_t started_ms_{0};
    uint32_t next_sweep_ms_{0};
    uint8_t sweep_index_{0};
    uint32_t revision_{0};
    std::string error_;

    DiscoverNode nodes_[kMaxNodes]{};
    size_t node_count_{0};

    struct NameRecord {
        uint8_t key[8]{};
        uint8_t node_type{0};
        char name[32]{};
        uint32_t last_ms{0};
        bool valid{false};
    };
    NameRecord names_[kMaxNodes]{};
    size_t name_count_{0};

    DiscoverStats stats_{};

    // Desktop-only simulation cadence.
    uint32_t last_sim_ms_{0};
};

} // namespace platform
