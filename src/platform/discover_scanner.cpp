/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "discover_scanner.h"

#include "meshcore_protocol.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace platform {
namespace {

constexpr uint32_t kSweepIntervalsMs[] = {4000, 4000, 8000, 15000, 30000};
constexpr size_t kSweepIntervalCount = sizeof(kSweepIntervalsMs) / sizeof(kSweepIntervalsMs[0]);
constexpr size_t kMaxPacketsPerTick = 6;
constexpr uint8_t kAllNodeTypes = 0x1E; // chat | repeater | room | sensor

int8_t clamp_i8(int value) {
    return static_cast<int8_t>(std::max(-128, std::min(127, value)));
}

uint32_t next_tag(uint32_t now_ms) {
    static uint32_t counter = 0;
    uint32_t tag = (now_ms * 2654435761U) ^ ((++counter) * 40503U) ^ 0xA5C3E1F7U;
    return tag ? tag : 1U;
}

} // namespace

DiscoverScanner::DiscoverScanner(MeshRadio& radio) : radio_(radio) {}

void DiscoverScanner::start(const model::MeshConfig& config, uint32_t now_ms) {
    active_ = true;
    scanning_ = true;
    simulated_ = radio_.simulated();
    started_ms_ = now_ms;
    next_sweep_ms_ = now_ms;
    sweep_index_ = 0;
    last_sim_ms_ = now_ms;
    tag_ = next_tag(now_ms);
    node_count_ = 0;
    name_count_ = 0;
    std::memset(nodes_, 0, sizeof(nodes_));
    std::memset(names_, 0, sizeof(names_));
    error_.clear();
    ++revision_;
    update_stats();

    if (simulated_) {
        // A compact, clearly-simulated neighbourhood so the desktop simulator
        // exercises every row colour, the scroll path and the paused state.
        struct SimNode {
            const char* name;
            uint8_t type;
            int8_t rssi;
            int8_t snr_q4;
            uint8_t hops;
        };
        const SimNode seed[] = {
            {"Ridge Rptr", meshcore::kAdvTypeRepeater, -78, 49, 0},
            {"Hilltop R2", meshcore::kAdvTypeRepeater, -102, 24, 0},
            {"OCREP Demo", meshcore::kAdvTypeRepeater, -105, 14, 0},
            {"Banya Link", meshcore::kAdvTypeRepeater, -105, 18, 0},
            {"Longshot", meshcore::kAdvTypeSensor, -116, -28, 1},
            {"Creek Relay", meshcore::kAdvTypeRepeater, -108, 8, 0},
            {"Plaza Chat", meshcore::kAdvTypeChat, -111, 6, 1},
        };
        for (const auto& item : seed) {
            uint8_t key[32] = {};
            const size_t name_len = std::strlen(item.name);
            for (size_t i = 0; i < sizeof(key); ++i) {
                key[i] = static_cast<uint8_t>(item.name[i % name_len] ^ (0x31U + i));
            }
            upsert_name(key, item.type, item.name, now_ms);
            upsert_node(key, sizeof(key), item.type, item.rssi, item.snr_q4, 0, item.hops, now_ms);
        }
        update_stats();
        return;
    }

    if (!radio_.ready()) {
        error_ = radio_.error().empty() ? "radio is not ready" : radio_.error();
        active_ = false;
        scanning_ = false;
        update_stats();
        return;
    }

    if (!radio_.apply_config(config)) {
        error_ = radio_.error().empty() ? "radio configuration failed" : radio_.error();
        active_ = false;
        scanning_ = false;
        update_stats();
        return;
    }
    if (!radio_.start_receive()) {
        error_ = radio_.error().empty() ? "could not arm receive" : radio_.error();
        scanning_ = false;
    }
    update_stats();
}

void DiscoverScanner::stop() {
    active_ = false;
    scanning_ = false;
    if (!simulated_) {
        radio_.standby();
    }
    update_stats();
}

void DiscoverScanner::set_scanning(bool scanning) {
    if (!active_) {
        return;
    }
    scanning_ = scanning;
    if (scanning) {
        sweep_index_ = 0;
        next_sweep_ms_ = 0; // sweep on the next tick
    }
    update_stats();
    ++revision_;
}

bool DiscoverScanner::scanning() const {
    return scanning_;
}

bool DiscoverScanner::active() const {
    return active_;
}

const DiscoverNode* DiscoverScanner::nodes() const {
    return nodes_;
}

size_t DiscoverScanner::node_count() const {
    return node_count_;
}

const DiscoverStats& DiscoverScanner::stats() const {
    return stats_;
}

uint32_t DiscoverScanner::revision() const {
    return revision_;
}

const std::string& DiscoverScanner::error() const {
    return error_;
}

void DiscoverScanner::tick(uint32_t now_ms) {
    if (!active_) {
        return;
    }

    if (simulated_) {
        simulate(now_ms);
        return;
    }

    poll_receive(now_ms);

    if (scanning_ && (next_sweep_ms_ == 0 || static_cast<int32_t>(now_ms - next_sweep_ms_) >= 0)) {
        transmit_sweep(now_ms);
    }
    update_stats();
}

void DiscoverScanner::poll_receive(uint32_t now_ms) {
    for (size_t packet_index = 0; packet_index < kMaxPacketsPerTick; ++packet_index) {
        uint8_t buffer[255] = {};
        size_t length = 0;
        float rssi = 0.0f;
        float snr = 0.0f;
        if (!radio_.receive(buffer, sizeof(buffer), length, rssi, snr)) {
            break;
        }

        meshcore::PacketView packet;
        if (!meshcore::parse_packet(buffer, length, packet)) {
            continue;
        }

        if (packet.payload_type == meshcore::kPayloadControl) {
            meshcore::DiscoverResponse response;
            if (meshcore::parse_discover_response(packet, response) && response.tag == tag_) {
                const int8_t rssi_i8 = clamp_i8(static_cast<int>(std::lround(rssi)));
                const int8_t snr_q4 = clamp_i8(static_cast<int>(std::lround(snr * 4.0f)));
                upsert_node(response.pubkey, response.pubkey_len, response.node_type, rssi_i8, snr_q4,
                            response.their_snr_q4, packet.hops, now_ms);
            }
        } else if (packet.payload_type == meshcore::kPayloadAdvert) {
            meshcore::Advert advert;
            if (meshcore::parse_advert(packet, advert) && advert.has_name) {
                upsert_name(advert.pubkey, advert.node_type, advert.name, now_ms);
            }
        }
    }
}

void DiscoverScanner::transmit_sweep(uint32_t now_ms) {
    uint8_t frame[64] = {};
    const size_t length = meshcore::build_discover_request(kAllNodeTypes, tag_, frame, sizeof(frame));
    if (length == 0) {
        error_ = "could not build discovery request";
        scanning_ = false;
        return;
    }

    (void)radio_.standby();
    if (!radio_.transmit(frame, length)) {
        error_ = radio_.error().empty() ? "discovery transmit failed" : radio_.error();
        scanning_ = false;
        ++revision_;
        return;
    }
    if (!radio_.start_receive()) {
        error_ = radio_.error().empty() ? "could not re-arm receive" : radio_.error();
        scanning_ = false;
        ++revision_;
        return;
    }

    const size_t index = std::min<size_t>(sweep_index_, kSweepIntervalCount - 1);
    next_sweep_ms_ = now_ms + kSweepIntervalsMs[index];
    if (sweep_index_ < 250) {
        ++sweep_index_;
    }
    ++revision_;
}

void DiscoverScanner::upsert_node(const uint8_t* pubkey,
                                  uint8_t pubkey_len,
                                  uint8_t node_type,
                                  int8_t rssi,
                                  int8_t snr_q4,
                                  int8_t their_snr_q4,
                                  uint8_t hops,
                                  uint32_t now_ms) {
    if (!pubkey || pubkey_len == 0) {
        return;
    }

    int slot = -1;
    for (size_t i = 0; i < node_count_; ++i) {
        if (std::memcmp(nodes_[i].pubkey, pubkey, 8) == 0) {
            slot = static_cast<int>(i);
            break;
        }
    }
    if (slot < 0) {
        if (node_count_ < kMaxNodes) {
            slot = static_cast<int>(node_count_++);
        } else {
            uint32_t oldest = UINT32_MAX;
            slot = 0;
            for (size_t i = 0; i < node_count_; ++i) {
                if (nodes_[i].last_ms < oldest) {
                    oldest = nodes_[i].last_ms;
                    slot = static_cast<int>(i);
                }
            }
        }
        std::memset(&nodes_[slot], 0, sizeof(DiscoverNode));
        nodes_[slot].first_ms = now_ms;
        nodes_[slot].pubkey_len = std::min<uint8_t>(pubkey_len, sizeof(nodes_[slot].pubkey));
        std::memcpy(nodes_[slot].pubkey, pubkey, nodes_[slot].pubkey_len);
    }

    DiscoverNode& node = nodes_[slot];
    node.node_type = node_type;
    node.rssi = rssi;
    node.snr_q4 = snr_q4;
    node.their_snr_q4 = their_snr_q4;
    node.hops = hops;
    node.last_ms = now_ms;
    if (node.heard < 0xFFFF) {
        ++node.heard;
    }

    const char* cached = lookup_name(pubkey);
    if (cached) {
        std::strncpy(node.name, cached, sizeof(node.name) - 1);
        node.name[sizeof(node.name) - 1] = '\0';
        node.name_valid = true;
    }
    ++revision_;
}

void DiscoverScanner::upsert_name(const uint8_t* pubkey, uint8_t node_type, const char* name, uint32_t now_ms) {
    if (!pubkey || !name || name[0] == '\0') {
        return;
    }

    int slot = -1;
    for (size_t i = 0; i < name_count_; ++i) {
        if (names_[i].valid && std::memcmp(names_[i].key, pubkey, 8) == 0) {
            slot = static_cast<int>(i);
            break;
        }
    }
    if (slot < 0) {
        if (name_count_ < kMaxNodes) {
            slot = static_cast<int>(name_count_++);
        } else {
            uint32_t oldest = UINT32_MAX;
            slot = 0;
            for (size_t i = 0; i < name_count_; ++i) {
                if (names_[i].last_ms < oldest) {
                    oldest = names_[i].last_ms;
                    slot = static_cast<int>(i);
                }
            }
        }
    }

    NameRecord& record = names_[slot];
    std::memcpy(record.key, pubkey, sizeof(record.key));
    record.node_type = node_type;
    record.last_ms = now_ms;
    record.valid = true;
    std::strncpy(record.name, name, sizeof(record.name) - 1);
    record.name[sizeof(record.name) - 1] = '\0';

    for (size_t i = 0; i < node_count_; ++i) {
        if (std::memcmp(nodes_[i].pubkey, pubkey, 8) == 0) {
            std::strncpy(nodes_[i].name, record.name, sizeof(nodes_[i].name) - 1);
            nodes_[i].name[sizeof(nodes_[i].name) - 1] = '\0';
            nodes_[i].name_valid = true;
            break;
        }
    }
    ++revision_;
}

const char* DiscoverScanner::lookup_name(const uint8_t* pubkey) const {
    for (size_t i = 0; i < name_count_; ++i) {
        if (names_[i].valid && std::memcmp(names_[i].key, pubkey, 8) == 0) {
            return names_[i].name;
        }
    }
    return nullptr;
}

void DiscoverScanner::update_stats() {
    DiscoverStats stats;
    stats.scanning = scanning_;
    stats.simulated = simulated_;
    stats.radio_ready = radio_.ready();
    for (size_t i = 0; i < node_count_; ++i) {
        const uint8_t type = nodes_[i].node_type;
        ++stats.total;
        if (type == meshcore::kAdvTypeRepeater) {
            ++stats.repeaters;
        } else if (type == meshcore::kAdvTypeChat) {
            ++stats.companions;
        } else if (type == meshcore::kAdvTypeRoom) {
            ++stats.rooms;
        } else if (type == meshcore::kAdvTypeSensor) {
            ++stats.sensors;
        }
    }
    stats_ = stats;
}

void DiscoverScanner::simulate(uint32_t now_ms) {
    if (last_sim_ms_ == 0) {
        last_sim_ms_ = now_ms;
        return;
    }
    if (now_ms - last_sim_ms_ < 1000) {
        return;
    }
    last_sim_ms_ = now_ms;
    if (!scanning_) {
        return;
    }

    for (size_t i = 0; i < node_count_; ++i) {
        DiscoverNode& node = nodes_[i];
        const int jitter = static_cast<int>((now_ms / 1000 + i * 7) % 5) - 2;
        node.rssi = clamp_i8(node.rssi + jitter);
        node.snr_q4 = clamp_i8(node.snr_q4 + ((jitter > 0) ? 1 : -1));
        node.last_ms = now_ms;
        if (node.heard < 0xFFFF) {
            ++node.heard;
        }
    }
    ++revision_;
}

} // namespace platform
